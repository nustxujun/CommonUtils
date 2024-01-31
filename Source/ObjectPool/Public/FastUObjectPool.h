#pragma once 

#include "CoreMinimal.h"

#include "FastUObjectPool.generated.h"

class UPooledObject;

class OBJECTPOOL_API FFastUObjectPool :public FGCObject
{
public:
	FFastUObjectPool(TSubclassOf<UPooledObject> InClass);
	virtual ~FFastUObjectPool();

	UPooledObject* Create();
	void Destroy(UPooledObject* Obj);
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const {return "FastUObjectPool"; }

private:
	void DiscardUnusedObjects();
private:
	UClass* Class;
	FDelegateHandle ExitEvent;
	TArray<UPooledObject*> Objects;
	int NumAllocated = 0;
	int NumGenerated = 0;
};

class OBJECTPOOL_API FGeneralUObjectPool 
{
public:
	template<class T>
	T* Create()
	{
		return Cast<T>(Create(T::StaticClass()));
	}

	UPooledObject* Create(TSubclassOf<UPooledObject> Class);
	void Destroy(UPooledObject* Obj);

private:
	TMap<UClass*, TSharedPtr<FFastUObjectPool>> Pools;
};

template<class ObjectType>
class TCommonUObjectPool : public FFastUObjectPool
{
public:
	TCommonUObjectPool():FFastUObjectPool(ObjectType::StaticClass()) {};
};


UCLASS()
class OBJECTPOOL_API UPooledObject : public UObject
{
	GENERATED_BODY()

	friend class FFastUObjectPool;
public:
	virtual ~UPooledObject();
	void Recycle();
private:
	virtual bool IsReadyForFinishDestroy() override;
	virtual void BeginDestroy() override;

private:
	FFastUObjectPool* Pool = nullptr;
	bool IsInPool = false;
	bool IsDiscard = false;
};
