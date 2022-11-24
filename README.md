

	Personal utils plugin for UE

## Features
- [x] [XRange](#XRange)
- [x] [Property Utils](#PropertyUtils)
- [x] [Excel Utils](#ExcelUtils)
- [x] [Lua Utils](#LuaUtils)
- [x] [Object Pool](#ObjectPool)


XRange
--
Python like
```c++
	for (auto i : XRange(1))
	{
		UE_LOG(LogTemp, Warning, TEXT("Index: %d"), i);        

	}

	for (auto i : XRange(1,100))
	{
		UE_LOG(LogTemp, Warning, TEXT("Index: %d"), i);        

	}

	for (auto i : XRange(100,1, -2))
	{
		UE_LOG(LogTemp, Warning, TEXT("Index: %d"), i);        

	}

	TArray<int> Array = {3,2,1};
	for (auto Item : XRange(Array))
	{
		UE_LOG(LogTemp, Warning, TEXT("Index: %d, Value: %d"), Item.Key, Item.Value);        
	}
```

PropertyUtils
--
Help for reading and writing properties of UObject (Include Blueprint)
```c++
	auto Actor = NewObject<AActor>();
	auto Wrapper = UPUObjectWrapper::Create(Actor, FPropertyFilters::NoFiltering);
	// access private member
	auto Prop = Wrapper->GetPropertyByName(TEXT("bCanBeDamaged"));
	bool bCanBeDamaged = Prop->GetValue<bool>();
	Prop->SetValue(!bCanBeDamaged);

	ensure(Actor->CanBeDamaged() == !bCanBeDamaged);
```

ExcelUtils
--
Help for reading and writing excel files(.xlsx) in c++ and blueprint
```c++
	auto ExcelFile = UExcelFile::CreateExcelFile(TEXT("G:/Test.xlsx"));
	auto Sheet = ExcelFile->AddSheet(TEXT("TestSheet"));
	ExcelFile->AddRow(Sheet, 1, {TEXT("Col1"), TEXT("Col2")});
	ExcelFile->Save();
```

LuaUtils
--

- Auto add scope timer for every lua function, make lua profiling easier in Insight.

ObjectPool
--
Help to reduce the memory allocation


- ``FFastUObjectPool``, a pool used for UObject
- ``FNormalObjectPool``, a dynamic size pool which can be collected by gc , good for memory usage
- ``FFixedObjectPool``, a fixed size pool , cache friendy
- ``FFlatObjectPool``, a balance between cache friendly and memory usage