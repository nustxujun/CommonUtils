#pragma once 
#include "CoreMinimal.h"

#if defined(_MSC_VER) && !defined(__clang__)
#define UNIQUE_FUNCTION_ID __FUNCSIG__
#else
#define UNIQUE_FUNCTION_ID __PRETTY_FUNCTION__
#endif

template<class T>
inline uint64 TTypeId()
{
	static uint64 Id = (uint64)FCrc::MemCrc32(UNIQUE_FUNCTION_ID, sizeof(UNIQUE_FUNCTION_ID));
	return Id;
}

template<class T>
struct TRemoveCVRef {
	typedef typename TRemoveCV<typename TRemoveReference<T>::Type>::Type Type;
};


class FAny;
template<class T>
inline T AnyCast(const FAny&);

class FAny
{
public:
	FAny(){}
	FAny(const FAny& Other): Holder(Other.Holder ? Other.Holder->Clone(): nullptr){}
	FAny(FAny&& Other) : Holder(Other.Holder) { Other.Holder = nullptr; }


	template<class ValueType>
	FAny(const ValueType& Value) : Holder(new PlaceHolder<typename TRemoveCVRef<ValueType>::Type>(Value))
	{
	}

	template<class ValueType>
	FAny(ValueType&& Value, typename TEnableIf<!TIsSame<FAny&, ValueType>::Value>::Type* = nullptr) : Holder(new PlaceHolder<typename TRemoveCVRef<ValueType>::Type>(MoveTemp(Value)))
	{
	}


	~FAny()
	{
		if (Holder) delete Holder;
	}

	FAny& Swap(FAny& RHS)
	{
		::Swap(Holder, RHS.Holder);
		return *this;
	}

	FAny& operator= (const FAny& RHS)
	{
		FAny(RHS).Swap(*this);
		return *this;
	}

	template<class ValueType>
	FAny& operator= (typename TRemoveCVRef<ValueType>::Type Value)
	{
		FAny(MoveTemp(Value)).Swap(*this);
		return *this; 
	}

	template<class ValueType>
	ValueType Cast()const
	{
		return AnyCast<ValueType>(*this);
	}

	uint64 Type()const
	{
		return Holder ? Holder->Type()  : 0;
	}

	bool IsNone()const
	{
		return Holder == nullptr;
	}

public:
	class PlaceHolderBase
	{
	public:
		virtual ~PlaceHolderBase(){}

		virtual uint64 Type()const = 0;
		virtual PlaceHolderBase* Clone()const = 0;
	};

	template<class ValueType>
	class PlaceHolder : public PlaceHolderBase
	{
	public:
		PlaceHolder(ValueType InValue):
			Value(MoveTemp(InValue))
		{}

		virtual uint64 Type()const override 
		{ 
			return TTypeId<ValueType>(); 
		}

		virtual PlaceHolderBase* Clone()const override { return new PlaceHolder(Value);}

	public:
		ValueType Value;

	};

public:
	PlaceHolderBase* Holder = nullptr;
};

template<class T>
inline T AnyCast(FAny& Operand)
{
	typedef typename TRemoveCVRef<T>::Type Type;
	checkf(TTypeId<Type>() == Operand.Type(), TEXT("Bad any cast!"));
	return static_cast<FAny::PlaceHolder<Type>*>(Operand.Holder)->Value;
}

template<class T>
inline T AnyCast(const FAny& Operand)
{
	return AnyCast<T>(const_cast<FAny&>(Operand));
}