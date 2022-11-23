#pragma once

#include "CoreMinimal.h"

#define ENABLE_OBJECTS_CHECK WITH_EDITOR || UE_BUILD_DEBUG || UE_BUILD_TEST



/*
    dynamic size and memory usage friendly
    can be collected when gc
*/
template<class Type,  bool AutoShink = false>
class FNormalObjectPool: public FGCObject
{
    union ObjectType
    {
        Type Obj;
        void* Next;
    };
public:
    ~FNormalObjectPool()
    {
        ensure(NumAllocated == 0);
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
#if ENABLE_OBJECTS_CHECK
        TotalObjects.Empty();
#endif
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
        ensure(!TotalObjects.Contains(Current));
        TotalObjects.Add(Current);
#endif

        return new (Current) Type(Forward<Args>(InArgs) ...);
    }

    void Destroy(Type* Obj)
    {
#if ENABLE_OBJECTS_CHECK
        ensure(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        *(void**)Obj = FreeList;
        FreeList = Obj;
        NumAllocated--;
        ensure(NumAllocated >= 0);
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
        Type Obj;
        void* Next;
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
        ensure(NumAllocated == 0);
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
        ensure(TotalObjects.Contains(Obj));
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
        ensure(NumAllocated >= 0);
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
*/

template<class Type>
CONSTEXPR FORCEINLINE int GetCountOnCacheLine()
{
    constexpr int CacheLineSize = 64;
    using ObjectType = union{Type Obj; void* Next;};
    constexpr auto Count = CacheLineSize  / sizeof(ObjectType);
#if PLATFORM_COMPILER_HAS_IF_CONSTEXPR
    if constexpr(Count > 0 && sizeof(ObjectType) != CacheLineSize)
#else
    if (Count > 0 && sizeof(ObjectType) != CacheLineSize)
#endif
        return Count; // expect object on the same cache-line
    else
        return 16; 
}

/*
    used for 
*/

template<class Type, int CountPerChunk = GetCountOnCacheLine<Type>()>
class FFlatObjectPool
{
    union ObjectType
    {
        Type Obj;
        void* Next;
    };
public:
    ~FFlatObjectPool()
    {   
        ensure(NumAllocated == 0);
        
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
        ensure(TotalObjects.Contains(Obj));
        TotalObjects.Remove(Obj);
#endif
        Obj->~Type();
        *(void**)Obj = FreeList;
        FreeList = Obj;
        NumAllocated--;
        ensure(NumAllocated >= 0);
    }
private:
#if ENABLE_OBJECTS_CHECK
    TSet<void*> TotalObjects;
#endif

    TArray<void*> Chunks;
    void* FreeList = nullptr;

    int NumAllocated = 0;

};