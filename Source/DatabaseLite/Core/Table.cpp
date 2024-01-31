#include "Table.h"
#include "Range.h"
#include "StaticText.h"


constexpr int32 TABLE_MAGIC_NUM = 0xFDB7ab1e;

constexpr uint32 INVALID_DATA_INDEX = -1;

#define THREAD_SAFTY 0

FDBTable::FDBTable(FFile::Ptr InFile):
	File(InFile)
{
	FileSystem = File->GetFileSystem();


}

void FDBTable::Init(const TMap<FString, FKeyTypeSequence>& IndexKeyTypes)
{ 

	Header.MagicNum = TABLE_MAGIC_NUM;
	Header.NumRows = 0;
	Header.FreeList = INVALID_DATA_INDEX;

	int KeyOffset = 0;
	for (auto& KeyItem: IndexKeyTypes)
	{
		FIndex DBIndex;
		DBIndex.File = FileSystem->NewFile();
		auto SeachIndex = new TIndex<FBTree>(DBIndex.File);
		SeachIndex->Init();
		DBIndex.Index = FBaseIndex::Ptr(SeachIndex);
		DBIndex.KeyTypes = KeyItem.Value;
		DBIndex.KeyOffset = KeyOffset;
		KeyOffset += FIndexHelper::GetKeySize(KeyItem.Value);
		Indices.Add(KeyItem.Key, DBIndex);
	}

	Header.RowDataOffset = KeyOffset;


	File->SeekWrite(sizeof(Header));
	for (auto& Item : Indices)
	{
		File->Write(Item.Key);

		auto& DBIndex = Item.Value;
		File->Write(DBIndex.File->GetId());
		File->Write((int)DBIndex.KeyTypes.Num());
		for (auto Type : DBIndex.KeyTypes)
		{
			File->Write(Type);
		}
		File->Write(DBIndex.KeyOffset);
	}

	Header.DataBegin = Header.DataEnd = File->TellWrite();
	Header.NumIndices = Indices.Num();
	DataFile = FileSystem->NewFile();
	Header.DataFileId = DataFile->GetId();
	FlushHeader();

}

void FDBTable::Open()
{
	File->Read(Header);
	check(TABLE_MAGIC_NUM == Header.MagicNum);
	for (auto Index = 0; Index < Header.NumIndices; ++Index)
	{
		FIndex DBIndex;

		FString Name;
		PageId Id;
		File->Read(Name);
		File->Read(Id);

		int NumKeys;
		File->Read(NumKeys);

		for (auto KeyIndex : XRange(NumKeys))
		{
			EKeyType Type;
			CHECK_RESULT(File->Read(Type));
			DBIndex.KeyTypes.Add(Type);
		}

		File->Read(DBIndex.KeyOffset);

		DBIndex.File = FileSystem->OpenFile(Id);

		auto SeachIndex = new TIndex<FBTree>(DBIndex.File);
		SeachIndex->Open();
		DBIndex.Index = FBaseIndex::Ptr(SeachIndex);

		Indices.Add(Name, DBIndex);
	}

	DataFile = FileSystem->OpenFile(Header.DataFileId);
	check(DataFile);
}

void FDBTable::Delete()
{
	Header.MagicNum = 0xDeadDead;
	FlushHeader();

	File->Delete();
	File.Reset();
	DataFile->Delete();
	DataFile.Reset();
	for (auto& Item : Indices)
	{
		Item.Value.File->Delete();
	}
	Indices.Reset();
}

TArray<FDBTable::RowData> FDBTable::GetRows()
{
	TArray<RowData> Result;
	Result.Reserve(Header.NumRows);
	File->SeekRead(Header.DataBegin);
	int Index = 0;

	while (Index < Header.NumRows)
	{
		RowData Data;
		File->SkipRead(Header.RowDataOffset);
		uint32 DataPointer;
		CHECK_RESULT(File->Read(DataPointer));
		if (DataPointer == INVALID_DATA_INDEX)
			continue;

		Index+=1;
		DataFile->SeekRead(DataPointer);
		int Size;
		CHECK_RESULT(DataFile->Read(Size));
		Data.SetNumUninitialized(Size);
		CHECK_RESULT(DataFile->Read(Data.GetData(), Size));

		Result.Add(MoveTemp(Data));
	}

	return Result;
}

FDBTable::RowArray FDBTable::Find(const FString& KeyName, const FKeySequence& Key)
{
	auto Index = Indices.Find(KeyName);
	check(Index);
	auto DataIndices = Index->Index->Find(ConverToNumber(Key, Index->KeyTypes, false));

	RowArray Result;
	for (auto& DataIndex : DataIndices)
	{
		if (!Equal(DataIndex, Index->KeyOffset,Key, Index->KeyTypes))
			continue;

		RowData Data;
		if (ReadRowData(DataIndex, Data))
			Result.Add(Data);
	}
	return Result;
}

bool FDBTable::FindOne(const FString& KeyName, const FKeySequence& Key,const TFunction<void*(int)>& Buffer)
{
	auto Index = Indices.Find(KeyName);
	check(Index);
	if (!Index->Index->FindOne(ConverToNumber(Key, Index->KeyTypes, false),[&](uint32 Data){
			if (!Equal(Data, Index->KeyOffset,Key, Index->KeyTypes))
				return false;

			return ReadRowData(Data,Buffer);
		}))
	{
		return false;
	}

	return true;

}

bool FDBTable::FindOne(const FString& KeyName, const FKeySequence& Key, const TFunction<bool(const void*, int)>& Buffer)
{
	auto Index = Indices.Find(KeyName);
	check(Index);
	if (!Index->Index->FindOne(ConverToNumber(Key, Index->KeyTypes, false), [&](uint32 DataIndex)
		{
			if (!Equal(DataIndex, Index->KeyOffset, Key, Index->KeyTypes))
				return false;

			TArray<uint8> Data;
			if (!ReadRowData(DataIndex, Data))
				return false;

			return Buffer(Data.GetData(), Data.Num());
		}))
	{
		return false;
	}

	return true;

}


bool FDBTable::FindOne(const FString& KeyName, const FKeySequence& Key, FString& Str)
{
	return FindOne(KeyName, Key, [&](const void* Buffer, int Size)->bool {
		auto Begin = (const uint8*) Buffer;

		int Num = 0;
		FMemory::Memcpy(&Num, Begin, 4);
		Begin +=4;

		if (Num <= 0)
			return true;

		Str.GetCharArray().SetNumUninitialized(Num + 1);
		FMemory::Memcpy(GetData(Str), Begin, Num * sizeof(TCHAR));
		Str.GetCharArray()[Num] = 0;
		return true;
	});
}

bool FDBTable::AddRow(const TMap<FString, FKeySequence>& Keys, const void* Buffer, int Size, bool bUnique)
{

	uint32 ReservedDataIndex = INVALID_DATA_INDEX;

	for (auto& Item : Keys)
	{
		auto Index = Indices.Find(Item.Key);
		check(Index);
		auto DataIndices = Index->Index->Find(ConverToNumber(Item.Value, Index->KeyTypes, false));

		RowArray Result;
		for (auto& DataIndex : DataIndices)
		{
			if (IsRowValid(DataIndex))
			{
				if (!Equal(DataIndex, Index->KeyOffset, Item.Value, Index->KeyTypes))
					continue;

				if (bUnique)
					return false;

			}
			else 
			{
				if (ReservedDataIndex == INVALID_DATA_INDEX)
					ReservedDataIndex = DataIndex;
				else
				{
					check(ReservedDataIndex == DataIndex);
				}
			}
		}
	}

	if (ReservedDataIndex != INVALID_DATA_INDEX)
	{
		WriteRow(Keys,ReservedDataIndex, WriteData(Buffer, Size));
		Header.NumRows += 1;
		FlushHeader();
	}
	else
	{
		auto DataIndex = WriteRow(Keys, Buffer, Size);
		for (auto& Item : Keys)
		{
			auto Index = Indices.Find(Item.Key);
			check(Index);

			Index->Index->Insert(ConverToNumber(Item.Value, Index->KeyTypes, true), DataIndex);
		}
	}

	return true;
}

bool FDBTable::AddRow(const TMap<FString, FKeySequence>& Keys, const FString& Val, bool bUnique)
{
	TArray<uint8> Data;
	int Len = Val.Len();
	Data.SetNumUninitialized(4 + Len * sizeof(TCHAR));
	FMemory::Memcpy(Data.GetData(), &Len, 4);
	FMemory::Memcpy(Data.GetData() + 4, GetData(Val),Len * sizeof(TCHAR));
	return AddRow(Keys, Data.GetData(), Data.Num(), bUnique);
}

bool FDBTable::UpdateRow(const FString& KeyName, const FKeySequence& Key, const void* Buffer, int Size)
{
	auto Index = Indices.Find(KeyName);
	check(Index);
	auto KeyId = ConverToNumber(Key, Index->KeyTypes, false);
	auto DataIndices = Index->Index->Find(KeyId);
	if (DataIndices.Num() == 0)
		return false;


	for (auto& Data : DataIndices)
	{
		if (!Equal(Data, Index->KeyOffset,Key, Index->KeyTypes))
			continue;
		UpdateRow(Data, Buffer, Size);
	}

	return true;
}

bool FDBTable::RemoveRow(const FString& KeyName, const FKeySequence& Key)
{
	auto Index = Indices.Find(KeyName);
	check(Index);
	auto KeyId = ConverToNumber(Key, Index->KeyTypes, false);
	auto DataIndices = Index->Index->Find(KeyId);
	if (DataIndices.Num() == 0)
		return false;


	int RemoveCount = 0;
	auto Count = DataIndices.Num();
	for (int i = 0; i < Count; ++i)
	{
		auto Data = DataIndices[i];

		if (!IsRowValid(Data))
			continue;
		if (!Equal(Data, Index->KeyOffset, Key, Index->KeyTypes))
		{
			if (RemoveCount)
			{
				MoveRowData(DataIndices[i - RemoveCount], Data);
			}
		}
		else
		{
			File->SeekWrite(Data + Header.RowDataOffset);
			File->Write(INVALID_DATA_INDEX);
			RemoveCount++;
		}
	}


	Header.NumRows -= RemoveCount;
	FlushHeader();

	return true;

}

bool FDBTable::IsRowValid(uint32 DataIndex)
{
	File->SeekRead(DataIndex + Header.RowDataOffset);
	uint32 Ptr;
	return File->Read(Ptr) && Ptr != INVALID_DATA_INDEX;
}


bool FDBTable::Equal(uint32 DataIndex, int Offset, const FKeySequence& Keys, const FKeyTypeSequence& Types)
{
	File->SeekRead(DataIndex + Offset);

	int Index = 0;
	for (auto& Type : Types)
	{
		switch (Type)
		{
		case EKeyType::Integer:
		{
			int64 Key;
			File->Read(Key);
			if (Key != AnyCast<int64>(Keys[Index]))
				return false;
		}
		break;
		case EKeyType::String:
		{
			uint32 StringIndex;
			File->Read(StringIndex);
			auto String = FileSystem->GetStaticText().Get(StringIndex);
			if (String != AnyCast<FString>(Keys[Index]))
				return false;
		}
		break;
		}
		Index++;
	}
	return true;
}


FKeySequence FDBTable::ReadRowKey(uint32 DataIndex, int Offset, const FKeyTypeSequence& Types)
{
	File->SeekRead(DataIndex + Offset);
	FKeySequence Keys;
	CHECK_RESULT(FIndexHelper::Read(Keys, File, Types));
	return Keys;
}

bool FDBTable::MoveRowData(uint32 DstDataIndex, uint32 SrcDataIndex)
{
	if (DstDataIndex == INVALID_DATA_INDEX || SrcDataIndex == INVALID_DATA_INDEX)
		return false;

	File->SeekRead(SrcDataIndex + Header.RowDataOffset);
	uint32 DataPointer;
	File->Read(DataPointer);

	File->SeekWrite(DstDataIndex + Header.RowDataOffset);
	File->Write(DataPointer);

	return true;
}


bool FDBTable::ReadRowData(uint32 DataIndex, RowData& Data)
{
	if (DataIndex == INVALID_DATA_INDEX)
		return false;

	File->SeekRead(DataIndex + Header.RowDataOffset);
	uint32 DataPointer;
	File->Read(DataPointer);
	if (DataPointer == INVALID_DATA_INDEX)
		return false;
	int Size;
	DataFile->SeekRead(DataPointer);
	DataFile->Read(Size);
	Data.SetNumUninitialized(Size, false);
	DataFile->Read(Data.GetData(), Size);

	return true;
}

bool FDBTable::ReadRowData(uint32 DataIndex, const TFunction<void* (int)>& Buffer)
{
	if (DataIndex == INVALID_DATA_INDEX)
		return false;

	File->SeekRead(DataIndex + Header.RowDataOffset);
	uint32 DataPointer;
	File->Read(DataPointer);
	if (DataPointer == INVALID_DATA_INDEX)
		return false;
	int Size;
	DataFile->SeekRead(DataPointer);
	DataFile->Read(Size);
	auto Data = Buffer(Size);
	if (Size == 0)
		return true;
	if (!Data)
		return false;
	DataFile->Read(Data, Size);
	return true;
}


uint32 FDBTable::WriteData(const void* Buffer, int Size)
{
	auto DataPointer = DataFile->GetSize();
	DataFile->SeekWrite(DataPointer);
	DataFile->Write(Size);
	DataFile->Write(Buffer, Size);
	return DataPointer;
}


void FDBTable::WriteRow(const TMap<FString, FKeySequence>& Keys, uint32 DataIndex, uint32 Data)
{
	for (auto& Item : Keys)
	{
		auto Index = Indices.Find(Item.Key);
		check(Index);
		File->SeekWrite(DataIndex + Index->KeyOffset);
		FIndexHelper::Write(Item.Value, File, Index->KeyTypes);
	}
	File->SeekWrite(DataIndex + Header.RowDataOffset);
	File->Write(Data);
}

uint32 FDBTable::WriteRow(const TMap<FString, FKeySequence>& Keys, const void* Buffer, int Size)
{
	return WriteRow(Keys,WriteData(Buffer, Size));
}

uint32 FDBTable::WriteRow(const TMap<FString, FKeySequence>& Keys, uint32 Data)
{
	auto DataIndex = Header.DataEnd;
	WriteRow(Keys,DataIndex,Data);
	Header.NumRows++;
	Header.DataEnd = DataIndex + Header.RowDataOffset + sizeof(Data);
	FlushHeader();
	return DataIndex;
}

void FDBTable::UpdateRow(uint32 DataIndex, const void* Buffer, int Size)
{
	File->SeekWrite(DataIndex + Header.RowDataOffset);
	File->Write(WriteData(Buffer, Size));
}

void FDBTable::FlushHeader()
{
	File->SeekWrite(0);
	File->Write(Header);
}

int64 FDBTable::ConverToNumber(const FKeySequence& Key,const FKeyTypeSequence& Types, bool bRefresh)
{
	if (Key.Num() == 1)
	{
		switch (Types[0])
		{
		case EKeyType::Integer: return AnyCast<int64>(Key[0]);
		case EKeyType::String: 
			if (bRefresh)
				return FileSystem->GetStaticText().FindOrCreate(AnyCast<FString>(Key[0]));
			else
				return FileSystem->GetStaticText().Find(AnyCast<FString>(Key[0]));

		}
	}
	return FIndexHelper::Hash(Key);
}

