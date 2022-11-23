#pragma once 

#include "CoreMinimal.h"

#define WITH_LIBXL PLATFORM_WINDOWS && PLATFORM_64BITS&& WITH_EDITOR

#if WITH_LIBXL
#include "libxl.h"
#endif