#pragma once 

#include "CoreMinimal.h"
#include "LRUCache.h"

enum class ELowLevelFileType
{
	Normal,
	Memory,
	Cached,
};

class ILowLevelFile
{
public:
	using Ptr = TSharedPtr<ILowLevelFile>;
public:
	virtual ~ILowLevelFile(){}
	virtual bool Write(const uint8* Buffer, uint32 Size) = 0;
	virtual bool Read(uint8* Buffer, uint32 Size ) = 0;
	virtual uint32 Tell() = 0;
	virtual bool Seek(uint32 Pos) = 0;
	virtual bool IsValid() = 0;
};

class FGenericPlatformFile: public ILowLevelFile
{
public:
	static ILowLevelFile::Ptr OpenRead(const FString& FileName);
	static ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead);

	virtual bool Write(const uint8* Buffer, uint32 Size) override
	{
		return FileHandle->Write(Buffer, Size);
	}
	virtual bool Read(uint8* Buffer, uint32 Size)override
	{
		return FileHandle->Read(Buffer, Size);
	}
	virtual uint32 Tell()override
	{
		return FileHandle->Tell();
	}
	virtual bool Seek(uint32 Pos)override
	{
		return FileHandle->Seek(Pos);
	}

	FGenericPlatformFile(TSharedPtr<IFileHandle> InFile): FileHandle(InFile){}

	virtual bool IsValid() override {return FileHandle.IsValid();}
private:
	TSharedPtr<IFileHandle> FileHandle;


};


class FCachedFile : public ILowLevelFile
{
	static const int SINGLE_CACHE_SIZE = 1024 * 16;
	static const int TOTAL_CACHE_SIZE = 2 * 1024 * 1024;
public:
	static ILowLevelFile::Ptr OpenRead(const FString& FileName);
	static ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead);
public:
	FCachedFile();
	~FCachedFile();

	virtual bool Write(const uint8* Buffer, uint32 Size)override;
	virtual bool Read(uint8* Buffer, uint32 Size) override;
	virtual uint32 Tell() override;
	virtual bool Seek(uint32 Pos) override;
	FCachedFile(TSharedPtr<IFileHandle> InFile) : FileHandle(InFile) {}
	virtual bool IsValid() override { return FileHandle.IsValid(); }
private:
	TSharedPtr<IFileHandle> FileHandle;
	TFlatLRUCache<uint32, TArray<uint8>, TOTAL_CACHE_SIZE / SINGLE_CACHE_SIZE> PageCaches;
};

class FMemoryFile : public ILowLevelFile
{
	static const int MEMORY_PAGE_SIZE = 1024 * 1024;
public:
	static ILowLevelFile::Ptr OpenRead(const FString& FileName);
	static ILowLevelFile::Ptr OpenWrite(const FString& FileName, bool bAppend, bool bAllowRead);
public:
	FMemoryFile();
	~FMemoryFile();

	virtual bool Write(const uint8* Buffer, uint32 Size)override;
	virtual bool Read(uint8* Buffer, uint32 Size) override;
	virtual uint32 Tell() override;
	virtual bool Seek(uint32 Pos) override;
	virtual bool IsValid() override { return true; }

private:
	void AppendPage();
private:
	TArray<uint8*> Pages;
	uint32 TotalSize;
	uint32 Pos;
};