#include "LuaStatsFull.h"
#include "Misc/Parse.h"
#include "String/ParseTokens.h"

// make sure we have the lua library
#ifndef HAS_LUA_BUILD 
	#define HAS_LUA_BUILD 0
#endif

#define WITH_LUA_STATS HAS_LUA_BUILD


#if WITH_LUA_STATS && STATS

DECLARE_STATS_GROUP(TEXT("Lua stats full"), STATGROUP_LuaStatsFull, STATCAT_Advanced);

#define USING_SAFE_NAME 1
#define USING_CPU_TRACE 1

#if USING_SAFE_NAME
using String = FName;
#else
// assume the string is static in lua ,we can store and compare string by address
// fast but unsafe
using String = const char*;
#endif

struct FFunctionKey
{
	String Name;
	String Src;
	int Line;

	bool operator == (const FFunctionKey& Other)const
	{
		return Line == Other.Line && Name == Other.Name && Src == Other.Src;
	}
};

FORCEINLINE uint32 GetTypeHash(const FFunctionKey& Thing)
{
	return HashCombine(GetTypeHash(Thing.Line), HashCombine(GetTypeHash(Thing.Name), GetTypeHash(Thing.Src)));
};

#if USING_CPU_TRACE

using Id = uint32;
using FCounter = FCpuProfilerTrace::FEventScope;

#define CONSTRUCTOR_PARAMS(Id) Id, CpuChannel
#define CMP_ID(Id) Id == 0
#define CRT_ID(Name) FCpuProfilerTrace::OutputEventType(*Name)

#else

using Id = TStatId;
using FCounter = FScopeCycleCounter;

#define CONSTRUCTOR_PARAMS Id
#define CMP_ID(Id) Id.IsNone()
#define CRT_ID(Name) FDynamicStats::CreateStatId<FStatGroup_STATGROUP_LuaStatsFull>(Name)

#endif


FORCEINLINE Id GetId(const FFunctionKey& Key, lua_Debug* ar)
{
	static TMap<FFunctionKey, Id> StatIds;
	auto& StatId = StatIds.FindOrAdd(Key, {});
	if (CMP_ID(StatId))
	{
		FString Name;
		if (ar->name)
			Name = FString(ANSI_TO_TCHAR(ar->name)) + ANSI_TO_TCHAR(ar->short_src) + LexToString(ar->linedefined);
		else
			Name = ANSI_TO_TCHAR(ar->short_src) + LexToString(ar->linedefined);
		StatId = CRT_ID(Name);
	}
	return StatId;
}


struct FStackFrame
{
	void operator=(const FStackFrame&) = delete;
	

#if PLATFORM_DESKTOP
	FFunctionKey FunctionKey;
	FStackFrame(FFunctionKey k, Id Id):
		FunctionKey(k), 
		Counter(CONSTRUCTOR_PARAMS(Id))
	{
	}
#else
	FStackFrame(Id Id) :
		Counter(CONSTRUCTOR_PARAMS(Id))
	{
	}
#endif
	FCounter Counter;
	bool Discard = false;

};


static struct
{
	// if true , stats will hide the built-in function just like "pair", "ipair", "next", "type", etc .
	bool bSkipCFunction = true;
	/*
		if true, stats will hide the tail calling function name .
		Code:
			function Caller()
				return Callee(); // tail call 
			end

			function Callee()
				-- Do Something
			end

		Stats:
			if true:
				____________________
				|  Caller			|
				____________________
					| Callee		|
					________________
						|DoSomething|

			if false:
				________________
				| Caller		|
				________________
					|DoSomething|
	*/
	bool bSkipTailCall = true; 
} 
LuaStatsSettings;

void hook(lua_State* L, lua_Debug* ar)
{
	static TArray<FStackFrame> CallStack;

	if (lua_getinfo(L, "nS", ar) == 0)
	{
		return;
	}

	if (LuaStatsSettings.bSkipCFunction && ar->what[0] == 'C')
		return;

	FFunctionKey FunctionKey{ 
		String( ar->name ), 
#if USING_SAFE_NAME
		/* limit string length to 1024 for FName */
		String(FMath::Min(ar->srclen,(size_t)(NAME_SIZE - 1)), ar->source + (ar->srclen < (size_t)NAME_SIZE? 0: (int64)ar->srclen - (int64)NAME_SIZE + 1)),
#else
		String(ar->source),
#endif
		ar->linedefined };

	switch (ar->event)
	{
	case LUA_HOOKTAILCALL:
		if (!LuaStatsSettings.bSkipCFunction)
			CallStack[CallStack.Num() - 1].Discard = true;
		else
			break;
	case LUA_HOOKCALL:
	{
		auto StatId = GetId(FunctionKey, ar);

#if PLATFORM_DESKTOP
		CallStack.Emplace(FunctionKey, StatId);
#else
		CallStack.Emplace(StatId);
#endif

	}
	break;
	case LUA_HOOKRET:
	{
		do
		{
			CallStack.RemoveAt(CallStack.Num() - 1, 1, false);
		}
		while (!LuaStatsSettings.bSkipTailCall && CallStack.Num() != 0 && CallStack[CallStack.Num() - 1].Discard);
	}
	break;
	default:
		break;
	}
}

#endif

void FLuaStatsFull::Start(struct lua_State* L)
{
#if WITH_LUA_STATS && STATS
	FString Functions;
	FParse::Value(FCommandLine::Get(), TEXT("-luastatsfull="), Functions, false);
	UE::String::ParseTokens(Functions, TEXT(","), [](FStringView Token) {
		if (Token.Compare(TEXT("cfunction"), ESearchCase::IgnoreCase) == 0)
		{
			LuaStatsSettings.bSkipCFunction = false;
		}
		else if (Token.Compare(TEXT("tailcall"), ESearchCase::IgnoreCase) == 0)
		{
			LuaStatsSettings.bSkipTailCall = false;
		}
	});

	if (!Functions.IsEmpty())
		lua_sethook(L, hook, LUA_MASKCALL | LUA_MASKRET, 0);
#endif
}
