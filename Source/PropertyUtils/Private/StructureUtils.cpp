#include "StructureUtils.h"
#include "PropertyUtils.h"

#pragma optimize("",off)
void UPUStructureUtils::Generic_GetStructurePropertyNames(TArray<FString>& OutNames, const void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, const_cast<void*>(StructAddr));

	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));

	for (auto Prop : PUProp->GetOriginProperties())
	{
		OutNames.Add(Prop.Key);
	}
}

void UPUStructureUtils::Generic_GetStructurePropertiesAsString(TArray<FString>& OutValues, const void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, const_cast<void*>(StructAddr));

	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));

	for (auto Prop : PUProp->GetOriginProperties())
	{
		FString Value;
		Prop.Value->GetInternalProperty()->ExportTextItem(Value, Prop.Value->GetValuePtr(),nullptr,nullptr,0);
		OutValues.Add(Value);
	}
}




void UPUStructureUtils::Generic_GetStructurePropertyByName(const FString& Name, void* ValueAddr, const FProperty* ValueProperty, const void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, const_cast<void*>(StructAddr));
	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));
	auto Prop = PUProp->GetPropertyByName(Name);
	if (!Prop)
		return;
	auto InternalProp = Prop->GetInternalProperty();
	ensureMsgf(InternalProp->IsA( ValueProperty->GetClass()), TEXT("GetStructurePropertyByName check value type failed,"));

	ValueProperty->CopyCompleteValue(ValueAddr, Prop->GetValuePtr());
}


void UPUStructureUtils::Generic_GetStructurePropertyByNameAsString(const FString& Name,FString& OutValue, const void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, const_cast<void*>(StructAddr));
	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));
	auto Prop = PUProp->GetPropertyByName(Name);
	if (!Prop)
		return;
	auto InternalProp = Prop->GetInternalProperty();
	InternalProp->ExportTextItem(OutValue, Prop->GetValuePtr(), NULL, NULL, 0);
}

void UPUStructureUtils::Generic_SetStructurePropertyByName(const FString& Name, const void* ValueAddr, const FProperty* ValueProperty, void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, StructAddr);
	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));
	auto Prop = PUProp->GetPropertyByName(Name);
	if (!Prop)
		return;
	auto InternalProp = Prop->GetInternalProperty();
	ensureMsgf(InternalProp->IsA(ValueProperty->GetClass()), TEXT("SetStructurePropertyByName check value type failed,"));

	Prop->SetValue(ValueAddr);
}


void UPUStructureUtils::Generic_SetStructurePropertyByNameAsString(const FString& Name, const FString& InValue, void* StructAddr, const FStructProperty* StructProperty)
{
	auto Wrapper = UPUObjectWrapper::Create(StructProperty, StructAddr);
	auto PUProp = StaticCastSharedPtr<FPUStructProperty>(Wrapper->GetPropertyByName(StructProperty->GetName()));
	auto Prop = PUProp->GetPropertyByName(Name);
	if (!Prop)
		return;

	auto InternalProp = Prop->GetInternalProperty();
	InternalProp->ImportText(*InValue, Prop->GetValuePtr(), 0, NULL);
}
#pragma optimize("",on)
