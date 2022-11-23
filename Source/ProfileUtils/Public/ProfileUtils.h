#pragma once

#include "CoreMinimal.h"

#define USING_SAFE_NAME 1
#define USING_CPU_TRACE 1

class FProfileUtils
{
public:
#if USING_CPU_TRACE
    using CounterType = FCpuProfilerTrace::FEventScope;
    using Id = uint32;


#else
    using CounterType = FScopeCycleCounter;
    using Id = TStatId;

#endif

    using Counter = TUniquePtr<CounterType>;

    template<class Key, class MakeName>
    Id FProfileUtils::GetId(const Key& InKey, const MakeName& InMakeName)
    {
        static TMap<Key, Id> StatIds;
        auto& StatId = StatIds.FindOrAdd(InKey, {});

#if USING_CPU_TRACE
        if (StatId == 0)
        {
            StatId = FCpuProfilerTrace::OutputEventType(*InMakeName());
        }
#else
        if (Id.IsNone())
        {
            StatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_LuaStatsFull>(InMakeName());
        }
#endif
        return StatId;
    }

 
    template<class Key, class MakeName>
    static Counter CreateCounter(const Key& InKey, const MakeName& InMakeName)
    {
        return MakeUnique<CounterType>(GetId<Key, MakeName>(InKey, InMakeName));
    }
};