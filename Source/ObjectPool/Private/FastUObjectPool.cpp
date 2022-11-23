#include "FastUObjectPool.h"

#define WITH_AUTO_SHRINK_POOL 1


FFastUObjectPool::FFastUObjectPool(TSubclassOf<UPooledObject> InClass):
	Class(InClass)
{
	ExitEvent = FCoreDelegates::OnPreExit.AddRaw(this, &FFastUObjectPool::DiscardUnusedObjects);
}

FFastUObjectPool::~FFastUObjectPool()
{
	FCoreDelegates::OnPreExit.Remove(ExitEvent);
	ensureMsgf(NumAllocated == 0, TEXT("you must recycle all object before destroying pool"));
	for (auto Obj:Objects)
	{
		Obj->Pool = nullptr;
		Obj->IsDiscard = true;
	}
}

UPooledObject* FFastUObjectPool::Create()
{
	NumAllocated++;

	if (Objects.Num() > 0)
	{
		auto Obj = Objects.Pop(false);
		Obj->IsInPool = false;
		return Obj;
	}

	auto Obj = NewObject<UPooledObject>(GetTransientPackage(), Class);
	ensure(Obj);
	NumGenerated++;

	Obj->Pool = this;

	return Obj;
}

void FFastUObjectPool::Destroy(UPooledObject* Obj)
{
	ensure(Obj->IsValidLowLevelFast() && !Obj->IsPendingKill());// if object is invalid , try to crash client now
	ensureMsgf(Obj->Pool == this, TEXT("Try to recycle uobject which is not created by this pool !!!"));
	ensure(!Obj->IsInPool);
	
	Obj->IsInPool = true;
	Objects.Push(Obj);

	--NumAllocated;
	ensure(NumAllocated >= 0);
}

void FFastUObjectPool::DiscardUnusedObjects()
{
	for (auto Object : Objects)
	{
		Object->IsDiscard = true;
	}
	NumGenerated = NumAllocated;
	Objects.Empty(NumAllocated);
}

void FFastUObjectPool::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_AUTO_SHRINK_POOL
	DiscardUnusedObjects();
#else
	for (auto Object : Objects)
	{
		Collector.AddReferencedObject(Object);
		ensureMsgf(Object, TEXT("this uobject is pending kill !"));
	}
#endif
}


UPooledObject* FGeneralUObjectPool::Create(TSubclassOf<UPooledObject> Class)
{
	auto Pool = Pools.Find(Class);
	if (!Pool)
	{
		Pools.Add(Class, MakeShared<FFastUObjectPool>(Class));
		Pool = Pools.Find(Class);
	}
	return (*Pool)->Create();
}

void FGeneralUObjectPool::Destroy(UPooledObject* Obj)
{
	auto Pool = Pools.Find(Obj->GetClass());
	ensureMsgf(Pool, TEXT("object can not be destroyed before creating.")); 
	(*Pool)->Destroy(Obj);
}


UPooledObject::~UPooledObject()
{
	// last protection
	ensure(IsReadyForFinishDestroy());
}

void UPooledObject::Recycle()
{
	ensureMsgf(Pool, TEXT("pool is destroyed."));
	Pool->Destroy(this);
}


bool UPooledObject::IsReadyForFinishDestroy()
{
	// allocated object cannot be destroyed
	return HasAnyFlags(RF_ClassDefaultObject) || (IsInPool && IsDiscard);
}

void UPooledObject::BeginDestroy()
{
	ensureMsgf(IsReadyForFinishDestroy(), TEXT("pooled object must be destryed in pool !"));
	Super::BeginDestroy();
}