#pragma once 

/*
	example:
	
	// Construct Wrapper
	UPUObjectWrapper Wrapper(MyBlueprintObject);
	auto Property = Wrapper.GetPropertyByName(TEXT("BlueprintVariableStr"));

	if (!Property)
		return;

	// Value operation
	FString Content = Property->GetValue<FString>();
	Property->SetValue(Content + TEXT("_trail"));

	// UserDefinedStruct
	auto StructProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper.GetPropertyByName(TEXT("UserDefinedStructVariable")));
	auto Member = StructProp->GetPropertyByName("StructMember");
	Member->SetValue<int32>(1);

*/

#include "CoreMinimal.h"

#include "PropertyUtils.generated.h"

class UPUObjectWrapper;

class PROPERTYUTILS_API FPUProperty: public FGCObject
{
	friend class UPUObjectWrapper;
public:
	using Ptr = TSharedPtr<FPUProperty>;

	FPUProperty() = delete;
	FPUProperty(FPUProperty&) = delete;
	void operator=(FPUProperty) = delete;

public:
	FPUProperty(UPUObjectWrapper* InWrapper,const FProperty* Prop, void* ContPtr);
	virtual ~FPUProperty();
	virtual void* GetValuePtr()const;
	FString GetName()const;
	const FProperty* GetInternalProperty();

	template<class ValueType>
	ValueType GetValue()const
	{
		ensure(Property->ElementSize == sizeof(ValueType));
		return *static_cast<ValueType*>(GetValuePtr());
	}

	template<class ValueType>
	void SetValue(const ValueType& Value)
	{
		Property->CopyCompleteValue(ContentPtr, &Value);
	}

	void SetValue(void* Data)
	{
		Property->CopyCompleteValue(ContentPtr, Data);
	}

	template<class ValueType >
	operator ValueType() const
	{
		return GetValue<ValueType>();
	}

	template<class ValueType>
	void operator =(ValueType Value)
	{
		SetValue<ValueType>(MoveTemp(Value));
	}

	FString ExportAsString()const;
	void ImportByString(const FString& Text);

	virtual void Assign(Ptr Other);
private:
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const { return "FPUProperty"; }
protected:
	UPUObjectWrapper* Wrapper = nullptr; 
	const FProperty* Property = nullptr;
	void* ContentPtr = nullptr;
};

using PropertyContainer = TMap<FString, FPUProperty::Ptr>;

using PropertyFilter = TFunction<bool(const FProperty*)>;
class PROPERTYUTILS_API FPropertyFilters
{
public:
	const static PropertyFilter NoFiltering;
	const static PropertyFilter ScriptingPropertyOnly; // show the properties only created by blueprint, native properties will be hidden.
	const static PropertyFilter StructurePropertyOnly;
	const static PropertyFilter EditableOnly; // show editable properties 
};


class PROPERTYUTILS_API FPUPropertyRange
{
	TArray<FPUProperty::Ptr> Properties;
public:
	class Iterator
	{
		FPUPropertyRange* Properties;
		int32 Current;
	public:
		Iterator(FPUPropertyRange* P, int32 C):
			Properties(P), Current(C)
		{

		}
		FPUProperty::Ptr operator*()const
		{
			return (*Properties)[Current];
		}

		Iterator& operator++()
		{
			Current++;
			return *this;
		}

		bool operator != (const Iterator& A)const
		{
			return Current != A.Current;
		}
	};
public:
	FPUPropertyRange(class FPUStructWrapper* InContainer,const PropertyFilter& Filter = FPropertyFilters::NoFiltering);

	Iterator begin()
	{
		return { this, 0 };
	}

	Iterator end()
	{
		return { this, Properties.Num() };
	}

	FPUProperty::Ptr operator[](int32 Index)
	{
		return Properties[Index];
	}

};

class PROPERTYUTILS_API FPUStructWrapper
{
protected:
	void Initialize(const UStruct* Class, void* ContentPtr, class UPUObjectWrapper* InWrapper);
public:
	FPUProperty::Ptr GetPropertyByName(const FString& Name)const;
#if WITH_EDITOR
	PropertyContainer& GetProperties() { return Properties; };
#endif
	PropertyContainer& GetOriginProperties() { return OriginNameProperties; }
protected:
	const UStruct* Struct;
#if WITH_EDITOR
	PropertyContainer Properties;
#endif
	PropertyContainer OriginNameProperties;

};

UCLASS()
class PROPERTYUTILS_API UPUObjectWrapper : public UObject, public FPUStructWrapper
{
	GENERATED_BODY()
public:
	template<class ... Args>
	static UPUObjectWrapper* Create(Args ... args)
	{
		auto Wrapper = NewObject<UPUObjectWrapper>();
		Wrapper->Initialize(Forward<Args>(args) ...);
		return Wrapper;
	}
public:


public:
	UPUObjectWrapper();
	void Initialize(UObject* Object, PropertyFilter InFilter = FPropertyFilters::ScriptingPropertyOnly);
	void Initialize(const UStruct* Class, void* ContentPtr, PropertyFilter InFilter = FPropertyFilters::ScriptingPropertyOnly);
	void Initialize(const FProperty* Property, void* ContentPtr, PropertyFilter InFilter = FPropertyFilters::ScriptingPropertyOnly);


	bool DoFilter(const FProperty* Prop)const;

private:
	UObject* Object;
	PropertyFilter Filter;
};

class PROPERTYUTILS_API FPUBoolProperty :public FPUProperty
{
public:
	using Ptr = TSharedPtr<FPUBoolProperty>;
public:
	using FPUProperty::FPUProperty;
	virtual void* GetValuePtr()const override;
private:
	mutable bool bIsTrue;
};


class PROPERTYUTILS_API FPUStructProperty:public FPUProperty, public FPUStructWrapper
{
public:
	using Ptr = TSharedPtr<FPUStructProperty>;
public:
	FPUStructProperty(UPUObjectWrapper* InWrapper, const FProperty* Prop, void* Cont);
	virtual void Assign(FPUProperty::Ptr Other) override;

};

class PROPERTYUTILS_API FPUArrayProperty : public FPUProperty
{
public:
	using Ptr = TSharedPtr<FPUArrayProperty>;
public:
	FPUArrayProperty(UPUObjectWrapper* InWrapper, const FProperty* Prop, void* Cont);

	FPUProperty::Ptr GetElementProperty(int32 Index);
	void* GetElementPtr(int32 Index);
	void SetElementValue(int32 Index, void* Data);
	void Add();
	int32 Num();


	template<class ElementType>
	ElementType GetElementValue(int32 Index)
	{
		return *static_cast<ElementType*>(GetElementPtr(Index));
	}

	template<class ElementType>
	void SetElementValue(int32 Index, const ElementType& Value)
	{
		SetElementValue(Index, &Value);
	}


	template<class ElementType>
	void Add(const ElementType& Value)
	{
		Add();
		SetElementValue(Num() - 1, &Value);
	}


protected:
	FScriptArrayHelper ArrayHelper;
	const FProperty* ElementProperty;
};

class PROPERTYUTILS_API FPUMapProperty :public FPUProperty
{
public:
	using Ptr = TSharedPtr<FPUMapProperty>;
	FPUMapProperty(UPUObjectWrapper* InWrapper, const FProperty* Prop, void* Cont);

	TArray<TPair<void*, void*>> GetAllPropertyPtr();
	TArray<TPair<FPUProperty::Ptr, FPUProperty::Ptr>> GetAllProperties();

	const FProperty* GetKeyProperty() { return KeyProperty; }
	const FProperty* GetValueProperty() { return ValueProperty; }


	void* FindValuePtr(void* Key);

	template<class Key, class Value>
	Value* FindValue(const Key& InKey)
	{
		return static_cast<Value*>(FindValuePtr(&InKey));
	}

	template<class Key, class Value>
	TArray<TPair<Key, Value>> GetAllPropertyPairs()
	{
		TArray<TPair<Key, Value>> Result;
		for (auto& Item : GetAllPropertyPtr())
		{
			Result.Add(*static_cast<Key*>(Item.Key), *static_cast<Value*>(Item.Value));
		}
		return Result;
	}

private:
	FScriptMapHelper MapHelper;
	const FProperty* KeyProperty;
	const FProperty* ValueProperty;

};