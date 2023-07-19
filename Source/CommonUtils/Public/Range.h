#pragma once
#include "CoreMinimal.h"

/*
* 
* example:
* 
*	// 0 ~ 100
*	for (auto i : XRange(100))
*	{
*	}
* 
*	// 50 ~90
*	for (auto i : XRange(50, 90))
*	{
*	}
* 
*	// 90 ~ 50
*	for (auto i : XRange(90, 50, -1))
*	{
*   }
* 
*/

template<class Iterator>
class TIteratorRange
{
public:
	TIteratorRange(Iterator From, Iterator To) :
		Begin(MoveTemp(From)), End(MoveTemp(To))
	{
	}

public:
	Iterator begin()const
	{
		return Begin;
	}

	Iterator end()const
	{
		return End;
	}
private:
	Iterator Begin;
	Iterator End;
};

template<class Type>
class TRangeIterator
{
public:
	TRangeIterator(Type InValue) :
		Value(MoveTemp(InValue))
	{
	}

	const Type& operator*()const
	{
		return Value;
	}

	bool operator != (const TRangeIterator& Other)const
	{
		return Value != Other.Value;
	}

	void operator ++()
	{
		++Value;
	}

public:
	Type Value;
};

template<class Type>
class TXRangeIterator
{
	using Operator = bool(TXRangeIterator::*)(const Type&)const;

	bool Less(const Type& Other)const
	{
		return Value < Other;
	}

	bool Greater(const Type& Other)const
	{
		return Value > Other;
	}


public:
	TXRangeIterator(Type InValue, Type InStep = {}) :
		Value(MoveTemp(InValue)), Step(MoveTemp(InStep))
	{
		if (InStep > Type{})
			Compare = &TXRangeIterator::Less;
		else
			Compare = &TXRangeIterator::Greater;
	}

	const Type& operator*()const
	{
		return Value;
	}

	bool operator != (const TXRangeIterator& Other)const
	{
		return (this->*Compare)(Other.Value);
	}

	void operator ++()
	{
		Value += Step;
	}

public:
	Type Value;
	Type Step;
	Operator Compare;
};

template<class T>
TIteratorRange<TRangeIterator<T>> XRange(T To)
{
	return { 0, MoveTemp(To) };
}


template<class T>
TIteratorRange<TRangeIterator<T>> XRange(T From, T To)
{
	return { MoveTemp(From), MoveTemp(To) };
}


template<class T>
TIteratorRange<TXRangeIterator<T>> XRange(T From, T To, T Step)
{
	return { { MoveTemp(From), MoveTemp(Step) }, { MoveTemp(To) } };
}

/*
* example:
* 
*	TArray<FString> StringList = {"1", "2"};
*	for (auto Elem : XRange(StringList))
*	{
*		auto Index = Elem.Key;
*		auto Str = Elem.Value;
*	}
*/

template<class T>
class TArrayRangeIterator
{
public:
	TArrayRangeIterator(const TArray<T>& InArray, size_t InIndex) :
		Array(InArray),Index(InIndex)
	{
	}

	TPair<size_t, const T&> operator*()const
	{
		return TPair<size_t, const T&>{ Index , Array[Index] };
	}

	bool operator !=(const TArrayRangeIterator& Other)
	{
		return Index != Other.Index;
	}

	void operator++()
	{
		Index++;
	}

private:
	const TArray<T>& Array;
	size_t Index = 0;
};

template<class T>
class TNativeArrayRangeIterator
{
public:
	TNativeArrayRangeIterator(const T* InArray, size_t InIndex) :
		Array(InArray), Index(InIndex)
	{
	}

	TPair<size_t, const T&> operator*()const
	{
		return TPair<size_t, const T&>{ Index, Array[Index] };
	}

	bool operator !=(const TNativeArrayRangeIterator& Other)
	{
		return Index != Other.Index;
	}

	void operator++()
	{
		Index++;
	}

private:
	const T* Array;
	size_t Index = 0;
};


template<class T>
TIteratorRange<TArrayRangeIterator<T>> XRange(const TArray<T>& InArray)
{
	return { {InArray, 0}, {InArray, (size_t)InArray.Num()} };
}

template<class T, size_t Num>
TIteratorRange<TNativeArrayRangeIterator<T>> XRange(const T(&InArray)[Num])
{
	return { {InArray, 0}, {InArray, Num} };
}