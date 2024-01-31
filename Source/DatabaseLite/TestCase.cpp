#include "DatabaseLite.h"

#include <chrono>

auto Test = [](){
	{
		
		FDatabaseLite DB;
		DB.Open(FPaths::ProjectSavedDir() + TEXT("Test.db"), false, ELowLevelFileType::Memory);

		FDBTable* Table;
		if (DB.IsTableExists("TestTable"))
		{
			Table = DB.GetTable("TestTable");
		}
		else
		{
			FKeyTypeSequence Types;
			Types.Add(EKeyType::Integer);
			Table = DB.CreateTable("TestTable",{ {TEXT("id"),Types}});
			for (int64 i = 0; i < 1024 * 1024; ++i )
			{ 
				FKeySequence Keys;
				Keys.Add((i));
				Table->AddRow({{TEXT("id"), Keys}}, &i, sizeof(i),false);
			}
		}

		int Count = 0;
		auto Time = std::chrono::high_resolution_clock::now();
		for (int64 i = 0; i < 1000000  ; ++i)
		{ 
			FKeySequence Keys;
			//Keys.Add(FMath::Rand() % (1024 * 256));
			Keys.Add(i);
			int64 num;
			Table->FindOne(TEXT("id"), Keys,num);



		}
		auto Diff = (std::chrono::high_resolution_clock::now() - Time).count() / 1000.0f;
		if (Diff > 10)
		{
			Count++;
			//UE_LOG(LogCore, Log, TEXT("xxx cost %f"), Diff / 100000000);
		}
		//auto Value = *(int*)Ret[0].GetData();
	}
	UE_LOG(LogTemp, Display, TEXT("test DatabaseLite suc."));
	return 0;
};

//#include "SQLiteDatabaseConnection.h"
//#include "SQLiteResultSet.h"

//#define CHECK_EXECUTION(x) {if (!x) { PrintErrorInfo(Conn.GetLastError());}}
//
//static void PrintErrorInfo(FString String)
//{
//	check(false);
//}

//auto Test2 = [](){
//	FSQLiteDatabaseConnection Conn;
//	Conn.Open(*(FPaths::ProjectSavedDir() + TEXT("Test2.db")), 0, 0);
//
//
//
//	FSQLiteResultSet* Record;
//	
//	//CHECK_EXECUTION(Conn.Execute(TEXT("DROP table if exists 'TestTable';"), Record));
//	//delete Record;
//
//	
//	//CHECK_EXECUTION(Conn.Execute(TEXT("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='TestTable'"), Record));
//	//auto bIsExists = Record->GetInt(*Record->GetColumnNames()[0].ColumnName);`c
//	//delete Record;
//
//
//	bool bIsExists = true;
//	if (!bIsExists)
//	{
//		CHECK_EXECUTION(Conn.Execute(TEXT("CREATE TABLE 'TestTable' ( 'Id'	INTEGER, 'Content'	INTEGER, PRIMARY KEY('Id') );"), Record));
//		delete Record;
//
//		const int size = 256;
//		char Data[size] = {};
//		FMemory::Memset(Data, 'a', (size - 1));
//		for (int i = 0; i < 1024 * 256; ++i)
//		{
//			CHECK_EXECUTION(Conn.Execute(*FString::Printf(TEXT("INSERT INTO TestTable VALUES (%d, %d);"), i, i), Record));
//			delete Record;
//		}
//
//	}
//	{
//
//		auto Time = std::chrono::high_resolution_clock::now();
//		for (int i = 0; i < 1000; ++i)
//		{
//			//CHECK_EXECUTION(Conn.Execute(*FString::Printf(TEXT("SELECT * FROM TestTable WHERE id=%d;"), FMath::Rand() % (1024 * 256)), Record));
//			CHECK_EXECUTION(Conn.Execute(*FString::Printf(TEXT("SELECT * FROM TestTable WHERE id=%d;"),i), Record));
//
//
//			for (FSQLiteResultSet::TIterator NameIterator(Record); NameIterator; ++NameIterator)
//			{
//				//auto Cols = NameIterator->GetColumnNames();
//				auto id = NameIterator->GetInt(TEXT("Id"));
//				auto str = NameIterator->GetString(TEXT("Content"));
//				//do something with the results here
//			}
//
//
//			delete Record;
//		}
//
//		auto Diff = (std::chrono::high_resolution_clock::now() - Time).count() / 1000.0f;
//		UE_LOG(LogCore, Log, TEXT("yyy cost %f"), Diff / 1000);
//
//	}
//	Conn.Close();
//
//	return 0;
//};
//



TAutoConsoleVariable<int> TestCase(TEXT("ConfigTestCase"), 3, TEXT(""));


static auto TestAutoReg = []() {
	TestCase->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](auto Var) {
			Test();
			//Test2();
		}));
	return 0;
}();
