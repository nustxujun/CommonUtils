#pragma once 

#include "CoreMinimal.h" 

template<class Scalar, uint32 Dimensions>
struct TDistanceMethod
{
    FORCEINLINE Scalar operator()(const Scalar* P1, const Scalar* P2)const
    {
        Scalar Distance = 0;
        for (int32 i = 0; i < Dimensions; P1++, P2++, i++)
        {
            Scalar Diff = *P1 - *P2;
            Distance += Diff * Diff;
        }
        return FMath::Sqrt(Distance);
    }
};

template<
    class ValueType, 
    uint32 Dimensions = 2, 
    class Scalar = float , 
    class DistanceMethod = TDistanceMethod<Scalar, Dimensions>
>
class AcmKDTree
{
    struct Node
    {
        Scalar Point[Dimensions];
        Node* Left ;
        Node* Right ;

        Node(const Scalar* InPoint):
            Left(nullptr),Right(nullptr)
        {
            FMemory::Memcpy(Point, InPoint, sizeof(Scalar) * Dimensions);
        }
    };

    struct InternalNode
    {
        Scalar Distance;
        Node* Node;

        bool operator < (const InternalNode& Other)const
        {
            return Distance > Other.Distance;
        }
    };
public:
    AcmKDTree(int Capacity = 0)
    {
        Nodes.Reserve(Capacity);
        Values.Reserve(Capacity);
    }

    void Build()
    {
        const auto Num = Nodes.Num();

        TArray<Node*> List;
        List.Reserve(Num);
        for (int32 i = 0; i < Num; i++)
        {
            List.Add(&Nodes[i]);
        }
        Root = MakeTree(MoveTemp(List));

    }

    void Add(const Scalar* Point, ValueType Value)
    {
        Nodes.Emplace(Point);
        Values.Add(MoveTemp(Value));
    }

    template<class PointType>
    void Add(const PointType& Point, ValueType Value)
    {
        check(sizeof(PointType) == sizeof(Scalar) * Dimensions);
        Add((const Scalar*)&Point, MoveTemp(Value));
    }


    void Search(const Scalar* Point, Scalar Radius, int32 MaxNum, TArray<ValueType>& OutValues)
    {
        TArray<InternalNode> Output;
        Search(Root, Point, 0, Radius, MaxNum,DistanceMethod(), Output);
        for (auto& Item : Output)
        {
            OutValues.Add(GetValue(Item.Node));
        }
    }

    template<class Vec>
    void Search(const Vec& Point, Scalar Radius, int32 MaxNum, TArray<ValueType>& OutValues)
    {
        check(sizeof(Vec) == sizeof(Scalar) * Dimensions);
        Search((const Scalar*)&Point, Radius, MaxNum, OutValues);
    }

private:
    void Search(Node* Node,const Scalar* Point,int Level,float Radius,int32 MaxNum,const DistanceMethod& Method, TArray<InternalNode>& Output )
    {
        if(Node == nullptr)
        {
            return;
        }

        auto Dim = Level % Dimensions;
        auto Diff = Point[Dim] - Node->Point[Dim];
        if (Diff < 0)
        {
            Search(Node->Left, Point, Level + 1, Radius, MaxNum, Method,Output);
        }
        else
        {
            Search(Node->Right, Point, Level + 1, Radius, MaxNum, Method,Output);
        }

        auto Distance = Method(Point, Node->Point);
        if (Distance <= Radius)
        {
            Output.HeapPush({Distance, Node});

            while (Output.Num() > MaxNum)
            {
                Output.HeapPopDiscard(false);
            }
        }

        if (Output.Num() > 0)
        {
            auto& Top = Output.Top();

            if (Diff < 0 && Top.Distance >= -Diff)
            {
                Search(Node->Right, Point, Level + 1, Radius, MaxNum, Method, Output);
            }
            else if (Diff >= 0 && Top.Distance >= Diff)
            {
                Search(Node->Left, Point, Level + 1, Radius, MaxNum, Method, Output);
            }
        };
    }

    const ValueType& GetValue(Node* Node)const
    {
        return Values[Node - Nodes.GetData()];
    }

    int GetStackSize()const
    {
        return RoundUpToPowerOfTwo(Nodes.Num());
    }

private:

    Node* MakeTree(TArray<Node*> List, int Level = 0)
    {
        auto Num = List.Num();
        auto CurDim = Level % Dimensions;
        auto Mid = Num / 2;

        List.Sort([CurDim](auto& A, auto& B)->bool{
            return A.Point[CurDim] < B.Point[CurDim];
        });

        auto MidNode = List[Mid];

        if (Mid + 1 < Num)
        {
            TArray<Node*> Right;
            Right.Append(List.GetData() + Mid + 1, Num - Mid - 1);
            MidNode->Right = MakeTree( MoveTemp(Right), Level + 1);
        }
        if (Mid > 0)
        {
            List.RemoveAtSwap(Mid, Num - Mid);
            MidNode->Left = MakeTree(MoveTemp(List), Level + 1);
        }
        return MidNode;
    }
            


private:
    TArray<Node> Nodes;
    TArray<ValueType> Values;
    Node* Root;
};