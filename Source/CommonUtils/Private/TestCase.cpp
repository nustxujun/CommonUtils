#include "CommonUtils.h"

#define RUN_TEST 0

#if RUN_TEST
#pragma optimize("",off)

static int TestCase = []() {
    TArray<int> Array = {3,2,1};
    for (auto Item : XRange(Array))
    { 
        UE_LOG(LogTemp, Warning, TEXT("Index: %d, Value: %d"), Item.Key, Item.Value);        
    }

    for (auto i : XRange(100,1,-2))
    {
        UE_LOG(LogTemp, Warning, TEXT("Index: %d"), i);        
    }

    // visit a excel file
    auto ExcelFile = UExcelFile::CreateExcelFile(TEXT("G:/Test.xlsx"));
    auto Sheet = ExcelFile->AddSheet(TEXT("TestSheet"));
    ExcelFile->AddRow(Sheet, 1, {TEXT("Col1"), TEXT("Col2")});
    ExcelFile->Save();


    auto Actor = NewObject<AActor>();
    auto Wrapper = UPUObjectWrapper::Create(Actor, FPropertyFilters::NoFiltering);
    // access private member
    auto Prop = Wrapper->GetPropertyByName(TEXT("bCanBeDamaged"));
    bool bCanBeDamaged = Prop->GetValue<bool>();
    Prop->SetValue(!bCanBeDamaged);

    ensure(Actor->CanBeDamaged() == !bCanBeDamaged);

    return 0;
}();

#pragma optimize("",on)
#endif