#pragma once

#include "CoreMinimal.h"

class LUAUTILS_API FLuaStatsFull
{
public:
	static void Start(struct lua_State* L);
};