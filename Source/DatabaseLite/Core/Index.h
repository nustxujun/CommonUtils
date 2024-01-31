#pragma once 

#include "CoreMinimal.h"
#include "File.h"	
#include "Any.h"

enum class EKeyType : uint8
{
	Invalid,
	Integer,
	String
};

using FKeyTypeSequence = TArray<EKeyType>;


struct FKeySequence
{
	TArray<FAny> Keys;

	FKeySequence() = default;

	FKeySequence(TArray<FAny> InKeys): Keys(MoveTemp(InKeys))
	{}

	template<class T>
	FKeySequence(T&& Value)
	{
		Add(Forward<T>(Value));
	}


	int Num() const { return Keys.Num(); }
	FAny& operator[](int Index)
	{
		return Keys[Index];
	}

	const FAny& operator[](int Index)const
	{
		return Keys[Index];
	}

	void Add(FString Str)
	{
		Keys.Add(MoveTemp(Str));
	}

	void Add(int32 Val)
	{
		Keys.Add((int64)Val);
	}

	void Add(int64 Val)
	{
		Keys.Add(Val);
	}

	void Add(const FName& Name)
	{
		Keys.Add(Name.ToString());
	}

	void Add(FAny Any)
	{
		Keys.Add(MoveTemp(Any));
	}

	static FKeyTypeSequence GetTypes(const TArray<FAny>& Keys)
	{
		FKeyTypeSequence Types;
		for (auto& Key : Keys)
		{
			if (Key.Type() == TTypeId<int64>())
				Types.Add(EKeyType::Integer);
			else if (Key.Type() == TTypeId<FString>())
				Types.Add(EKeyType::String);
			else
			{
				check(false);
			}
		}

		return Types;
	}
};

//using FKeySequence = TArray<FAny>;


class FBaseIndex
{
public:
	using Ptr = TSharedPtr<FBaseIndex>;
public:
	virtual ~FBaseIndex(){};
	virtual TArray<uint32> Find(int64 Key) = 0;
	virtual bool FindOne(int64 Key, const TFunction<bool(uint32)>& Callback) = 0;
	virtual void Insert(int64 Key, uint32 Data) = 0;
	virtual FString GetTypeName()const = 0;
};


template<class SeachType>
class TIndex: public FBaseIndex
{
public:
	TIndex(FFile::Ptr File):Seacher(File)
	{
		
	}

	void Init()
	{
		Seacher.Init();
	}

	void Open()
	{
		Seacher.Open();
	}

	virtual TArray<uint32> Find(int64 Key) override
	{
		return Seacher.Find(Key);
	}

	bool FindOne(int64 Key, const TFunction<bool(uint32)>& Callback)
	{
		return Seacher.FindOne(Key, Callback);
	}

	virtual void Insert(int64 Key, uint32 Data) override
	{
		Seacher.Insert(Key, Data);
	}	

	virtual FString GetTypeName()const
	{
		return Seacher.GetTypeName();
	}
private:
	SeachType Seacher;
};

class FIndexHelper
{
public:
	static bool Read(FKeySequence& Keys, FFile::Ptr File, const FKeyTypeSequence& Types);
	static bool Write(const FKeySequence& Keys, FFile::Ptr File, const FKeyTypeSequence& Types);
	static int32 GetKeySize(const FKeyTypeSequence& KeyTypes);
	static bool Equal(const FKeySequence& Keys1, const FKeySequence& Keys2);
	static uint32 Hash(const FKeySequence& Keys);

};