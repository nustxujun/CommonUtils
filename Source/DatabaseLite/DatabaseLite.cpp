#include "DatabaseLite.h"
#include "Range.h"

DEFINE_LOG_CATEGORY_STATIC(LogDatabaseLite, Log, All);


const static FString NAME_STRING = TEXT("Name");
const static FString TABLE_RECORDS_FILE = TEXT("TableRecords");


FDatabaseLite::~FDatabaseLite()
{
	Close();
}


bool FDatabaseLite::Open(const FString& FileName, bool bReadOnly, ELowLevelFileType FileType)
{
	FileSys = MakeShared<FFileSystem>();
	if (!FileSys->Init(FileName, bReadOnly, FileType))
	{
		FileSys.Reset();
		return false;
	}

	InitInternalTable();

	return true;
}

void FDatabaseLite::Close()
{
	InternalTable.Reset();
	Tables.Reset();
	FileSys.Reset();
}

void FDatabaseLite::InitInternalTable()
{
	auto Records = FileSys->OpenFile(TABLE_RECORDS_FILE);
	if (!Records)
	{
		Records = FileSys->NewFile(TABLE_RECORDS_FILE);
		InternalTable = MakeShared<FDBTable>(Records);
		InternalTable->Init({{NAME_STRING, FKeyTypeSequence{EKeyType::String}}});

	}
	else
	{
		InternalTable = MakeShared<FDBTable>(Records);
		InternalTable->Open();

	}
}

void FDatabaseLite::AddTableRecord(const FString& Name, PageId Id)
{
	check(InternalTable->Find(NAME_STRING,Name).Num() == 0);
	CHECK_RESULT(InternalTable->AddRow({{NAME_STRING, Name}}, Id, true));
}

void FDatabaseLite::RemoveTableRecord(const FString& Name)
{
	InternalTable->RemoveRow(NAME_STRING, Name);

}
PageId FDatabaseLite::GetTableFile(const FString& Name)
{
	PageId Id = PAGE_ID_INVALID;
	InternalTable->FindOne(NAME_STRING, Name, Id);
	return Id;
}


FDBTable* FDatabaseLite::GetTable(const FString& TableName) 
{
	auto Tab = Tables.FindRef(TableName);
	if (Tab)
		return Tab.Get();
	else
		return OpenTable(TableName);
}

FDBTable* FDatabaseLite::CreateTable(const FString& TableName, const TMap<FString, FKeyTypeSequence> & IndexKeyTypes)
{
	check(!IsTableExists(TableName));

	auto TableFile = FileSys->NewFile();
	AddTableRecord(TableName, TableFile->GetId());

	auto Table = MakeShared<FDBTable>(TableFile);
	Table->Init(IndexKeyTypes);

	Tables.Add(TableName, Table);
	return GetTable(TableName);
}

FDBTable* FDatabaseLite::OpenTable(const FString& TableName)
{
	auto Id = GetTableFile(TableName);
	if (Id == PAGE_ID_INVALID)
	{
		UE_LOG(LogDatabaseLite, Warning, TEXT("can not open table %s"), *TableName);
		return nullptr;
	}
	check(IsTableExists(TableName));
	check(Tables.Find(TableName) == nullptr);

	auto Table = MakeShared<FDBTable>(FileSys->OpenFile(Id));
	Table->Open();
	Tables.Add(TableName, Table);
	return GetTable(TableName);
}

void FDatabaseLite::DeleteTable(const FString& TableName)
{
	if (!IsTableExists(TableName))
		return;
	auto Table = GetTable(TableName);
	Table->Delete();
	Tables.Remove(TableName);
	RemoveTableRecord(TableName);
}

bool FDatabaseLite::IsTableExists(const FString& TableName)const
{
	if (Tables.Find(TableName))
		return true;
	PageId Id;
	return InternalTable->FindOne(NAME_STRING,TableName, Id) ;
}

FDBTable::RowArray FDatabaseLite::Query(const FString& TableName, const FString& KeyName, const FKeySequence& Keys) 
{
	auto Table = GetTable(TableName);
	if (!Table)
		return {};

	return Table->Find(KeyName, Keys);
}

FDBTable::RowArray FDatabaseLite::GetRows(const FString& TableName)
{
	auto Table = GetTable(TableName);
	if (!Table)
		return {};

	return Table->GetRows();
}
