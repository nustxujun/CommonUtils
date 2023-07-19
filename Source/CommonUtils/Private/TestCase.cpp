#include "CommonUtils.h"
#include "Misc/AutomationTest.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXRangeTest, "CommonUtils.XRange", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
#define CHECK_RESULT(expr) if ((expr) == false) return false;

bool FXRangeTest::RunTest(const FString& Parameters)
{

    {
        int a = 0;
        int Count = 10;
        for (auto Index : XRange(Count))
        {
            a++;
        }

        if (a != Count)
            return false;
    }

    {
        int a = 0;
        int Count = 10;
        for (auto Index : XRange(20,0,-2))
        {
            a++;
        }

        if (a != Count)
            return false;
    }

    {
        TArray<int> Array = { 0,1,2 };
        for (auto Item : XRange(Array))
        {
            if (Item.Key != Item.Value)
                return false;
        }

    }

    {
        int Array[] = {0,1,2};
        for (auto Item : XRange(Array))
        {
            if (Item.Key != Item.Value)
                return false;
        }

    }

    return true;
};


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExcelUtilTest, "CommonUtils.ExcelUtil", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExcelUtilTest::RunTest(const FString& Parameters)
{
    // Excel Utils
    auto ExcelFile = UExcelFile::CreateExcelFile(TEXT("Test.xlsx"));
    auto Sheet = ExcelFile->AddSheet(TEXT("TestSheet"));
    ExcelFile->AddRow(Sheet, 1, {TEXT("Col1"), TEXT("Col2")});
    ExcelFile->Save();

    return true;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyUtilTest, "CommonUtils.PropertyUtil", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPropertyUtilTest::RunTest(const FString& Parameters)
{
    auto Actor = NewObject<AActor>();
    auto Wrapper = UPUObjectWrapper::Create(Actor, FPropertyFilters::NoFiltering);
    // access private member
    auto Prop = Wrapper->GetPropertyByName(TEXT("bCanBeDamaged"));
    bool bCanBeDamaged = Prop->GetValue<bool>();
    Prop->SetValue(!bCanBeDamaged);

    return Actor->CanBeDamaged() == !bCanBeDamaged;
};



struct CastTest
{
    template<class T>
    bool Test(const T& Value)
    {
        FAny Any = Value;
        return AnyCast<T>(Any) == Value;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnyTest, "CommonUtils.Any", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAnyTest::RunTest(const FString& Parameters)
{
    FAny Any;
    if (!Any.IsNone())
        return false;

    CastTest Test;
    CHECK_RESULT(Test.Test<int>(1));
    CHECK_RESULT(Test.Test<const char*>("1"));
    CHECK_RESULT(Test.Test<FString>(TEXT("1")));

    auto Lambda = [](){
        return 1;
    };
    Any = Lambda;
    CHECK_RESULT(TTypeId<decltype(Lambda)>() == Any.Type());


    return true;
};


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPromiseTest, "CommonUtils.Promise", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPromiseTest::RunTest(const FString& Parameters)
{
	static int stepcount = 1;
	{
		{
			auto A = FPromise::New();
			A.Resolve();
			auto B = A;
			B.Then([]() { });
		}


		auto step1 = FPromise::New();
		auto step2 = FPromise::New();
		auto step3 = FPromise::New();
		auto step4 = FPromise::New();
		auto step5 = FPromise::New();

		step1.Then([step2, step3, step4, step5]() {
			ensure(stepcount++ == 1);
			step2.Then(step3);
			step3.Then([step4]() {
				ensure(stepcount++ == 3);
				return step4;
			}).Then([step5]() {
				ensure(stepcount++ == 4);
				return step5;
			});
			return step2;
		}).Then([]() {
			ensure(stepcount++ == 2);
		}).Then([]() {
		ensure(stepcount++ == 5);
		});

		step5.Resolve();
		step4.Resolve();
		step3.Resolve();
		step2.Resolve();
		step1.Resolve();

		CHECK_RESULT(stepcount == 6);
	}

	{
		auto step1 = FPromise::New();
		auto step2 = FPromise::New();

		static int stepcount2 = 1;


		step1.Then([]() {});
		step1.Resolve();
		step2.Then([]() {
			stepcount2++;
			});
		step1.Then(step2);

		step2.Resolve();


		CHECK_RESULT(stepcount2 == 2);
	}
	{
		auto step1 = FPromise::New();
		auto step2 = FPromise::New();

		bool ret = 0;

		step1.Then([step2]() { return step2; })
			.Then([&ret]() { ret = 1; });

		step1.Resolve();
		step1.Close();
		step2.Resolve();

		CHECK_RESULT(ret == 0);
	}

	{
		auto step1 = FPromise::New();
		auto step2 = FPromise::New();

		bool ret = 0;

		step1.Then([]() {});
		step1.Resolve();
		step2.Then([&ret]() { ret = 1; });
		step1.Then(step2);

		step2.Resolve();

		CHECK_RESULT(ret == 1);
	}

	{
		auto step1 = FPromise::New();
		bool ret = 0;
		{
			auto step2 = FPromise::New();
			step2.Then([step2]() {
				ensure(step2.IsValid());
				});
			step1.Then([]() {});
			step1.Then(step2);
			step2.Resolve();
		}

		step1.Resolve();

		CHECK_RESULT(ret == 0);
	}

	{
		int idx = 0;
		auto step1 = FPromise::New().Then([&idx]() {
			idx = 1;
			});
		auto step2 = FPromise::New().Then([&idx]() {
			idx = 2;
			});

		FPromise::Race({ step1, step2 });

		step2.Resolve();
		step1.Resolve();

		CHECK_RESULT(idx == 2);

	}

	return true;
}
