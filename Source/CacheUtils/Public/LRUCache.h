#pragma once 

#include "CoreMinimal.h"


template<int Size>
struct TLRUQueue
{
    struct Node
    {
        Node* Prev = nullptr;
        Node* Next = nullptr;
    };

    Node Nodes[Size];
    Node* Head;
    Node* Tail;

    TLRUQueue()
    {
        static_assert(Size >= 3, "LRU Size must be greater and than 2");
        Reset();
    }

    int GetIndex(const Node* Node)
    {
        return Node - Nodes;
    }

    int Refresh(Node* Node)
    {
        if (Node == Head)
            return GetIndex(Node);

        if (Node == Tail)
        {
            Tail = Node->Prev;
            Tail->Next = nullptr;

            Node->Prev = nullptr;
            Node->Next = Head;
            Head->Prev = Node;
            Head = Node;
        }
        else
        {
            Node->Prev->Next = Node->Next;
            Node->Next->Prev = Node->Prev;

            Node->Prev = nullptr;
            Node->Next = Head;
            Head->Prev = Node;
            Head = Node;
        }
        return GetIndex(Node);
    }


    void Reset()
    {
        for (int i = 0; i < Size; ++i)
        {
            Nodes[i].Prev = &Nodes[i - 1];
            Nodes[i].Next = &Nodes[i + 1];
        }
        Nodes[0].Prev = nullptr;
        Nodes[Size - 1].Next = nullptr;

        Head = Nodes;
        Tail = Nodes + Size - 1;
    }


};


template<class KeyType, class ValueType, int Size>
class TFlatLRUCache
{
public:
    ValueType* Push(KeyType Key)
    {
        int Index ;
        if ( (Index = Refer(Key)) != -1)
        {
            return &Values[Index];
        }

        Index = Queue.Refresh(Queue.Tail);

        NodeMap.Remove(Keys[Index]);
        NodeMap.Add(Key, Queue.Nodes + Index);
        Keys[Index] = MoveTemp(Key);
        return &Values[Index];
    }

    bool Push(KeyType Key, ValueType Value)
    {
        auto ValuePtr = Push(MoveTemp(Key));
        if (ValuePtr)
        {
            *ValuePtr = MoveTemp(Value);
            return true;
        }
        else
            return false;
    }

    ValueType* Get(const KeyType& Key)
    {
        auto Node = NodeMap.FindRef(Key);
        if (Node == nullptr)
            return nullptr;

        return &Values[Queue.GetIndex(Node)];
    }

    int Refer(const KeyType& Key)
    {
        auto Node = NodeMap.FindRef(Key);
        if (Node == nullptr)
            return -1;

        return Queue.Refresh(Node);
    }

    ValueType* GetAndRefer(const KeyType& Key)
    {
        auto Node = NodeMap.FindRef(Key);
        if (Node == nullptr)
            return nullptr;

        return &Values[Queue.Refresh(Node)];
    }


    void Reset()
    {
        NodeMap.Empty(Size);
        Queue.Reset();

        for (int i = 0; i < Size; ++i)
        {
            Keys[i] = {};
            Values[i] = {};
        }
    }

private:
    TLRUQueue<Size> Queue;
    KeyType Keys[Size] = {};
    ValueType Values[Size] = {};
    TMap<KeyType, typename TLRUQueue<Size>::Node*> NodeMap;
};


template<class KeyType, class ValueType, int Size, int K>
class TFlatLRUCacheK
{
public:
    ValueType* Get(KeyType Key)
    {

        auto Node = NodeMap.FindRef(Key);
        if (Node == nullptr)
            return nullptr;

        return Values + Queue.Refresh(Node);
    }

    ValueType* Push(KeyType Key)
    {
        int Index;
        if ( (Index = Refer(Key)) != -1)
        {
            return &Values[Index];
        }

        auto Counter = History.Get(Key);
        if (!Counter)
        {
            History.Push(Key,1);
            return nullptr;
        }
        else if (*Counter < K - 1)
        {
            *Counter += 1;
            return nullptr;
        }
        else
        {
            *Counter = 1;
        }

        Index = Queue.Refresh(Queue.Tail);

        NodeMap.Remove(Keys[Index]);
        NodeMap.Add(Key, Queue.Nodes + Index);
        Keys[Index] = MoveTemp(Key);
        return &Values[Index];
    }

    bool Push(KeyType Key, ValueType Value)
    {
        auto ValuePtr = Push(MoveTemp(Key));
        if (ValuePtr)
        {
            *ValuePtr = MoveTemp(Value);
            return true;
        }
        else
            return false;
    }

    int Refer(const KeyType& Key)
    {
        auto Node = NodeMap.FindRef(Key);
        if (Node == nullptr)
            return -1;

        auto Counter = History.Get(Key);
        if (!Counter)
        {
            History.Push(Key, 1);
            return -1;
        }
        else if (*Counter < K - 1)
        {
            *Counter += 1;
            return -1;
        }
        else
        {
            //*Counter = 1;
        }


        
        return Queue.Refresh(Node);
    }

    void Reset()
    {
        NodeMap.Empty(Size);
        Queue.Reset();
        History.Reset();

        for (int i = 0; i < Size; ++i)
        {
            Keys[i] = {};
            Values[i] = {};
        }
    }

private:
    TFlatLRUCache<KeyType, int, Size> History;
    TLRUQueue<Size> Queue;
    KeyType Keys[Size] = {};
    ValueType Values[Size] = {};
    TMap<KeyType, typename TLRUQueue<Size>::Node*> NodeMap;
};