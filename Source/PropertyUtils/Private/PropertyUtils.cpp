#include "PropertyUtils.h"
#if WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#endif

#define PARAMETER_LIST_WITH_TYPE UPUObjectWrapper* InWrapper,const FProperty* InProperty, void* InContentPtr
#define PARAMETER_LIST InWrapper, InProperty, InContentPtr

#define PROPERTY_FACTORY(Class) +[](PARAMETER_LIST_WITH_TYPE) { return FPUProperty::Ptr(new Class(PARAMETER_LIST)); }
#define ADD_FACTORY(Prop, Class) Factory(Prop::StaticClass(), PROPERTY_FACTORY(Class))

using Factory = TPair<FFieldClass*, TFunction<FPUProperty::Ptr(PARAMETER_LIST_WITH_TYPE)>>;

// Factories for PUProperty, Pay attention to the order of factories.
static TArray<Factory> PropertyFactories =
{
	ADD_FACTORY(FBoolProperty, FPUBoolProperty),
	ADD_FACTORY(FStructProperty, FPUStructProperty),
	ADD_FACTORY(FArrayProperty, FPUArrayProperty),
	ADD_FACTORY(FMapProperty, FPUMapProperty),

};

static FPUProperty::Ptr ConstructAceProperty(PARAMETER_LIST_WITH_TYPE)
{
	for (auto& Item : PropertyFactories)
	{
		if (InProperty->IsA(Item.Key))
		{
			return Item.Value(PARAMETER_LIST);
		}
	}

	return FPUProperty::Ptr(new FPUProperty(PARAMETER_LIST));
}

// Implemention of filters
const PropertyFilter FPropertyFilters::NoFiltering = +[](const FProperty*) {return true; };
const PropertyFilter FPropertyFilters::ScriptingPropertyOnly = [](const FProperty* Prop)
{
	return Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) &&
		!Prop->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic | CPF_NativeAccessSpecifierProtected | CPF_NativeAccessSpecifierPrivate); 
};
const PropertyFilter FPropertyFilters::StructurePropertyOnly = [](const FProperty* Prop)
{
	return Prop->IsA<FStructProperty>();
};
const PropertyFilter FPropertyFilters::EditableOnly = [](const FProperty* Prop)
{
	return Prop->HasAnyPropertyFlags(CPF_Edit)
#if WITH_EDITOR
		&& !Prop->HasAnyPropertyFlags(CPF_EditConst)
#endif
	;
};

FPUPropertyRange::FPUPropertyRange(FPUStructWrapper* InContainer, const PropertyFilter& Filter)
{
	for (auto& Prop : InContainer->GetOriginProperties())
	{
		if (!Filter(Prop.Value->GetInternalProperty()))
			continue;
		Properties.Add(Prop.Value);
	}
}

void FPUStructWrapper::Initialize(const UStruct* InStruct, void* ContentPtr, UPUObjectWrapper* InWrapper)
{
	OriginNameProperties.Empty();
	Struct = InStruct;

#if WITH_EDITOR
	Properties.Empty();
	auto UserStruct = Cast<UUserDefinedStruct>(Struct);
#endif

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		auto StructProp = *It;
		if (!InWrapper->DoFilter(StructProp))
			continue;
		auto Content = StructProp->ContainerPtrToValuePtr<void>(ContentPtr);
		auto PUProp = ConstructAceProperty(InWrapper, StructProp, Content);
		auto PropName = StructProp->GetName();
		OriginNameProperties.Add(PropName, PUProp);
#if WITH_EDITOR
		if (UserStruct)
		{
			auto Guid = FStructureEditorUtils::GetGuidForProperty(StructProp);
			auto Desc = FStructureEditorUtils::GetVarDescByGuid(UserStruct, Guid);
			Properties.Add(Desc->FriendlyName, PUProp);
		}
#endif
	}
}

FPUProperty::Ptr FPUStructWrapper::GetPropertyByName(const FString& Name)const
{
	auto Result = OriginNameProperties.Find(Name);
	if (Result)
		return *Result;
#if WITH_EDITOR
	Result = Properties.Find(Name);
	if (Result)
		return *Result;
#endif
	return {};
}



// Wrapper
UPUObjectWrapper::UPUObjectWrapper()
{

}



void UPUObjectWrapper::Initialize(UObject* Obj, PropertyFilter InFilter)
{
	Object = Obj;
	Filter = MoveTemp(InFilter);

	Initialize(Object->GetClass(), Object, Filter);

}

void UPUObjectWrapper::Initialize(const UStruct* Class, void* ContentPtr, PropertyFilter InFilter )
{
	Filter = MoveTemp(InFilter);
	FPUStructWrapper::Initialize(Class, ContentPtr, this);
}


void UPUObjectWrapper::Initialize(const FProperty* Property, void* ContentPtr, PropertyFilter InFilter )
{
	Filter = MoveTemp(InFilter);
	OriginNameProperties.Empty();
	OriginNameProperties.Add(Property->GetName(), ConstructAceProperty(this, Property, ContentPtr));
}


bool UPUObjectWrapper::DoFilter(const FProperty* Prop)const
{
	return Filter(Prop);
}


// Property


FPUProperty::FPUProperty(PARAMETER_LIST_WITH_TYPE):
	Wrapper(InWrapper), 
	Property(InProperty),
	ContentPtr(InContentPtr)
{
}

FPUProperty::~FPUProperty()
{
}

void* FPUProperty::GetValuePtr()const
{
	return ContentPtr;
}

FString FPUProperty::GetName()const
{
	return Property->GetName();
}

const FProperty* FPUProperty::GetInternalProperty()
{
	return Property;
}

FString FPUProperty::ExportAsString()const
{
	FString Result;
	Property->ExportTextItem(Result, ContentPtr, nullptr, nullptr, 0);
	return Result;
}

void FPUProperty::ImportByString(const FString& Text)
{
	Property->ImportText(*Text, ContentPtr, 0, nullptr);
}

void FPUProperty::Assign(Ptr Other)
{
	if (GetInternalProperty()->IsA(Other->GetInternalProperty()->GetClass()))
	{
		SetValue(Other->GetValuePtr());
	}
	else
	{
		// try by string
		ImportByString(Other->ExportAsString());
	}
}

void FPUProperty::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Wrapper);
}

void* FPUBoolProperty::GetValuePtr()const
{
	auto BoolProp = CastField<FBoolProperty>(Property);
	bIsTrue = BoolProp->GetPropertyValue(ContentPtr);
	return (void*) &bIsTrue;
}


FPUStructProperty::FPUStructProperty(PARAMETER_LIST_WITH_TYPE):
	FPUProperty(PARAMETER_LIST)
{
	ensure(InProperty->IsA<FStructProperty>());
	Initialize(CastField<FStructProperty>(InProperty)->Struct,InContentPtr,InWrapper);
}

void FPUStructProperty::Assign(FPUProperty::Ptr Other)
{
	auto OtherInternalProp = Other->GetInternalProperty();
	if (!OtherInternalProp->IsA(Property->GetClass()) && OtherInternalProp->IsA<FStructProperty>())
	{
		// they are different struct but we can assgining the property with same name
		auto StructProp = StaticCastSharedPtr<FPUStructProperty>(Other);
		for (auto Prop : OriginNameProperties)
		{
			auto OtherProp = StructProp->GetPropertyByName(Prop.Key);
			ensure(OtherProp);
			Prop.Value->Assign(OtherProp);
		}
	}
	else
	{
		FPUProperty::Assign(Other);
	}
}


FPUArrayProperty::FPUArrayProperty(PARAMETER_LIST_WITH_TYPE):
	FPUProperty(PARAMETER_LIST),
	ArrayHelper(CastField<FArrayProperty>(InProperty), InContentPtr)
{
	ensure(InProperty->IsA<FArrayProperty>());
	auto ArrayProp = CastField<FArrayProperty>(InProperty);
	ElementProperty = ArrayProp->Inner;
}

FPUProperty::Ptr FPUArrayProperty::GetElementProperty(int32 Index)
{
	return ConstructAceProperty(Wrapper, ElementProperty, GetElementPtr(Index));
}

void* FPUArrayProperty::GetElementPtr(int32 Index)
{
	ensure(ArrayHelper.IsValidIndex(Index));
	return ArrayHelper.GetRawPtr(Index);
}

void FPUArrayProperty::SetElementValue(int32 Index, void* Data)
{
	ElementProperty->CopyCompleteValue(GetElementPtr(Index), Data);
}

void FPUArrayProperty::Add()
{
	ArrayHelper.AddValue();
}

int32 FPUArrayProperty::Num()
{
	return ArrayHelper.Num();
}


FPUMapProperty::FPUMapProperty(PARAMETER_LIST_WITH_TYPE):
	FPUProperty(PARAMETER_LIST),
	MapHelper(CastField<FMapProperty>(InProperty), InContentPtr)
{
	ensure(InProperty->IsA<FMapProperty>());
	auto MapProp = CastField<FMapProperty>(InProperty);
	KeyProperty = MapProp->KeyProp;
	ValueProperty = MapProp->ValueProp;

}

void* FPUMapProperty::FindValuePtr(void* Key)
{
	auto Index = MapHelper.FindMapIndexWithKey(Key);
	return MapHelper.GetValuePtr(Index);
}


TArray<TPair<void*, void*>> FPUMapProperty::GetAllPropertyPtr()
{
	TArray<TPair<void*, void*>> Result;
	auto MaxIndex = MapHelper.GetMaxIndex();
	for (int Index = 0; Index < MaxIndex; ++Index)
	{
		if (!MapHelper.IsValidIndex(Index))
			continue;

		Result.Add( TPair<void*,void*>{ MapHelper.GetKeyPtr(Index), MapHelper.GetValuePtr(Index) });
	}
	return Result;
}


TArray<TPair<FPUProperty::Ptr, FPUProperty::Ptr>> FPUMapProperty::GetAllProperties()
{
	TArray<TPair<FPUProperty::Ptr, FPUProperty::Ptr>> Result;
	auto MaxIndex = MapHelper.GetMaxIndex();
	for (int Index = 0; Index < MaxIndex; ++Index)
	{
		if (!MapHelper.IsValidIndex(Index))
			continue;

		Result.Add(TPair<FPUProperty::Ptr, FPUProperty::Ptr>
		{ 
			ConstructAceProperty(Wrapper, KeyProperty, MapHelper.GetKeyPtr(Index)),
			ConstructAceProperty(Wrapper, ValueProperty, MapHelper.GetValuePtr(Index))
		});
	}

	return Result;
}

