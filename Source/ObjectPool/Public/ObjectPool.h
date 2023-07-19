#pragma once

#include "CoreMinimal.h"

#define ENABLE_OBJECTS_CHECK WITH_EDITOR || UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT



/*
    dynamic size and memory usage friendly
    can be collected when gc
*/
template<class Type,  bool AutoShink = false>
class FNormalObjectPool: public FGCObject
{
    union ObjectType
    {
        void* Next;
        char Obj[sizeof(Type)];
    };
public:
    ~FNormalObjectPool()
    {
        check(NumAllocated == 0);
        DiscardUnused();
    }

    void DiscardUnused()
    {
        while (FreeList)
        {
            auto Current = FreeList;
            FreeList = *(void**)FreeList;
            FMemory::Free(Current);
        }
    }

    template<class ... Args>
	Type* Create(Args&& ... InArgs)
    {
        NumAllocated++;
        void* Current;
        if (FreeList)
        {
            Current = FreeList;
            FreeList = *(void**)FreeList;
        }
        else
        {
            Current = FMemory::Malloc( sizeof(ObjectType));
        }

#if ENABLE_OBJECTS_CHECK
        check(!TotalObjects.Contains(Current));
        TotalObjects.Add(Current);
#endif

        return new (Current) Type(Forward<Args>(InArgs) ...);
    }

    void Destroy(Type* Obj)
    {
#if ENABLE_OBJECTS_CHECK
        check(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        *(void**)Obj = FreeList;
        FreeList = Obj;
        NumAllocated--;
        check(NumAllocated >= 0);
    }



	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        if (AutoShink)
        {
            DiscardUnused();
        }
    }

private:
#if ENABLE_OBJECTS_CHECK
    TSet<void*> TotalObjects;
#endif
    void* FreeList = nullptr;
    int NumAllocated = 0;
};


/*
    fixed size and cache friendly
*/
template<class Type, int Capcity = 16>
class FFixedObjectPool
{
    union ObjectType
    {
        void* Next;
        char Obj[sizeof(Type)];
    };
public:
    FFixedObjectPool()
    {
        Pool = (Type*)FMemory::Malloc(sizeof(ObjectType)* Capcity, 64);//cache-line align

        for (int i = 0; i < Capcity; i++)
        {
            *(void**)(Pool + i) = FreeList;
            FreeList = Pool + i;
        }
    }

    ~FFixedObjectPool()
    {   
        check(NumAllocated == 0);
        FMemory::Free(Pool);
    }


    template<class ... Args>
	Type* Create(Args&& ... InArgs)
    {
        NumAllocated++;
        void* Current;
        if (FreeList)
        {
            Current = FreeList;
            FreeList = *(void**)Current;
        }
        else
        {
            Current = FMemory::Malloc(sizeof(Type));
        }

#if ENABLE_OBJECTS_CHECK
        TotalObjects.Add(Current);
#endif
        return new (Current) Type(Forward<Args>(InArgs) ...);

    }

    void Destroy(Type* Obj)
    {
#if ENABLE_OBJECTS_CHECK
        check(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        if (Obj < Pool || Obj >= Pool + Capcity)
        {
            FMemory::Free(Obj);
        }
        else
        {

            *(void**)Obj = FreeList;
            FreeList = Obj;
        }
        NumAllocated--;
        check(NumAllocated >= 0);
    }

private:
#if ENABLE_OBJECTS_CHECK
    TSet<void*> TotalObjects;
#endif

    Type* Pool = nullptr;
    void* FreeList = nullptr;

    int NumAllocated = 0;
};

/*
    dynamic size and cache friendly
    can be collected by gc
*/

template<class Type>
CONSTEXPR FORCEINLINE int GetCountOnCacheLine()
{
    constexpr int CacheLineSize = 64;
    // use for unshrink pool
    using ObjectType = union{Type Obj; void* Next;};
    constexpr auto Count = CacheLineSize  / sizeof(ObjectType);
#if ENGINE_MAJOR_VERSION >= 5
    if constexpr(Count > 0 && sizeof(ObjectType) != CacheLineSize)
#else
    if (Count > 0 && sizeof(ObjectType) != CacheLineSize)
#endif
        return Count; // expect object on the same cache-line
    else
        return 16; 
}

template<class Type, int CountPerChunk = GetCountOnCacheLine<Type>(), bool AutoShrink = false>
class FFlatObjectPool;

template<class Type, int CountPerChunk>
class FFlatObjectPool<Type, CountPerChunk, false>
{
    union ObjectType
    {
        void* Next;
        char Obj[sizeof(Type)];
    };
public:
    ~FFlatObjectPool()
    {   
        check(NumAllocated == 0);
        
        for(auto& Chunk : Chunks)
            FMemory::Free(Chunk);

    }


    template<class ... Args>
	Type* Create(Args&& ... InArgs)
    {
        void* Current;
        if (FreeList == nullptr)
        {
            NewChunk();
        }
        Current = FreeList;
        FreeList = *(void**)Current;

#if ENABLE_OBJECTS_CHECK
        TotalObjects.Add(Current);
#endif
        NumAllocated++;
        return new (Current) Type(Forward<Args>(InArgs) ...);
    }

    void NewChunk()
    {

        auto Mem = FMemory::Malloc( sizeof(ObjectType) * CountPerChunk, 64); // cache-line align
        Chunks.Add(Mem);
        for (int i = 0; i < CountPerChunk; i++)
        {
            *(void**)Mem = FreeList;
            FreeList = Mem;
            Mem = (Type*)Mem + 1;
        }
    }

    void Destroy(Type* Obj)
    {
#if ENABLE_OBJECTS_CHECK
        check(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        *(void**)Obj = FreeList;
        FreeList = Obj;
        NumAllocated--;
        check(NumAllocated >= 0);
    }
private:
#if ENABLE_OBJECTS_CHECK
    TSet<void*> TotalObjects;
#endif

    TArray<void*> Chunks;
    void* FreeList = nullptr;

    int NumAllocated = 0;

};



template<class Type, int CountPerChunk>
class FFlatObjectPool<Type, CountPerChunk, true>: public FGCObject
{
    struct ObjectType
    {
        uint64 Index;
        union
        {
            void* Next;
            char Obj[sizeof(Type)];
        };
    };


    struct Chunk
    {
        void* Memory;
        void* FreeList;
        int NumAllocated;
    };
public:
    ~FFlatObjectPool()
    {
        for (auto& Chunk : Chunks)
        {
            check(Chunk.NumAllocated == 0);
        }

        DiscardUnused();
    }

    template<class ... Args>
    Type* Create(Args&& ... InArgs)
    {
        void* Current;
        Chunk* AllocatedChunk = nullptr;
        for (auto& Chunk : Chunks)
        {
            if (Chunk.FreeList)
            {
                AllocatedChunk = &Chunk;
            }
        }

        if (AllocatedChunk == nullptr)
        {
            AllocatedChunk = &NewChunk();
        }

        Current = AllocatedChunk->FreeList;
        AllocatedChunk->FreeList = *(void**)Current;
        AllocatedChunk->NumAllocated++;

#if ENABLE_OBJECTS_CHECK
        TotalObjects.Add(Current);
#endif
        return new (Current) Type(Forward<Args>(InArgs) ...);
    }


    void Destroy(Type* Obj)
    {
#if ENABLE_OBJECTS_CHECK
        check(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        
        auto Index = (uint64*)Obj - 1;
        auto& Chunk = Chunks[*Index];
        *(void**)Obj = Chunk.FreeList;
        Chunk.FreeList = Obj;
        Chunk.NumAllocated--;
        check(Chunk.NumAllocated >= 0);
    }

    virtual void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        DiscardUnused();
    }


    void DiscardUnused()
    {
        TArray<int> RemoveList;
        int Count = Chunks.Num();
        for (int i = Count - 1; i >= 0; --i)
        {
            if (Chunks[i].NumAllocated == 0)
            {
                RemoveList.Add(i);
            }
        }

        for (auto i : RemoveList)
        {
            FMemory::Free(Chunks[i].Memory);
            Chunks.RemoveAt(i,1,false);
        }

    }
private:
    Chunk& NewChunk()
    {
        auto Mem = FMemory::Malloc(sizeof(ObjectType) * CountPerChunk, 8); 
        Chunks.AddUninitialized();
        auto Index = Chunks.Num() - 1;
        auto& Chunk = Chunks[Index];
        Chunk.Memory = Mem;
        Chunk.FreeList = nullptr;
        Chunk.NumAllocated = 0;

        for (int i = 0; i < CountPerChunk; i++)
        {
            *(uint64*)Mem = Index;
            ((ObjectType*)Mem)->Next = Chunk.FreeList;
            Chunk.FreeList = (uint64*)Mem + 1;
            Mem = (ObjectType*)Mem + 1;
        }
        return Chunk;
    }

private:
#if ENABLE_OBJECTS_CHECK
    TSet<void*> TotalObjects;
#endif


    TArray<Chunk> Chunks;

};