#include "File.h"
#include "StaticText.h"
#include "Range.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"


struct FGuardWrite
{
	static bool Locked;
	FGuardWrite()
	{
		check(!Locked);
		Locked = true;
	}
	~FGuardWrite()
	{
		Locked = false;
	}
};
bool FGuardWrite::Locked = false;
#define GUARD_WRITE() FGuardWrite __GuardWrite;



inline uint32 GetPageOffset(PageId Id)
{
	return Id * FILE_PAGE_SIZE;
}



struct FFileHandleHelper
{
	static bool Read(void* Buffer, uint32 Size, TSharedPtr<ILowLevelFile> InHandle, PageId Id, uint32 Offset = 0)
	{
		auto Begin = GetPageOffset(Id) + Offset;
		auto End = Begin + FILE_PAGE_SIZE;
		check(Offset < FILE_PAGE_SIZE && Size < FILE_PAGE_SIZE);
		check(Begin + Size < End);
		InHandle->Seek(Begin + Offset);
		return InHandle->Read((uint8*)Buffer, Size);
	}

	static bool Write(const void* Buffer, uint32 Size, TSharedPtr<ILowLevelFile> InHandle, PageId Id, uint32 Offset = 0)
	{
		GUARD_WRITE();

		auto Begin = GetPageOffset(Id);
		auto End = Begin + FILE_PAGE_SIZE;
		check(Begin + Offset + Size < End);
		InHandle->Seek(Begin + Offset);
		return InHandle->Write((const uint8*)Buffer, Size);
	}


	template<class T>
	static bool Read(T& Value, TSharedPtr<ILowLevelFile> InHandle, PageId Id, uint32 Offset = 0)
	{
		return Read((void*) & Value, sizeof(Value), InHandle, Id, Offset);
	}

	template<class T>
	static bool Write(const T& Value, TSharedPtr<ILowLevelFile> InHandle, PageId Id, uint32 Offset = 0)
	{
		return Write((const void*) & Value, sizeof(Value), InHandle, Id, Offset);
	}
};


FFileSystem::FFileSystem()
{

}

FFileSystem::~FFileSystem()
{
	StaticText.Reset();
	if (WriteHandle)
		FlushHeader();
	HeadFile.Reset();
	for (auto& File : Files)
	{
		checkf(!File.Value.IsValid(), TEXT("File is not closed before closing filesystem."));
	}
}

struct LowLevelFileFactory
{
	struct Wrapper
	{
		using Ptr = TSharedPtr<Wrapper>;
		virtual ~Wrapper(){};
		virtual ILowLevelFile::Ptr OpenRead(const FString& FileName) = 0;
		virtual ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead) = 0;
	} ;

	template<class T>
	struct TWrapper : public Wrapper
	{
		ILowLevelFile::Ptr OpenRead(const FString& FileName)
		{
			return T::OpenRead(FileName);
		}
		ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead)
		{
			return T::OpenWrite(FileName, bAppend, bAllowRead);
		}
	} ;

	Wrapper::Ptr Instance;

	template<class T>
	static LowLevelFileFactory GetFactory()
	{
		LowLevelFileFactory Factory;
		Factory.Instance = Wrapper::Ptr(new TWrapper<T>());
		return Factory;
	}

	ILowLevelFile::Ptr OpenRead(const FString& FileName)
	{
		return Instance->OpenRead(FileName);
	}
	ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead)
	{
		return Instance->OpenWrite(FileName, bAppend, bAllowRead);
	}
};


static LowLevelFileFactory GetFactory(ELowLevelFileType Type)
{
	switch (Type)
	{
	case ELowLevelFileType::Normal:	return LowLevelFileFactory::GetFactory<FGenericPlatformFile>();
	case ELowLevelFileType::Memory:	return LowLevelFileFactory::GetFactory<FMemoryFile>();
	case ELowLevelFileType::Cached: return LowLevelFileFactory::GetFactory<FCachedFile>();
	}

	return {};
}


bool FFileSystem::Init(const FString& FileName, bool bReadOnly, ELowLevelFileType Type)
{
	bool bIsNewFile = !FPaths::FileExists(FileName);
	auto Factory = GetFactory(Type);
	if (!bReadOnly)
		ReadHandle = WriteHandle = (Factory.OpenWrite(*FileName, true, true));
	else if (bIsNewFile)
		return false;
	else
		ReadHandle = (Factory.OpenRead(*FileName));

	if (!ReadHandle || !ReadHandle->IsValid())
		return false;

	if (bIsNewFile || !(HeadFile = OpenFile(0)))
	{
		if (bReadOnly)
			return false;

		ReadHandle.Reset();
		WriteHandle.Reset();
		ReadHandle = WriteHandle = Factory.OpenWrite(*FileName, false, true);
		HeadFile = MakeShared<FFile>(this);
		HeadFile->Init(NewPage());
		//FFileHandleHelper::Write(Header,WriteHandle, 0);

		FlushHeader();

	}
	else
	{
		HeadFile->Read(Header);

		for (auto Index : XRange(Header.NamedFileCount))
		{
			FString Name;
			PageId Id;
			HeadFile->Read(Name);
			HeadFile->Read(Id);
			NamedFiles.Add(Name, Id);
		}
	}

	StaticText = MakeShared<FStaticText>(this);

	return true;
}

FFile::Ptr FFileSystem::OpenFile(PageId Id)
{
	auto File = Files.FindRef(Id);
	if (File.IsValid())
	{
		File.Pin()->ReadPos = 0;
		File.Pin()->WritePos = 0;
		return File.Pin();
	}
	auto SharedFile = MakeShared<FFile>(this);
	if (!SharedFile->Open(Id))
		return {};

	Files.Add(Id, File);
	return SharedFile;
}

FFile::Ptr FFileSystem::OpenFile(const FString& Name)
{
	auto Id = NamedFiles.FindRef(Name);
	if (Id == 0)
		return {};

	return OpenFile(Id);
}

FFile::Ptr FFileSystem::NewFile()
{
	PageId Id = NewPage();

	FFile::Ptr File = MakeShared<FFile>(this);
	File->Init(Id);

	FlushHeader();
	Files.Add(Id, File);
	return File;
}

FFile::Ptr FFileSystem::NewFile(const FString& Name)
{
	check(!NamedFiles.Find(Name));

	auto File = NewFile();
	NamedFiles.Add(Name, File->GetId());
	Header.NamedFileCount += 1;

	HeadFile->SeekWrite(HeadFile->GetSize());
	HeadFile->Write(Name);
	HeadFile->Write(File->GetId());

	FlushHeader();
	return File;
}

void FFileSystem::FlushHeader()
{
	HeadFile->SeekWrite(0);
	HeadFile->Write(Header);
}

PageId FFileSystem::NewPage()
{
	PageId NewId;
	if (Header.FreeList != PAGE_ID_INVALID)
	{
		PageId Next;
		FFileHandleHelper::Read(Next, ReadHandle, Header.FreeList);
		NewId = Header.FreeList;
		Header.FreeList = Next;
	}
	else
	{
		NewId = Header.PageCount++;
	}
	

	GUARD_WRITE();

	WriteHandle->Seek(GetPageOffset(NewId));
	uint32 Zero[1024] = {};
	FMemory::Memset(Zero, 0xcd, sizeof(Zero));
	for (uint32 i = 0; i < FILE_PAGE_SIZE / sizeof(Zero); ++i)
	{
		WriteHandle->Write((const uint8*)Zero, sizeof(Zero));
	}
	return NewId;
}

void FFileSystem::RecyclePage(PageId Id)
{
	if (Id == PAGE_ID_INVALID)
		return ;

	FFileHandleHelper::Write(Header.FreeList, ReadHandle, Id);
	Header.FreeList = Id;
}


FFile::FFile(FFileSystem* FileSys):
	System(FileSys)
{

}

FFile::~FFile()
{
	FlushHeader();
}

constexpr int32 FILE_MAGIC_NUM = 0xF11e;

bool FFile::Open(PageId BeginId)
{
	FFileHandleHelper::Read(FileHeader, System->ReadHandle, BeginId);
	//ensure(FileHeader.MagicNum == FILE_MAGIC_NUM);
	if (FileHeader.MagicNum != FILE_MAGIC_NUM)
	{
		return false;
	}

	check(FileHeader.IndexPages[0] == BeginId);
	Pages.Reserve(FileHeader.DataPageCount);

	int Index = 0;
	auto IndexPage = FileHeader.IndexPages[Index];
	auto Beg = GetPageOffset(IndexPage) + sizeof(FileHeader);
	auto End = GetPageOffset(IndexPage + 1);
	for(auto DataIndex: XRange(FileHeader.DataPageCount))
	{
		PageId Id;
		System->ReadHandle->Read((uint8*) & Id, sizeof(Id));
		Pages.Add(Id);
		Beg += sizeof(Id);

		if (Beg >= End)
		{
			check(Index < SINGLE_FILE_INDEX_PAGE_COUNT)
			IndexPage = FileHeader.IndexPages[++Index];
			check( IndexPage != PAGE_ID_INVALID )
			Beg = GetPageOffset(IndexPage);
			End = GetPageOffset(IndexPage + 1);
			System->ReadHandle->Seek(Beg);
		}
	}
	return true;
}

void FFile::Init(PageId BeginId)
{
	FileHeader.MagicNum = FILE_MAGIC_NUM;
	FileHeader.DataPageCount = 0;
	FMemory::Memset(FileHeader.IndexPages, 0xff, sizeof(FileHeader.IndexPages));
	FileHeader.IndexPages[0] = BeginId;
	FileHeader.DataEnd = 0;
	FileHeader.IndexEnd = GetPageOffset(BeginId) + sizeof(FileHeader) ;
	FFileHandleHelper::Write(FileHeader,System->WriteHandle, BeginId);
	System->WriteHandle->Write((const uint8*)&PAGE_ID_INVALID, sizeof(PAGE_ID_INVALID));

	AppendPage();
}

void FFile::Delete()
{
	FileHeader.MagicNum = 0xdeaddead;
	FlushHeader();

	for (auto Id : FileHeader.IndexPages)
	{
		if (Id != PAGE_ID_INVALID)
			System->RecyclePage(Id);

		break;
	}

	for (auto Id : Pages)
	{
		System->RecyclePage(Id);
	}

	System = nullptr;

}

void FFile::SeekRead(VirtualPos Pos)
{
	check(Pos <= GetDataEnd())
	ReadPos = Pos;
	System->ReadHandle->Seek(GetRealPos(Pos));
}

void FFile::SeekWrite(VirtualPos Pos)
{
	check(Pos <= GetDataEnd())
	WritePos = Pos;
	System->WriteHandle->Seek(GetRealPos(Pos));
}

FFile::VirtualPos FFile::TellRead() const
{
	return ReadPos;
}

FFile::VirtualPos FFile::TellWrite() const
{
	return WritePos;
}

FFile::VirtualPos FFile::GetSize()
{
	return FileHeader.DataEnd;
}

void FFile::SkipRead(VirtualPos Size)
{
	SeekRead(ReadPos + Size);
}

void FFile::SkipWrite(VirtualPos Size)
{
	SeekWrite(WritePos + Size);
}

bool FFile::Write(const void* Data, uint32 Size)
{
	auto Index = (WritePos / FILE_PAGE_SIZE);
	auto Offset = WritePos % FILE_PAGE_SIZE;
	auto Space = FILE_PAGE_SIZE - Offset;
	if (Space < Size)
	{
		auto Diff = Size - Space;
		if (!Write(Data, Space))
			return false;

		return Write((const uint8*)Data + Space, Diff);
	}
	else if (Space == Size && Index == Pages.Num() - 1)
	{
		AppendPage();
	}
	else if (Index == Pages.Num() )
	{
		check(Offset == 0);
		AppendPage();
	}
	else 
	{
		check(Index < (uint32)Pages.Num())
	}
	WritePos += Size;

	FileHeader.DataEnd = FMath::Max(WritePos, FileHeader.DataEnd);
	System->WriteHandle->Seek(GetPageOffset(Pages[Index]) + Offset);
	return System->WriteHandle->Write((const uint8*)Data, Size);
}

bool FFile::Read(void* Data, uint32 Size)
{
	auto Index = (ReadPos / FILE_PAGE_SIZE);
	if (Index >= (uint32)Pages.Num())
		return false;
	auto Offset = ReadPos % FILE_PAGE_SIZE;
	auto Space = FILE_PAGE_SIZE - Offset;
	if (Space < Size)
	{
		//Index += 1;
		//Offset = 0;
		//check(Index < (uint32)Pages.Num());
		//ReadPos = Index * FILE_PAGE_SIZE;

		auto Diff = Size - Space;
		if (!Read(Data, Space))
			return false;

		return Read((uint8*) Data + Space, Diff);
	}
	ReadPos += Size;
	System->ReadHandle->Seek(GetPageOffset(Pages[Index]) + Offset);
	return System->ReadHandle->Read((uint8*)Data, Size);
}

bool FFile::Write(const FString& String)
{
	int32 Num = String.Len() + 1;
	if (!Write(Num))
		return false;
	return Write((const uint8*)String.GetCharArray().GetData(), Num * sizeof(TCHAR));
}

bool FFile::Read(FString& String)
{
	int32 Num = 0;
	if (!Read(Num))
		return false;

	String.GetCharArray().SetNum(Num);
	return Read((uint8*) String.GetCharArray().GetData(), Num * sizeof(TCHAR));
}

bool FFile::WriteStaticString(const FString& String)
{
	auto Index = System->StaticText->FindOrCreate(String);
	return Write(Index);
}

bool FFile::ReadStaticString(FString& String)
{
	uint32 Index = 0;
	if (!Read(Index))
		return false;

	String = System->StaticText->Get(Index);
	return true;
}

PageId FFile::AppendPage()
{
	auto Id = System->NewPage();
	Pages.Add(Id);
	FileHeader.DataPageCount++;
	

	System->WriteHandle->Seek(FileHeader.IndexEnd);
	System->WriteHandle->Write((const uint8*)&Id, sizeof(Id));

	if ((FileHeader.IndexEnd + sizeof(Id)) / FILE_PAGE_SIZE != FileHeader.IndexEnd / FILE_PAGE_SIZE)
	{
		check((FileHeader.IndexEnd + sizeof(Id)) % FILE_PAGE_SIZE == 0);
		auto Index = 0;
		for (; Index < SINGLE_FILE_INDEX_PAGE_COUNT; Index++)
		{
			if (FileHeader.IndexPages[Index] == PAGE_ID_INVALID)
				break;
		}

		checkf(Index > 0 && Index < SINGLE_FILE_INDEX_PAGE_COUNT, TEXT("single table is limit to %d bytes"), FILE_PAGE_SIZE / sizeof(PageId) * SINGLE_FILE_INDEX_PAGE_COUNT * FILE_PAGE_SIZE);
		auto IndexPage = System->NewPage();
		FileHeader.IndexPages[Index] = IndexPage;
		FileHeader.IndexEnd = GetPageOffset(IndexPage);
	}
	else
	{
		FileHeader.IndexEnd += sizeof(Id);
	}

	System->WriteHandle->Seek(FileHeader.IndexEnd);
	System->WriteHandle->Write((const uint8*)&PAGE_ID_INVALID, sizeof(PAGE_ID_INVALID));


	FlushHeader();

	return Id;
}


FFile::RealPos FFile::GetRealPos(VirtualPos Pos)
{
	auto Index = (Pos / FILE_PAGE_SIZE);
	check(Index < (uint32)Pages.Num());
	auto Offset = Pos % FILE_PAGE_SIZE;
	return GetPageOffset(Pages[Index]) + Offset;
}

FFile::VirtualPos FFile::GetDataEnd()
{
	return Pages.Num() * FILE_PAGE_SIZE;
}


void FFile::FlushHeader()
{
	if (System && System->WriteHandle)
		FFileHandleHelper::Write(FileHeader, System->WriteHandle, FileHeader.IndexPages[0]);
}

