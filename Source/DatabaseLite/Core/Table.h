#pragma once 

#include "Index.h"
#include "File.h"

class DATABASELITE_API FDBTable
{
public:
	using RowData = TArray<uint8>;
	using RowArray = TArray<RowData>;
public:
	FDBTable(FFile::Ptr InFile);

	void Init(const TMap<FString, FKeyTypeSequence>& IndexKeyTypes);
	void Open();
	void Delete();

	TArray<RowData> GetRows();
	RowArray Find(const FString& KeyName, const FKeySequence& Key);
	bool FindOne(const FString& KeyName, const FKeySequence& Key, const TFunction<void*(int)>& Buffer);
	bool FindOne(const FString& KeyName, const FKeySequence& Key, const TFunction<bool(const void*, int)>& Buffer);
	bool FindOne(const FString& KeyName, const FKeySequence& Key, FString& Str);

	template<class T>
	bool FindOne(const FString& KeyName, const FKeySequence& Key, T& Value)
	{
		return FindOne(KeyName, Key, [&](int Size)->void*{
			if (Size != sizeof (T))
				return nullptr;
			return &Value;
		});
	}

	bool AddRow(const TMap<FString, FKeySequence>& Keys, const void* Buffer, int Size, bool bUnique);
	bool AddRow(const TMap<FString, FKeySequence>& Keys,const FString& Val, bool bUnique);

	template<class T>
	bool AddRow(const TMap<FString, FKeySequence>& Keys, const T& Value, bool bUnique )
	{
		return AddRow(Keys, &Value, sizeof(Value), bUnique);
	}
	bool UpdateRow(const FString& KeyName, const FKeySequence& Key, const void* Buffer, int Size);
	template<class T>
	bool UpdateRow(const FString& KeyName, const FKeySequence& Key, const T& Value)
	{
		return UpdateRow(KeyName, Key, &Value, sizeof(Value));
	}


	bool RemoveRow(const FString& KeyName, const FKeySequence& Key);
private:
	FKeySequence ReadRowKey(uint32 DataIndex,int Offset, const FKeyTypeSequence& Types);
	bool ReadRowData(uint32 DataIndex, RowData& Data);
	bool ReadRowData(uint32 DataIndex, const TFunction<void* (int)>& Buffer);
	bool MoveRowData(uint32 DstDataIndex, uint32 SrcDataIndex);
	bool Equal(uint32 DataIndex,int Offset, const FKeySequence& Keys, const FKeyTypeSequence& Types);

	uint32 WriteRow(const TMap<FString, FKeySequence>& Keys, const void* Buffer, int Size);
	uint32 WriteRow(const TMap<FString, FKeySequence>& Keys, uint32 Data);
	void WriteRow(const TMap<FString, FKeySequence>& Keys,uint32 DataIndex, uint32 Data);

	void UpdateRow(uint32 DataIndex, const void* Buffer, int Size);
	uint32 WriteData(const void*Buffer, int Size);

	bool IsRowValid(uint32 DataIndex);

	void FlushHeader();

	int64 ConverToNumber(const FKeySequence& Key, const FKeyTypeSequence& Types, bool bRefresh);
private:
	FFileSystem* FileSystem;
	FFile::Ptr File;
	FFile::Ptr DataFile;


	struct FIndex
	{
		FBaseIndex::Ptr Index;
		FFile::Ptr File;
		FKeyTypeSequence KeyTypes;
		int KeyOffset;
	};

	TMap<FString, FIndex> Indices;


	struct
	{
		int32 MagicNum = 0;
		int32 NumIndices = 0;
		uint32 DataBegin = 0;
		uint32 DataEnd = 0;
		int32 NumRows = 0;
		int32 RowDataOffset = 0;
		PageId DataFileId;
		uint32 FreeList ;
	}Header;
};


