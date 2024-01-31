#pragma once 

#include "CoreMinimal.h"
#include "LowLevelFile.h"

using PageId = uint32;
constexpr static uint32 FILE_PAGE_SIZE = 16 * 1024;
constexpr static uint32 PAGE_ID_STRIDE = sizeof(PageId);
constexpr static uint32 MAX_PAGE_COUNT = ~((PageId)0);
constexpr static uint32 MAX_DB_FILE_SIZE = FILE_PAGE_SIZE * MAX_PAGE_COUNT;
constexpr static uint32 SINGLE_FILE_INDEX_PAGE_COUNT = 8;
constexpr static PageId PAGE_ID_INVALID = ~((PageId)0);


class FFileSystem;
class FFile
{
	friend class FFileSystem;
public:
	using Ptr = TSharedPtr<FFile>;
	using VirtualPos = uint32;
	using RealPos = uint32;
	using PagePos = TPair<PageId, uint32>;
public:
	FFile(FFileSystem* FileSys);
	~FFile();
	bool Open(PageId Id);
	void Init(PageId Id);
	void Delete();

	void SeekRead(VirtualPos Pos);
	void SeekWrite(VirtualPos Pos);
	VirtualPos GetSize();

	VirtualPos TellRead()const;
	VirtualPos TellWrite()const;

	void SkipRead(VirtualPos Size);
	void SkipWrite(VirtualPos Size);

	bool Write(const void* Data, uint32 Size);
	bool Read(void* Buffer, uint32 Size);

	bool Write(const FString& String);
	bool Read(FString& String);

	bool WriteStaticString(const FString& String);
	bool ReadStaticString(FString& String);

	template<class T>
	bool Write(const T& Value)
	{
		return Write(&Value, sizeof(Value));
	}

	template<class T>
	bool Read(T& Value)
	{
		return Read(&Value, sizeof(Value));
	}

	FFileSystem* GetFileSystem(){return System;}
	PageId GetId(){return FileHeader.IndexPages[0];};
	PageId AppendPage();

private:
	RealPos GetRealPos(VirtualPos Pos);
	VirtualPos GetDataEnd();
	void FlushHeader();
private:

	FFileSystem* System;
	TArray<PageId> Pages;

	struct FFileHeader
	{
		int32 MagicNum;
		VirtualPos DataEnd;
		RealPos IndexEnd;
		PageId DataPageCount;
		PageId IndexPages[SINGLE_FILE_INDEX_PAGE_COUNT];
	}FileHeader;

	VirtualPos ReadPos = {};
	VirtualPos WritePos = {};

};


class FFileSystem
{
public:
	friend class FFile;
public:
	FFileSystem();
	~FFileSystem();

	bool Init(const FString& FileName, bool bReadOnly , ELowLevelFileType Type);

	FFile::Ptr OpenFile(PageId Id);
	FFile::Ptr OpenFile(const FString& Name);
	FFile::Ptr NewFile();
	FFile::Ptr NewFile(const FString& Name);
	class FStaticText& GetStaticText(){return *StaticText;}
	bool IsReadOnly()const {return !WriteHandle;}

private:
	void FlushHeader();
	PageId NewPage();
	void RecyclePage(PageId Id);
private:

	struct 
	{
		uint32 NamedFileCount = 0;
		uint32 PageCount = 0;
		PageId FreeList = PAGE_ID_INVALID;
		
	}Header;

	FFile::Ptr HeadFile;
	TMap<PageId, TWeakPtr<FFile>> Files;
	TMap<FString, PageId> NamedFiles;
	TSharedPtr<ILowLevelFile> ReadHandle;
	TSharedPtr<ILowLevelFile> WriteHandle;

	TSharedPtr<class FStaticText> StaticText;
};

#if UE_BUILD_DEVELOPMENT
#define CHECK_RESULT(x) check(x)
#else
#define CHECK_RESULT(x) x
#endif