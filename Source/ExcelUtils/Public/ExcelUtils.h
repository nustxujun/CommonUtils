#pragma once 

#include "CoreMinimal.h"
#include "ExcelUtils.generated.h"



namespace libxl
{
	template<class>
	struct IBookT;

	typedef IBookT<TCHAR> Book;
}

UENUM(BlueprintType)
enum class EExcelCellType : uint8
{
	CELLTYPE_EMPTY, 
	CELLTYPE_NUMBER, 
	CELLTYPE_STRING, 
	CELLTYPE_BOOLEAN, 
	CELLTYPE_BLANK, 
	CELLTYPE_ERROR 
};

UCLASS(BlueprintType, Blueprintable)
class EXCELUTILS_API UExcelFile:public UObject
{
	GENERATED_BODY()
public:
	/*
	*
	* @param bNolock Make a copy when opening excel file,then do every operations on this copy to avoid locking file
	*/
	UFUNCTION(BlueprintCallable)
	static UExcelFile* CreateExcelFile(FString FilePath, bool bNolock = true);

	/*
	*
	* @param bNolock see more about CreateExcelFile
	*/
	UFUNCTION(BlueprintCallable)
	bool Open(FString FilePath, bool bNolock = true);

	/*
	*
	* @param FilePath If FilePath is EMPTY, save to original file.
	*/
	UFUNCTION(BlueprintCallable)
	bool Save(FString FilePath = TEXT(""));

	UFUNCTION(BlueprintCallable)
	void Close();

	UFUNCTION(BlueprintCallable)
	int32 GetSheetCount()const;

	UFUNCTION(BlueprintCallable)
	FString GetSheetName(int32 SheetIndex)const;

	UFUNCTION(BlueprintCallable)
	int32 AddSheet(const FString& Name );

	UFUNCTION(BlueprintCallable)
	int32 FindSheetByName(const FString& Name);

	UFUNCTION(BlueprintCallable)
	int32 GetSheetLastRow(int32 SheetIndex)const;

	UFUNCTION(BlueprintCallable)
	int32 GetSheetLastCol(int32 SheetIndex)const;

	UFUNCTION(BlueprintCallable)
	FString ReadString(int32 SheetIndex, int32 Row, int32 Col);

	UFUNCTION(BlueprintCallable)
	float ReadNumber(int32 SheetIndex, int32 Row, int32 Col);

	UFUNCTION(BlueprintCallable)
	int ReadInt(int32 SheetIndex, int32 Row, int32 Col);

	UFUNCTION(BlueprintCallable)
	int64 ReadInt64(int32 SheetIndex, int32 Row, int32 Col);

	UFUNCTION(BlueprintCallable)
	TArray<FString> ReadRow(int32 SheetIndex, int32 Row);

	UFUNCTION(BlueprintCallable)
	TArray<FString> ReadCol(int32 SheetIndex, int32 Col);

	UFUNCTION(BlueprintCallable)
	void WriteString(int32 SheetIndex, int32 Row, int32 Col, const FString& Str);

	UFUNCTION(BlueprintCallable)
	void WriteRow(int32 SheetIndex, int32 Row, int32 StartCol, const TArray<FString>& RowStr);

	UFUNCTION(BlueprintCallable)
	void AddRow(int32 SheetIndex, int32 StartCol, const TArray<FString>& RowStr);

	UFUNCTION(BlueprintCallable)
	EExcelCellType GetCellType(int32 SheetIndex, int32 Row, int32 Col);



public:
	const TCHAR* ReadRaw(int32 Sheet, int32 Row, int32 Col);

	void Foreach(int32 SheetIndex, TFunction<bool(int32, int32, const TCHAR*)> Callback, int32 BeginRow = 0, int32 BeginCol = 0, int32 EndRow = INDEX_NONE, int32 EndCol = INDEX_NONE);

private:
	bool Load(bool bNolock);
private:
	TSharedPtr < libxl::Book > Book;
	FString PathName;
};