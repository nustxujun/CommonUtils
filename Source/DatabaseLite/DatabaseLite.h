#pragma once 

#include "CoreMinimal.h"
#include "Core/Table.h"
#include "Core/Index.h"
#include "Core/File.h"

class DATABASELITE_API FDatabaseLite
{
public:
	~FDatabaseLite();
	bool Open(const FString& FileName, bool bReadOnly = true, ELowLevelFileType FileType = ELowLevelFileType::Cached);
	void Close();
	FDBTable* GetTable(const FString& TableName) ;
	FDBTable* CreateTable(const FString& TableName, const TMap<FString, FKeyTypeSequence> &IndexKeyTypes);
	void DeleteTable(const FString& TableName);
	bool IsTableExists(const FString& TableName)const;

	FDBTable::RowArray Query(const FString& TableName, const FString& KeyName, const FKeySequence& Keys);
	FDBTable::RowArray GetRows(const FString& TableName);

private:
	FDBTable* OpenTable(const FString& TableName);
	void InitInternalTable();
	void AddTableRecord(const FString& Name, PageId Record);
	void RemoveTableRecord(const FString& Name);
	PageId GetTableFile(const FString& Name);
private:
	TSharedPtr<FFileSystem> FileSys;
	TMap<FString, TSharedPtr<FDBTable>> Tables;

	TSharedPtr<FDBTable> InternalTable;
};