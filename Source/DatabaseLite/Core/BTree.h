#pragma once 

#include "File.h"




class FBTree
{

public:
	FBTree(FFile::Ptr File);

	TArray<uint32> Find(int64 Key);
	bool FindOne(int64 Key, const TFunction<bool(uint32)>& Callback);

	void Insert(int64 Key, uint32 Data);
	FString GetTypeName()const;


	void Init();
	void Open();

private:
	bool Find(int64 Key, uint32& Node, int& Index);
	bool GetData(uint32 Node, int Index, const TFunction<bool(uint32)>& Callback);
	void GetKeys(uint32 Node, TArray<int64>& Keys);
	uint32 GetNextNode(uint32 Node, int Index);

	void InsertToNode(uint32 Node, int Pos, int64 Key, uint32 Data, uint32 Next = -1, uint32 RightNode = -1);
	void InsertData(uint32 Node, int Pos, uint32 Data);
	uint32 AddData(uint32 Data);

	void Split(uint32 Node, uint32& Left, uint32& Right);
	uint32 CreateNode(uint32 Parent, int Index, bool bLeaf);
	uint32 CreatePage();

	void FlushHeader();

private:
	FFile::Ptr File;

	TArray<int64> RootKeys;

	struct
	{
		int MagicNum;
		uint32 RootDataPage;
		uint32 DataEnd;
		uint32 RootNode;
		uint32 PageCount;
	}Header;
};