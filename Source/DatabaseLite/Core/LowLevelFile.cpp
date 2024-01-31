#include "LowLevelFile.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

ILowLevelFile::Ptr FGenericPlatformFile::OpenRead(const FString& FileName)
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return ILowLevelFile::Ptr(new  FGenericPlatformFile{ MakeShareable(PlatformFile.OpenRead(*FileName)) });
}

ILowLevelFile::Ptr FGenericPlatformFile::OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead)
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return ILowLevelFile::Ptr(new  FGenericPlatformFile{ MakeShareable(PlatformFile.OpenWrite(*FileName, bAppend, bAllowRead)) });

}



ILowLevelFile::Ptr FCachedFile::OpenRead(const FString& FileName)
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return ILowLevelFile::Ptr(new  FCachedFile{ MakeShareable(PlatformFile.OpenRead(*FileName)) });
}

ILowLevelFile::Ptr FCachedFile::OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead)
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return ILowLevelFile::Ptr(new  FCachedFile{ MakeShareable(PlatformFile.OpenWrite(*FileName, bAppend, bAllowRead)) });

}



FCachedFile::FCachedFile()
{

}

FCachedFile::~FCachedFile()
{

}

bool FCachedFile::Write(const uint8* Buffer, uint32 Size)
{
	auto Pos = Tell();
	auto Index = Pos / SINGLE_CACHE_SIZE;
	auto Offset = Pos % SINGLE_CACHE_SIZE;
	auto Space = SINGLE_CACHE_SIZE - Offset;
	if (Space < Size)
	{
		if (!Write(Buffer, Space))
		{ 
			return false;
		}

		return Write(Buffer + Space, Size - Space);
	}
	if (auto Cache = PageCaches.Get(Index + 1))
	{
		FMemory::Memcpy(Cache->GetData() + Offset, Buffer, Size);
	}
	return FileHandle->Write(Buffer, Size);
}

bool FCachedFile::Read(uint8* Buffer, uint32 Size)
{
	const auto Pos = Tell();
	auto Index = Pos / SINGLE_CACHE_SIZE;
	auto Offset = Pos % SINGLE_CACHE_SIZE;
	auto Space = SINGLE_CACHE_SIZE - Offset;
	if (Space < Size)
	{
		if (!Read(Buffer, Space))
			return false;

		return Read(Buffer + Space, Size - Space);
	}

	TArray<uint8>* Cache = PageCaches.Get(Index + 1);
	if (Cache)
	{
		FMemory::Memcpy(Buffer, Cache->GetData() + Offset, Size);
		PageCaches.Refer(Index + 1);
		FileHandle->Seek(Pos + Size);
		return true;
	}
	else if (FileHandle->Size() >= (Index + 1) * SINGLE_CACHE_SIZE)
	{

		Cache = PageCaches.Push(Index + 1);
		if (Cache)
		{
			Cache->SetNumUninitialized(SINGLE_CACHE_SIZE);
			FileHandle->Seek(Index * SINGLE_CACHE_SIZE);
			if (!FileHandle->Read(Cache->GetData(), SINGLE_CACHE_SIZE))
			{
				return false;
			}
			FMemory::Memcpy(Buffer, Cache->GetData() + Offset, Size);
			return FileHandle->Seek(Pos + Size);
		}
	}
	return FileHandle->Read(Buffer, Size);
}

uint32 FCachedFile::Tell()
{
	return FileHandle->Tell();
}

bool FCachedFile::Seek(uint32 Pos)
{
	return FileHandle->Seek(Pos);
}





ILowLevelFile::Ptr FMemoryFile::OpenRead(const FString& FileName)
{
	return ILowLevelFile::Ptr(new FMemoryFile());
}

ILowLevelFile::Ptr FMemoryFile::OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead)
{
	return ILowLevelFile::Ptr(new FMemoryFile());
}

FMemoryFile::FMemoryFile()
{
	AppendPage();
}


FMemoryFile::~FMemoryFile()
{
	for (auto Page : Pages)
	{
		FMemory::Free(Page);
	}
}

bool FMemoryFile::Write(const uint8* Buffer, uint32 Size)
{
	check(Size <= MEMORY_PAGE_SIZE);
	auto Index = Pos / MEMORY_PAGE_SIZE;
	auto Offset = Pos % MEMORY_PAGE_SIZE;
	auto Space = MEMORY_PAGE_SIZE - Offset;

	if (Space < Size)
	{
		if (!Write(Buffer, Space))
			return false;
		
		return Write(Buffer + Space, Size - Space);
		
	}
	else if (Space == Size && Index == Pages.Num() - 1)
	{
		AppendPage();
	}

	Pos += Size;
	FMemory::Memcpy(Pages[Index] + Offset, Buffer, Size);
	return true;
}

bool FMemoryFile::Read(uint8* Buffer, uint32 Size)
{
	check(Size <= MEMORY_PAGE_SIZE);
	auto Index = Pos / MEMORY_PAGE_SIZE;
	if (Index >= (uint32)Pages.Num())
		return false;

	auto Offset = Pos % MEMORY_PAGE_SIZE;
	auto Space = MEMORY_PAGE_SIZE - Offset;
	if (Space < Size)
	{
		if (!Read(Buffer, Space))
			return false;

		return Read(Buffer + Space, Size - Space);
	}
	Pos += Size;
	FMemory::Memcpy(Buffer, Pages[Index] + Offset, Size);
	return true;

}

uint32 FMemoryFile::Tell()
{
	return Pos;
}

bool FMemoryFile::Seek(uint32 InPos)
{
	if (InPos / MEMORY_PAGE_SIZE >= (uint32)Pages.Num())
		return false;
	Pos = InPos;
	return true;
}

void FMemoryFile::AppendPage()
{
	Pages.Add((uint8*)FMemory::Malloc(MEMORY_PAGE_SIZE));
}
