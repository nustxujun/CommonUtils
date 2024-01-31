#include "BTree.h"


struct FData
{
	uint32 Data;
	uint32 Next;
};

constexpr int KEY_SIZE = sizeof(int64);
constexpr int DATA_SIZE = sizeof(FData);
constexpr int CHILD_SIZE = sizeof(uint32);

constexpr int M = FILE_PAGE_SIZE / (KEY_SIZE + DATA_SIZE + CHILD_SIZE) - 1;
constexpr int MAX_NUM_KEYS = M - 1;
constexpr int MAX_NUM_DATAS = M - 1;
constexpr int MAX_NUM_CHILDREN = M;



constexpr uint32 INVALID = ~0;

struct FNodeHeader
{
	uint32 Node;
	int Index;
	bool IsLeaf;
};

constexpr int MAX_NUM_SPACE_USAGE = MAX_NUM_KEYS * KEY_SIZE + MAX_NUM_DATAS * DATA_SIZE + MAX_NUM_CHILDREN * CHILD_SIZE + sizeof(int) + sizeof(FNodeHeader);
constexpr int KEY_BEGIN = sizeof(FNodeHeader) + sizeof(int);
constexpr int DATA_BEGIN = KEY_BEGIN + MAX_NUM_KEYS * KEY_SIZE;
constexpr int CHILD_BEGIN = DATA_BEGIN + MAX_NUM_DATAS * DATA_SIZE;


/*
			  
	┌───────────────────────────────────────────────────────────────┐
	│ FNodeHeader │ Keys count │ Keys ... │ Datas ... │ Children .. │
	└───────────────────────────────────────────────────────────────┘
*/


inline uint32 GetNodeOffset(uint32 Node)
{
	return Node * FILE_PAGE_SIZE;
}

#define CHECK_NODE_END_READ(Node) check(File->TellRead() <= Node * FILE_PAGE_SIZE + MAX_NUM_SPACE_USAGE);
#define CHECK_NODE_END_WRITE(Node) check(File->TellWrite() <= Node * FILE_PAGE_SIZE + MAX_NUM_SPACE_USAGE);

FBTree::FBTree(FFile::Ptr InFile):File(InFile)
{
	static_assert(MAX_NUM_SPACE_USAGE <= FILE_PAGE_SIZE, "page size is invalid");
}


static int LowerBound(int64 Key, const TArray<int64>& Array)
{
	int Begin = 0 ;
	int End = Array.Num();
	while (End != Begin )
	{
		auto Mid = (Begin + End) / 2;
		if (Array[Mid] < Key)
		{
			Begin = Mid + 1;
		}
		else
		{
			End = Mid;
		}
	}
	return Begin ;
}

bool FBTree::Find(int64 Key, uint32& Node, int& Index)
{
	static TArray<int64> Keys;
	Keys = RootKeys;
	Node = Header.RootNode;
	while (true)
	{
		auto Num = Keys.Num();
		Index = LowerBound(Key, Keys);
		if (Num == 0)
			return {};
		else if (Index < Num && Keys[Index] == Key)
		{
			return true;
		}
		else
		{
			Node = GetNextNode(Node, Index);
			if (Node == INVALID)
				return false;

			GetKeys(Node,Keys);
		}
	}
}

TArray<uint32> FBTree::Find(int64 Key)
{
	uint32 Node;
	int Index;
	if (Find(Key, Node, Index))
	{
		TArray<uint32> Datas;
		Datas.Reserve(2);
		GetData(Node, Index, [&](auto Data) {
			Datas.Add(Data);
			return false;
		});
		return Datas;
	}
	else
	{
		return {};
	}
}

bool FBTree::FindOne(int64 Key, const TFunction<bool(uint32)>& Callback)
{
	uint32 Node;
	int Index;
	if (Find(Key, Node, Index))
	{
		return GetData(Node, Index,Callback);
	}
	else
	{
		return false;
	}
}


void FBTree::Insert(int64 Key, uint32 Data)
{
	auto Keys = RootKeys;
	uint32 Node = Header.RootNode;
	while (true)
	{
		auto Num = Keys.Num();
		auto Bound = LowerBound(Key, Keys);
		if (Bound < Num && Keys[Bound] == Key)
		{
			InsertData(Node, Bound, Data);
			return;
		}
		else
		{
			auto Index = Bound;

			auto NextNode = GetNextNode(Node, Index);
			if (NextNode == INVALID)
			{
				// leaf
				if (Num + 1 >= M)
				{
					auto Mid = Num / 2;
					auto bLess = Key < Keys[Mid];
					uint32 Left, Right;
					Split(Node, Left, Right);

					if (bLess)
						InsertToNode(Left, Index, Key, Data);
					else
						InsertToNode(Right, Index - Mid - 1, Key, Data);
				}
				else
				{
					InsertToNode(Node, Index, Key, Data);
				}
				return;
			}
			else
			{
				Node = NextNode;
				GetKeys(Node,Keys);
			}
		}
	}
}

FString FBTree::GetTypeName()const
{
	return TEXT("BTreeSeacher");
}


constexpr int32 BTREE_MAGIC_NUM = 0xFB7cee;
void FBTree::Init()
{
	Header.MagicNum = BTREE_MAGIC_NUM;
	Header.RootDataPage = 0;
	Header.RootNode = 0;
	Header.PageCount = 0;

	Header.RootDataPage = CreatePage();
	Header.DataEnd = GetNodeOffset(Header.RootDataPage);
	Header.RootNode = CreateNode(INVALID, 0,true);

	FlushHeader();
}


void FBTree::Open()
{
	File->Read(Header);
	check(Header.MagicNum == BTREE_MAGIC_NUM);

	
	GetKeys(Header.RootNode, RootKeys);
}


bool FBTree::GetData(uint32 Node, int Index, const TFunction<bool(uint32)>& Callback)
{
	File->SeekRead(GetNodeOffset(Node) + sizeof(FNodeHeader));
	int Num;
	CHECK_RESULT(File->Read(Num));
	if (Index >= Num)
		return false;

	auto DataBegin = GetNodeOffset(Node) + sizeof(FNodeHeader) + sizeof(int) + MAX_NUM_KEYS * KEY_SIZE + Index * DATA_SIZE;
	File->SeekRead(DataBegin);
	CHECK_NODE_END_READ(Node);

	while(true)
	{
		FData Data;
		File->Read(Data);
		if (Callback(Data.Data))
			return true;

		if (Data.Next == INVALID)
			break;

		File->SeekRead(Data.Next);
	}
	return false;
}


void FBTree::GetKeys(uint32 Node, TArray<int64>& Keys)
{
	auto KeyBegin = GetNodeOffset(Node) + sizeof(FNodeHeader);
	File->SeekRead(KeyBegin);
	int Num;
	CHECK_RESULT(File->Read(Num));
	Keys.SetNumUninitialized(Num, false);
	File->Read(Keys.GetData(), Num * KEY_SIZE);
	CHECK_NODE_END_READ(Node);
}

uint32 FBTree::GetNextNode(uint32 Node, int Index)
{

	File->SeekRead(GetNodeOffset(Node));
	FNodeHeader NodeHeader;
	int KeyNum;
	CHECK_RESULT(File->Read(NodeHeader));
	CHECK_RESULT(File->Read(KeyNum));
	if (NodeHeader.IsLeaf || KeyNum == 0 ||  (KeyNum + 1) < Index)
		return INVALID;

	auto Skip = Index * CHILD_SIZE;
	auto ChildBegin = GetNodeOffset(Node) + CHILD_BEGIN;
	File->SeekRead(ChildBegin + Skip);
	uint32 Child;
	CHECK_RESULT(File->Read(Child));
	return Child;
}

template<class T>
static void InsertElement(const T& Value, int Count, uint32 Begin, FFile::Ptr File)
{
	TArray<T> Elements;
	Elements.SetNumUninitialized(Count);
	File->SeekRead(Begin);
	CHECK_RESULT(File->Read(Elements.GetData(), Count * sizeof(T)));
	File->SeekWrite(Begin);
	File->Write(Value);
	File->Write(Elements.GetData(), Count * sizeof(T));
}


void FBTree::InsertToNode(uint32 Node, int Pos, int64 Key, uint32 Data, uint32 Next, uint32 RightNode)
{
	int Num;
	int NodeOffset = GetNodeOffset(Node);
	auto NumBegin = NodeOffset + sizeof(FNodeHeader);
	File->SeekRead(NumBegin);
	CHECK_RESULT(File->Read(Num));
	File->SeekWrite(NumBegin);
	File->Write(Num + 1);

	int KeyCount = Num - Pos;
	auto KeyBegin = NodeOffset + KEY_BEGIN + Pos * KEY_SIZE;
	InsertElement(Key, KeyCount, KeyBegin, File);

	int DataCount = Num - Pos;
	auto DataBegin = NodeOffset + DATA_BEGIN + Pos * DATA_SIZE;
	FData DataList = { Data ,Next };
	InsertElement(DataList, KeyCount, DataBegin, File);
	CHECK_NODE_END_WRITE(Node);

	if (Node == Header.RootNode)
	{
		GetKeys(Node, RootKeys);
	}

	if (RightNode == INVALID) 
		return;// insert into leaf



	int ChildrenCount = Num - Pos;
	auto ChildBegin = NodeOffset + CHILD_BEGIN + (Pos + 1) * CHILD_SIZE;
	TArray<uint32> Children;
	Children.SetNumUninitialized(ChildrenCount);
	File->SeekRead(ChildBegin);
	CHECK_RESULT(File->Read(Children.GetData(), ChildrenCount * sizeof(uint32)));
	CHECK_NODE_END_READ(Node);

	if (Children.Num() != 0)
	{
		FNodeHeader NodeHeader;
		File->SeekRead(GetNodeOffset(Children[0]));
		File->Read(NodeHeader);
		for (auto Child : Children)
		{
			auto Offset = GetNodeOffset(Child);
			File->SeekWrite(Offset);
			NodeHeader.Index += 1;
			File->Write(NodeHeader);
		}
	}

	File->SeekWrite(ChildBegin);
	File->Write(RightNode);
	File->Write(Children.GetData(), ChildrenCount * sizeof(uint32));


	//InsertElement(RightNode, ChildrenCount, ChildBegin, File);
}

void FBTree::InsertData(uint32 Node, int Pos, uint32 Data)
{
	auto DataBegin = GetNodeOffset(Node) + DATA_BEGIN;
	auto Skip = Pos * DATA_SIZE;
	File->SeekRead(DataBegin + Skip);
	while(true)
	{
		auto Cur = File->TellRead();
		FData DataList;
		File->Read(DataList);
		if (DataList.Next != INVALID)
		{
			File->SeekRead(DataList.Next);
		}
		else
		{
			DataList.Next = AddData(Data);
			File->SeekWrite(Cur);
			File->Write(DataList);
			break;
		}
	}

}

template<class T>
static TArray<T> GetRight(int Count, uint32 Begin, FFile::Ptr File)
{
	TArray<T> Right;
	Right.SetNumUninitialized(Count);
	File->SeekRead(Begin);
	CHECK_RESULT(File->Read(Right.GetData(), Count * sizeof(T)));
	return Right;
}

template<class T>
static void FillElements(const TArray<T>& Elements, uint32 Begin, FFile::Ptr File)
{
	File->SeekWrite(Begin);
	File->Write(Elements.GetData(), sizeof(T) * Elements.Num());
}

void FBTree::Split(uint32 Node, uint32& Left, uint32& Right)
{
	auto ParentBegin = GetNodeOffset(Node);
	File->SeekRead(ParentBegin);
	FNodeHeader NodeHeader;
	CHECK_RESULT(File->Read(NodeHeader));
	auto NumBegin = File->TellRead();
	int Num;
	CHECK_RESULT(File->Read(Num));
	check(Num == MAX_NUM_KEYS);
	auto Mid = MAX_NUM_KEYS / 2;
	auto Cur = File->TellRead();
	
	// Read Mid
	int64 MidKey;
	FData MidDataList;
	File->SeekRead(Cur + Mid * KEY_SIZE);
	CHECK_RESULT(File->Read(MidKey));
	File->SeekRead(Cur + MAX_NUM_KEYS * KEY_SIZE + Mid * DATA_SIZE);
	CHECK_RESULT(File->Read(MidDataList));

	// Modify Count
	File->SeekWrite(NumBegin);
	File->Write(Mid);
	Left = Node;

	// Read Right Node
	auto Count = Num - Mid - 1;
	auto KeyBegin = Cur + (Mid + 1) * KEY_SIZE;
	auto DataBegin = Cur + MAX_NUM_KEYS * KEY_SIZE + (Mid + 1) * DATA_SIZE;
	auto ChildBegin = Cur + MAX_NUM_KEYS * KEY_SIZE + MAX_NUM_DATAS * DATA_SIZE + (Mid + 1) * CHILD_SIZE;
	
	TArray<int64> RightKeys = GetRight<int64>(Count, KeyBegin, File);
	TArray<FData> RightDatas = GetRight<FData>(Count, DataBegin, File);
	TArray<uint32> RightChildren = GetRight<uint32>(Count + 1, ChildBegin, File);

	// create right node


	auto CreateRight = [&](uint32 Parent, uint32 Index, bool bLeaf){
		Right = CreateNode(Parent, Index, bLeaf);
		auto NewNumBegin = GetNodeOffset(Right) + sizeof(FNodeHeader);
		auto NewKeyBegin = NewNumBegin + sizeof(int);
		auto NewDataBegin = NewKeyBegin + MAX_NUM_KEYS * KEY_SIZE;
		auto NewChildBegin = NewDataBegin + MAX_NUM_DATAS * DATA_SIZE;

		File->SeekWrite(NewNumBegin);
		File->Write(int(RightKeys.Num()));

		FillElements(RightKeys, NewKeyBegin, File);
		FillElements(RightDatas, NewDataBegin, File);
		if (bLeaf)
			return Right;
		FillElements(RightChildren, NewChildBegin, File);
		CHECK_NODE_END_WRITE(Right);

		if (RightChildren.Num() == 0)
			return Right;

		FNodeHeader NodeHeader;
		File->SeekRead(GetNodeOffset(RightChildren[0]));
		File->Read(NodeHeader);
		NodeHeader.Index = 0;
		for (auto Child : RightChildren)
		{
			auto Offset = GetNodeOffset(Child);
			File->SeekWrite(Offset);
			File->Write(NodeHeader);
			NodeHeader.Index += 1;
		}

		return Right;
	};


	if (NodeHeader.Node == INVALID)
	{
		// split root
		check(Node == Header.RootNode)
		auto RootNode = CreateNode(INVALID, 0, false);

		auto NewNumBegin = GetNodeOffset(RootNode) + sizeof(FNodeHeader);
		auto NewKeyBegin = NewNumBegin + sizeof(int);
		auto NewDataBegin = NewKeyBegin + MAX_NUM_KEYS * KEY_SIZE;
		auto NewChildBegin = NewDataBegin + MAX_NUM_DATAS * DATA_SIZE;

		File->SeekWrite(NewNumBegin);
		File->Write(int(1));

		FillElements(TArray<int64>{MidKey}, NewKeyBegin, File);
		FillElements(TArray<FData>{MidDataList}, NewDataBegin, File);
		FillElements(TArray<uint32>{Node, CreateRight(RootNode, 1, NodeHeader.IsLeaf)}, NewChildBegin, File);
		CHECK_NODE_END_WRITE(RootNode);


		Header.RootNode = RootNode;
		FlushHeader();
		GetKeys(RootNode, RootKeys);

		File->SeekWrite(GetNodeOffset(Node));
		File->Write(FNodeHeader{RootNode, 0, NodeHeader.IsLeaf});
	}
	else
	{
		// insert into parent
		TArray<int64> ParentKeys;
		GetKeys(NodeHeader.Node, ParentKeys);
		if (ParentKeys.Num() == MAX_NUM_KEYS)
		{
			// split parent
			auto ParentMid = MAX_NUM_KEYS / 2;
			bool bLess = MidKey < ParentKeys[ParentMid];

			uint32 ParentLeft, ParentRight;
			Split(NodeHeader.Node, ParentLeft, ParentRight);
			if (bLess)
			{
				NodeHeader.Node = ParentLeft;
			}
			else
			{
				NodeHeader.Node = ParentRight;
				check(NodeHeader.Index - ParentMid - 1 >= 0);
				NodeHeader.Index = NodeHeader.Index - ParentMid - 1;
			}
		}

		InsertToNode(NodeHeader.Node, NodeHeader.Index ,MidKey, MidDataList.Data, MidDataList.Next, CreateRight(NodeHeader.Node, NodeHeader.Index + 1, NodeHeader.IsLeaf));
	}

}

uint32 FBTree::CreatePage()
{
	File->AppendPage();
	auto NewPage = ++Header.PageCount;
	FlushHeader();

	return NewPage;
}

uint32 FBTree::CreateNode(uint32 Parent, int Index, bool bLeaf)
{
	auto NewNode = CreatePage();

	FNodeHeader ParentNode = {Parent, Index,bLeaf};
	File->SeekWrite(GetNodeOffset(NewNode));
	File->Write(ParentNode);
	const int Num = 0;
	File->Write(Num);

	return NewNode;
}

uint32 FBTree::AddData(uint32 Data)
{
	auto DataIndex = Header.DataEnd;
	Header.DataEnd += sizeof(FData);
	File->SeekWrite(DataIndex);
	FData DataList = {Data, INVALID};
	File->Write(DataList);
	if ((Header.DataEnd % FILE_PAGE_SIZE) == 0)
	{
		auto NewDataPage = CreatePage();
		Header.DataEnd = GetNodeOffset(NewDataPage);
	}

	FlushHeader();
	return DataIndex;
}

void FBTree::FlushHeader()
{
	File->SeekWrite(0);
	File->Write(Header);
}
