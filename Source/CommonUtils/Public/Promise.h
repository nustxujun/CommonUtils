#pragma once 
#include "CoreMinimal.h"
#include "Any.h"
#include <memory>
#include <tuple>
#include <functional>

enum class EPromiseState
{
	Pending,
	Resolved,
	Rejected
};

class FPromise
{
	struct FPromiseSharedObject
	{
		using Ptr = std::shared_ptr<FPromiseSharedObject>;
		struct FHolder
		{
			using Ptr = std::shared_ptr<FHolder>;

			FAny OnResolved;
			std::function<void(int)> OnRejected;
			std::function<void()> Solver;

			Ptr Next;
			bool bValid = true;
			bool bClosed = false;
			bool bBlocking = false;
			bool bCallbackOnly = false;

			bool IsBound()const
			{
				return OnRejected || !OnResolved.IsNone();
			}

			void Clear()
			{
				bValid = false;

				OnResolved = {};
				OnRejected = {};
				Solver = {};
			}

			void Call()
			{
				if (!bValid || bClosed || bBlocking || !IsBound() || !Solver )
					return;

				Solver();
			}

			void Close()
			{
				bClosed = true;
				if (Next)
					Next->Close();
				Next.reset();
				//Clear();
			}
		};


		FHolder::Ptr Holder;
		EPromiseState State = EPromiseState::Pending;

		void AddTask(FAny OnResolved, std::function<void(int)>&& OnRejected)
		{
			if (Holder->IsBound())
			{
				auto Cur = Holder;
				while (Cur->Next)
				{
					Cur = Cur->Next;

					if (!Cur->IsBound())
					{
						Cur->OnResolved = std::move(OnResolved);
						Cur->OnRejected = std::move(OnRejected);
						return;
					}
				}

				auto NewNode = std::make_shared<FHolder>();
				NewNode->bCallbackOnly = true;
				NewNode->bBlocking = true;

				Cur->Next = NewNode;
				
				NewNode->OnResolved = std::move(OnResolved);
				NewNode->OnRejected = std::move(OnRejected);
			}
			else
			{
				Holder->OnResolved = std::move(OnResolved);
				Holder->OnRejected = std::move(OnRejected);
			}
		}
		
		void AddSolver(std::function<void()>&& Solver)
		{
			Holder->Solver = (std::move(Solver));
		}

		void Reject(int Code)
		{
			if (Holder->OnRejected)
				Holder->OnRejected(Code);
		}

		void Reset()
		{
			auto Temp = Holder;
			auto Next = Holder->Next;

			if (Next)
			{
				Next->bBlocking = false;
				Holder = Next;
			}

			Temp->Next.reset();
			Temp->Clear();
		}

		template<class ... ParameterType>
		void Resolve(const ParameterType& ... Params)
		{
			/*
				resolved callback can be two form : 1. return nothing 2. return a promise
				1. normally execute callback.
				2. after executing callback, we will join then promise from return value

				take care about the function type matching, it is very strict.
			*/
			using FuncType = std::function<void(const ParameterType& ...)>;
			using FuncTypeWithPromise = std::function<FPromise(const ParameterType& ...)>;
			using FuncTypeNoParams = std::function<void(void)>;
			using FuncTypeNoParamsWithPromise = std::function<FPromise(void)>;

			
			//FAny OnResolved = FuncType();
			checkf(!Holder->OnResolved.IsNone(), TEXT("onresolve has not been given, but promise is resolved."))
			auto const Type = Holder->OnResolved.Type();
			if (Type == TTypeId<FuncType>())
			{
				auto& Func = Holder->OnResolved.Cast<FuncType&>();
				if (Func)
					Func(Params ...);

			}
			else if (Type == TTypeId<FuncTypeWithPromise>())
			{
				FPromise NextPromise;
				auto& Func = Holder->OnResolved.Cast<FuncTypeWithPromise&>();
				if (Func)
					NextPromise = Func(Params ...);

				if (NextPromise.IsValid())
				{
					Join(NextPromise.SharedObject, false);
				}
			}
			else if (Type == TTypeId<FuncTypeNoParams>())
			{
				auto& Func = Holder->OnResolved.Cast<FuncTypeNoParams&>();
				if (Func)
					Func();
			}
			else if (Type == TTypeId<FuncTypeNoParamsWithPromise>())
			{
				FPromise NextPromise;
				auto& Func = Holder->OnResolved.Cast<FuncTypeNoParamsWithPromise&>();
				if (Func)
					NextPromise = Func();

				if (NextPromise.IsValid())
				{
					Join(NextPromise.SharedObject, false);
				}

			}
			else
				ensureAlwaysMsgf(false, TEXT("type of parameters is not matched, only support const or tempore variables"));

			auto Next = Holder->Next;
			if (Next)
			{
				Next->bBlocking = false;
				Next->Call();
			}
			Reset();
		}

		void Call()
		{
			if (Holder)
				Holder->Call();
		}

		void Join(Ptr Other, bool bAddTail)
		{
			if (!Holder->bValid)
			{
				Holder = Other->Holder;
				return;
			}

			Other->Holder->bBlocking = true;

			auto Prev = Holder;
			if (bAddTail)
			{
				while (Prev->Next)
				{
					Prev = Prev->Next;
				}
			}

			auto OtherHolder = Other->Holder;
			auto Next = Prev->Next;

			do 
			{
				if (!Next)
				{
					Prev->Next = OtherHolder;
					break;
				}
				else if (Next->IsBound() && Next->bCallbackOnly && !OtherHolder->IsBound())
				{
					OtherHolder->OnResolved = std::move(Next->OnResolved);
					OtherHolder->OnRejected = std::move(Next->OnRejected);

					auto Temp = OtherHolder->Next;

					Prev->Next = OtherHolder;
					OtherHolder->Next = Next->Next;

					Prev = OtherHolder;
					Next = Next->Next;

					OtherHolder = Temp;
				}
				else
				{
					auto Temp = OtherHolder->Next;
					Prev->Next = OtherHolder;
					OtherHolder->Next = Next;

					Prev = OtherHolder;
					OtherHolder = Temp;
				}
				
			}
			while(OtherHolder);
		}
	};

public:
	static FPromise New()
	{
		FPromise Promise;
		Promise.SharedObject = std::make_shared<FPromiseSharedObject>();
		Promise.SharedObject->Holder = std::make_shared<FPromiseSharedObject::FHolder>();

		return Promise;
	}

	static void Race(const TArray<FPromise>& List)
	{
		auto Promise = New();
		Promise.Then(std::function<void()>([List]()mutable
		{
			for (auto& P : List)
			{
				P.Close();
			}
		}));
		for (auto& P : List)
		{
			P.Then(Promise);
		}
		Promise.Resolve();
	}


	template<class ... ParameterType>
	void Resolve(ParameterType&& ... Params) const
	{
		if (!IsValid())
			return;

		checkf(SharedObject->State == EPromiseState::Pending, TEXT("you can not resolve a promise which is resolved or rejected."));
		SharedObject->State = EPromiseState::Resolved;

		SharedObject->AddSolver([Promise = *this, Parameter = std::make_tuple(std::forward<ParameterType>(Params) ...)]() {
			Promise.Call(Parameter, std::make_index_sequence < std::tuple_size<decltype(Parameter)>{} > {});
		});

		SharedObject->Call();
	}

	void Reject(int Code)const
	{
		if (!IsValid())
			return;

		checkf(SharedObject->State == EPromiseState::Pending, TEXT("you can not reject a promise which is resolved or rejected."));
		SharedObject->State = EPromiseState::Rejected;

		SharedObject->AddSolver([SharedObject = SharedObject, Code](){
			SharedObject->Reject(Code);
		});

		SharedObject->Call();
	}



	/*
		we use std::function store the functor, in order to traits the function type
		inheritance type is not supported to be parameter.
	*/

	template <typename>
	struct ClosureTraits;

	template <typename FunctionT> 
	struct ClosureTraits : ClosureTraits<decltype(&std::remove_reference_t<FunctionT>::operator())> { };

	template <typename ReturnTypeT, typename... Args> 
	struct ClosureTraits<ReturnTypeT(Args...)> 
	{
		using FunctionType = std::function<ReturnTypeT(const Args& ...)>; 

		template<class F>
		static FunctionType GenFunction(F Func)
		{
			return [Func = std::move(Func)](const Args& ... args)->ReturnTypeT {
				return Func(args...);
			};
		}
	};

	template <typename ReturnTypeT, typename... Args> 
	struct ClosureTraits<ReturnTypeT(*)(Args...)> : ClosureTraits<ReturnTypeT(Args...)> { };

	template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
	struct ClosureTraits<ReturnTypeT(ClassTypeT::*)(Args...)> : ClosureTraits<ReturnTypeT(Args...)> { using class_type = ClassTypeT; };

	template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
	struct ClosureTraits<ReturnTypeT(ClassTypeT::*)(Args...) const> : ClosureTraits<ReturnTypeT(ClassTypeT::*)(Args...)> { };

	FPromise Then(FPromise Promise) const
	{
		if (!SharedObject || !SharedObject->Holder || SharedObject->Holder->bClosed)
		{
			Promise.Close();
			return *this;
		}

		SharedObject->Join(Promise.SharedObject, true);
		SharedObject->Call();
		return *this;
	}

	template<class F>
	FPromise Then(std::function<F>&& OnResolved, std::function<void(int)>&& OnRejected = {}) const
	{
		return Then(FAny(std::move(OnResolved)), std::move(OnRejected));
	}

	template<class F>
	FPromise Then(F&& OnResolved, std::function<void(int)>&& OnRejected = {}) const
	{
		typedef ClosureTraits<F> Traits;
		return Then(FAny(Traits::GenFunction(std::forward<F>(OnResolved))), std::move(OnRejected));
	}


	template< class ArgumentType, class ReturnType>
	FPromise Then(ReturnType(*OnResolved)(ArgumentType), std::function<void(int)>&& OnRejected = {}) const
	{
		return Then(FAny(std::function<ReturnType(ArgumentType)>(OnResolved)),std::move( OnRejected));
	}


	FPromise Then(FAny OnResolved, std::function<void(int)>&& OnRejected = {}) const
	{
		if (!IsValid())
			return *this;

		check(!OnResolved.IsNone() || OnRejected);

		SharedObject->AddTask(std::move(OnResolved), std::move(OnRejected));
		SharedObject->Call();

		return *this;
	}

	template<class ... ParameterType>
	void Call(const ParameterType& ... Params) const
	{
		SharedObject->Resolve(Params ...);
	}

	/*
		before c++17, we can only use index_sequence to unpack tuple as parameters
	*/
	template<class Tuple, std::size_t ... Index>
	void Call(Tuple& InTuple, std::index_sequence<Index...> const) const
	{
		Call(std::get<Index>(InTuple) ...);
	}

	bool IsValid()const
	{
		return SharedObject && SharedObject->Holder ;
	}

	bool IsClose()const
	{
		return !IsValid() || SharedObject->Holder->bClosed;
	}

	bool IsPending()const
	{
		return !IsClose() && SharedObject->State == EPromiseState::Pending;
	}

	void Close()const
	{
		if (SharedObject)
		{
			if (SharedObject->Holder)
			{
				SharedObject->Holder->Close();
			}
		}
	}

private:
	FPromiseSharedObject::Ptr SharedObject;
};

class FScopedPromise
{
public:
	FScopedPromise()
	{
		Promise = FPromise::New();
	}

	~FScopedPromise()
	{
		Close();
	}

	void Close()
	{
		Promise.Close();
	}

	FScopedPromise& operator = (FPromise InPromise)
	{
		return *this << InPromise;
	}

	FScopedPromise& operator << (FPromise InPromise)
	{
		Promise.Then(InPromise);

		return *this;
	}

	FPromise Promise;
};