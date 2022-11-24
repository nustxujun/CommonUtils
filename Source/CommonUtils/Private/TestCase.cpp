#include "CommonUtils.h"

#define RUN_TEST 0 && WITH_EDITOR

#if RUN_TEST
#pragma optimize("", off)

static int TestCase = []()
{
    // XRange
    {
        TArray<int> Array = {3, 2, 1};
        for (auto Item : XRange(Array))
        {
            UE_LOG(LogTemp, Warning, TEXT("Index: %d, Value: %d"), Item.Key, Item.Value);
        }

        for (auto i : XRange(100, 1, -2))
        {
            UE_LOG(LogTemp, Warning, TEXT("Index: %d"), i);
        }
    }

    // Excel Utils
    auto ExcelFile = UExcelFile::CreateExcelFile(TEXT("G:/Test.xlsx"));
    auto Sheet = ExcelFile->AddSheet(TEXT("TestSheet"));
    ExcelFile->AddRow(Sheet, 1, {TEXT("Col1"), TEXT("Col2")});
    ExcelFile->Save();

    // Property Utils
    auto Actor = NewObject<AActor>();
    auto Wrapper = UPUObjectWrapper::Create(Actor, FPropertyFilters::NoFiltering);
    // access private member
    auto Prop = Wrapper->GetPropertyByName(TEXT("bCanBeDamaged"));
    bool bCanBeDamaged = Prop->GetValue<bool>();
    Prop->SetValue(!bCanBeDamaged);

    ensure(Actor->CanBeDamaged() == !bCanBeDamaged);

    // Object Pool
    {
        auto Pool = FNormalObjectPool<int>();
        TArray<int *> Array = {
            Pool.Create(),
            Pool.Create(),
        };
        for (auto i: Array)
            Pool.Destroy(i);
    }

    {
        auto Pool = FFixedObjectPool<int, 1>();
        TArray<int*> Array = {
            Pool.Create(),
            Pool.Create(),
        };
        for (auto i : Array)
            Pool.Destroy(i);
    }

    {
        auto Pool = FFlatObjectPool<int>();
        TArray<int*> Array = {
            Pool.Create(),
            Pool.Create(),
        };
        for (auto i : Array)
            Pool.Destroy(i);
    }

    {
        // auto shrink
        auto Pool = FFlatObjectPool<int,1, true>();
        TArray<int*> Array = {
            Pool.Create(),
            Pool.Create(),
        };
        for (auto i : Array)
            Pool.Destroy(i);
    }
    return 0;
}();

#pragma optimize("", on)
#endif