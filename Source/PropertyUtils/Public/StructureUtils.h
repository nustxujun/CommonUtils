#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructureUtils.generated.h"




UCLASS()
class PROPERTYUTILS_API UPUStructureUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// GetStructurePropertyNames
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "CustomStruct"))
	static void GetStructurePropertyNames(TArray<FString>& OutNames,  const int32& CustomStruct);
	static void Generic_GetStructurePropertyNames(TArray<FString>& OutNames, const void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execGetStructurePropertyNames) {

		P_GET_TARRAY_REF(FString, OutNames);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;

		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_GetStructurePropertyNames(OutNames, StructAddr, StructProperty);
		P_NATIVE_END;
	}

	// GetStructurePropertiesAsString
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "CustomStruct"))
	static void GetStructurePropertiesAsString(TArray<FString>& OutValues, const int32& CustomStruct);
	static void Generic_GetStructurePropertiesAsString(TArray<FString>& OutValues, const void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execGetStructurePropertiesAsString) {

		P_GET_TARRAY_REF(FString, OutValues);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;

		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_GetStructurePropertiesAsString(OutValues, StructAddr, StructProperty);
		P_NATIVE_END;
	}


	// GetStructurePropertyByName
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "OutValue,CustomStruct"))
	static void GetStructurePropertyByName(const FString& Name,int32& OutValue, const int32& CustomStruct);
	static void Generic_GetStructurePropertyByName(const FString& Name, void* ValueAddr, const FProperty* ValueProperty ,const void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execGetStructurePropertyByName) {

		P_GET_STRUCT_REF(FString, Name);


		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(NULL);
		void* ValueAddr = Stack.MostRecentPropertyAddress;
		FProperty* ValueProperty = CastField<FProperty>(Stack.MostRecentProperty);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_GetStructurePropertyByName(*Name, ValueAddr, ValueProperty ,StructAddr, StructProperty);
		P_NATIVE_END;
	}

	// GetStructurePropertyByNameAsString
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "OutValue,CustomStruct"))
	static void GetStructurePropertyByNameAsString(const FString& Name, const int& OutValue, const int32& CustomStruct);
	static void Generic_GetStructurePropertyByNameAsString(const FString& Name,FString& OutValue, const void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execGetStructurePropertyByNameAsString) {

		P_GET_STRUCT_REF(FString, Name);
		P_GET_STRUCT_REF(FString, OutValue);


		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_GetStructurePropertyByNameAsString(*Name, OutValue,StructAddr, StructProperty);
		P_NATIVE_END;
	}

	// SetStructurePropertyByName
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "InValue,CustomStruct"))
	static void SetStructurePropertyByName(const FString& Name, const int32& InValue,const int32& CustomStruct);
	static void Generic_SetStructurePropertyByName(const FString& Name,const void* ValueAddr, const FProperty* ValueProperty, void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execSetStructurePropertyByName) {

		P_GET_STRUCT_REF(FString, Name);


		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(NULL);
		void* ValueAddr = Stack.MostRecentPropertyAddress;
		FProperty* ValueProperty = CastField<FProperty>(Stack.MostRecentProperty);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_SetStructurePropertyByName(*Name, ValueAddr, ValueProperty, StructAddr, StructProperty);
		P_NATIVE_END;
	}


	// SetStructurePropertyByNameAsString
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "PUStructureUtils", meta = (CustomStructureParam = "CustomStruct"))
		static void SetStructurePropertyByNameAsString(const FString& Name, const FString& InValue, const int32& CustomStruct);
	static void Generic_SetStructurePropertyByNameAsString(const FString& Name,const FString& InValue,  void* StructAddr, const FStructProperty* StructProperty);

	DECLARE_FUNCTION(execSetStructurePropertyByNameAsString) {

		P_GET_STRUCT_REF(FString, Name);
		P_GET_STRUCT_REF(FString, InValue);


		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FStructProperty>(NULL);
		void* StructAddr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);


		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_SetStructurePropertyByNameAsString(*Name, *InValue, StructAddr, StructProperty);
		P_NATIVE_END;
	}

};