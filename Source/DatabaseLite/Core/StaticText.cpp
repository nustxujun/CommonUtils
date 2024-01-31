#include "StaticText.h"
#include "File.h"
#include "BTree.h"
#include "Range.h"


inline int64 GetStringHash(const FString& String)
{
	union
	{
		int64 Number = 0;
		struct
		{
			uint32 HashValue;
			uint32 HashValue2;
		};
	} Hash;


	Hash.HashValue = GetTypeHash(String);
	Hash.HashValue2 = FCrc::MemCrc32(GetData(String), String.Len() * sizeof(TCHAR));
	return Hash.Number;
}

FStaticText::FStaticText(FFileSystem* System):
	FileSystem(System)
{
	const static FString StaticTextFile = TEXT("StaticTextFile");
	if (!(File = FileSystem->OpenFile(StaticTextFile)))
	{
		File = FileSystem->NewFile(StaticTextFile);
		Init();
	}
	else
	{
		Open();
	}
	const static FString StaticTextFileIndex = TEXT("StaticTextFileIndex");

	FFile::Ptr BTreeFile;
	if (!(BTreeFile = FileSystem->OpenFile(StaticTextFileIndex)))
	{
		BTreeFile = FileSystem->NewFile(StaticTextFileIndex);
		BTree = MakeShared<FBTree>(BTreeFile);
		BTree->Init();
	}
	else
	{
		BTree = MakeShared<FBTree>(BTreeFile);
		BTree->Open();
	}
}
FString FStaticText::Get(uint32 Index)
{
	if (File->GetSize() <= Index )
		return {};

	File->SeekRead(Index);
	FString Content;
	File->Read(Content);
	return Content;
}

uint32 FStaticText::Find(const FString& String)
{
	auto HashValue = GetStringHash(String);
	uint32 DataIndex = -1;
	auto Ret = BTree->FindOne(HashValue,[&](auto Data){
		auto Content = Get(Data);
		DataIndex = Data;
		return Content == String;
	});

	return DataIndex;
}

uint32 FStaticText::FindOrCreate(const FString& String)
{
	auto DataIndex = Find(String);
	if (DataIndex != -1)
		return DataIndex;

	auto HashValue = GetStringHash(String);

	DataIndex = File->GetSize();
	File->SeekWrite(DataIndex);
	File->Write(String);

	Header.Count++;
	FlushHeader();
	BTree->Insert(HashValue, DataIndex);

	return DataIndex;
}

void FStaticText::Init()
{
	Header.Count = 0;
	File->Write(Header);
}

void FStaticText::Open()
{
	File->Read(Header);
}

void FStaticText::FlushHeader()
{
	File->SeekWrite(0);
	File->Write(Header);
}

