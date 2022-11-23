#include "ExcelUtils.h"
#include "LibraryExcel.h"


static const auto EXT_BAK = TEXT(".bak");

UExcelFile* UExcelFile::CreateExcelFile(FString FilePath, bool bNolock )
{
	auto File = NewObject<UExcelFile>();
	File->Open(MoveTemp(FilePath), bNolock);
	return File;
}


bool UExcelFile::Open(FString FilePath, bool bNolock)
{
	PathName = MoveTemp(FilePath);
#if WITH_LIBXL
	// custom deleter
	Book = TSharedPtr<libxl::Book>(xlCreateXMLBook(), [](auto* BK) { BK->release(); });

	//  set license key
	const static auto User = TEXT("TommoT");
	const static auto Pwd = TEXT("windows-2421220b07c2e10a6eb96768a2p7r6gc");
	Book->setKey(User, Pwd);
	return Load(bNolock);
#else
	return false;
#endif
}

bool UExcelFile::Load(bool bNolock)
{
#if WITH_LIBXL
	if (bNolock)
	{
		auto LoadPath = FPaths::CreateTempFilename(*FPaths::GetPath(PathName), TEXT(""), EXT_BAK);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.CopyFile(*LoadPath, *PathName, EPlatformFileRead::AllowWrite))
		{
			return false;
		}

		auto Result = Book->load(*LoadPath);

		PlatformFile.DeleteFile(*LoadPath);
		return Result;
	}
	else
	{
		auto Result = Book->load(*PathName);
		return Result;
	}
#else
	return false;
#endif
}

bool UExcelFile::Save(FString FilePath )
{
#if WITH_LIBXL
	if (FilePath.IsEmpty())
		FilePath = PathName;

    auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FilePath));

	return Book->save(*FilePath);
#else
	return false;
#endif
}



void UExcelFile::Close()
{
#if WITH_LIBXL
	if (Book)
	{
		Book.Reset();
	}
#endif
}


int32 UExcelFile::GetSheetCount()const
{
#if WITH_LIBXL
	return Book->sheetCount();
#else
	return 0;
#endif
}

FString UExcelFile::GetSheetName(int32 SheetIndex)const
{
#if WITH_LIBXL
	return Book->getSheetName(SheetIndex);
#else
	return {};
#endif
}

int32 UExcelFile::AddSheet(const FString& Name)
{
#if WITH_LIBXL
	Book->addSheet(*Name);
	return Book->sheetCount() - 1;
#else
	return INDEX_NONE;
#endif
}

int32 UExcelFile::FindSheetByName(const FString& Name)
{
	auto Count = GetSheetCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Name == GetSheetName(Index))
			return Index;
	}
	return INDEX_NONE;
}


int32 UExcelFile::GetSheetLastRow(int32 SheetIndex)const
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		return Sheet->lastRow();
#endif
	return 0;
}


int32 UExcelFile::GetSheetLastCol(int32 SheetIndex)const
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		return Sheet->lastCol();
#endif
	return 0;
}


FString UExcelFile::ReadString(int32 SheetIndex, int32 Row, int32 Col)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		return Sheet->readStr(Row, Col);
#endif
	return {};
}

float UExcelFile::ReadNumber(int32 SheetIndex, int32 Row, int32 Col)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (!Sheet)
		return 0;
	auto Str = Sheet->readStr(Row, Col);
	if (Str)
		return FCString::Atof(Str);
#endif
	return {};
}

int64 UExcelFile::ReadInt64(int32 SheetIndex, int32 Row, int32 Col)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (!Sheet)
		return 0;
	auto Str = Sheet->readStr(Row, Col);
	if (Str)
		return FCString::Atoi64(Str);
#endif
	return {};
}

int UExcelFile::ReadInt(int32 SheetIndex, int32 Row, int32 Col)
{
	return (int)ReadInt64(SheetIndex, Row, Col);
}

const TCHAR* UExcelFile::ReadRaw(int32 SheetIndex, int32 Row, int32 Col)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		return Sheet->readStr(Row, Col);
#endif
	return nullptr;
}


TArray<FString> UExcelFile::ReadRow(int32 SheetIndex, int32 Row)
{
	TArray<FString> Result;
	Foreach(SheetIndex, [&](int32, int32, auto Str) { Result.Add(Str); return true; }, Row,  0, Row + 1, GetSheetLastCol(SheetIndex));
	return Result;
}


TArray<FString> UExcelFile::ReadCol(int32 SheetIndex, int32 Col)
{
	TArray<FString> Result;
	Foreach(SheetIndex, [&](int32, int32, auto Str) { Result.Add(Str); return true; }, 0,  Col, GetSheetLastRow(SheetIndex), Col + 1);
	return Result;
}

void UExcelFile::WriteString(int32 SheetIndex, int32 Row, int32 Col, const FString& Str)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		Sheet->writeStr(Row, Col, *Str);
#endif
}


void UExcelFile::WriteRow(int32 SheetIndex, int32 Row, int32 StartCol, const TArray<FString>& RowStr)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (!Sheet)
		return;
	for (int32 Col = 0; Col < RowStr.Num(); ++Col)
	{
		Sheet->writeStr(Row, StartCol + Col, *RowStr[Col]);
	}
#endif
}

void UExcelFile::AddRow(int32 SheetIndex, int32 StartCol, const TArray<FString>& RowStr)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (!Sheet)
	{
		ensureMsgf(false, TEXT("Sheet %d is not existing."), SheetIndex);
		return;
	}

	auto StartRow = Sheet->lastRow();
	auto Count = RowStr.Num();
	for (int i = 0; i < Count; ++i)
	{
		Sheet->writeStr(StartRow, StartCol + i, *RowStr[i]);
	}
#endif
}



void UExcelFile::Foreach(int32 SheetIndex, TFunction<bool(int32, int32, const TCHAR*)> Callback, int32 BeginRow , int32 BeginCol , int32 EndRow , int32 EndCol )
{
	if (EndRow == INDEX_NONE)
		EndRow = GetSheetLastRow(SheetIndex);
	if (EndCol == INDEX_NONE)
		EndCol = GetSheetLastCol(SheetIndex);

	for (int32 Row = BeginRow; Row < EndRow; ++Row)
	{
		for (int32 Col = BeginCol; Col < EndCol; ++Col)
		{
			if (!Callback(Row, Col, ReadRaw(SheetIndex, Row, Col)))
			{
				return;
			}
		}
	}

}


EExcelCellType UExcelFile::GetCellType(int32 SheetIndex, int32 Row, int32 Col)
{
#if WITH_LIBXL
	auto Sheet = Book->getSheet(SheetIndex);
	if (Sheet)
		return static_cast<EExcelCellType>(Sheet->cellType(Row, Col));
#endif
	return EExcelCellType::CELLTYPE_ERROR;
}
