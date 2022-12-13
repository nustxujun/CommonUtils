#pragma once 
#include "CoreMinimal.h"
#include "Physics/PhysicsFiltering.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsFiltering, All, All);

#define WITH_EXTRA_CUSTOM_COLLISION_CHANNEL 1

enum class ECustomMaskFilter
{
	None = 0,

	Max = 1 << NumCollisionChannelBits,
	Mask = Max - 1,
};

class FCustomPhysicsFiltering
{
public:

	struct CollisionChannelProfile
	{
		uint64 Blocking;
		uint64 Touching;
		ECollisionChannel Type;

#ifdef PLATFORM_DESKTOP // only for debug on desktop
		FCollisionResponseContainer ResponseToChannels;
		CollisionChannelProfile()
		{
		}
		CollisionChannelProfile(uint64 InBlocking, uint64 InTouching, ECollisionChannel InType):
			Blocking(InBlocking), Touching(InTouching), Type(InType), ResponseToChannels(ECR_Ignore)
		{
			for (int i = 0; i < 64; ++i)
			{
				if (Blocking & CRC_TO_BITFIELD(i))
				{
					ResponseToChannels.EnumArray[i] = ECR_Block;
				}
				else if (Touching & CRC_TO_BITFIELD(i))
				{
					ResponseToChannels.EnumArray[i] = ECR_Overlap;
				}
			}
		}
#endif

		bool operator == (const CollisionChannelProfile& Other)const
		{
			return Blocking == Other.Blocking && Touching == Other.Touching && Type == Other.Type;
		}
	};

public:
	static void Setup(bool enable);
	static uint32 GetCollisionProfileID(TEnumAsByte<enum ECollisionChannel> InObjectType, uint64 Blocking, uint64 Touching);
	static uint32 GetCollisionProfileID(TEnumAsByte<enum ECollisionChannel> InObjectType, const FCollisionResponseContainer& ResponseToChannels);
	static void SetCustomMaskFilter(uint32& Word3, ECustomMaskFilter MaskFilter, bool Enable);

	static const CollisionChannelProfile& GetCollisionProfileByID(uint32 ID);


	static PxFilterFlags SimulationFilterShader(
		PxFilterObjectAttributes attributes0, PxFilterData filterData0,
		PxFilterObjectAttributes attributes1, PxFilterData filterData1,
		PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize);

	static void FilterDataGenerator(
		TEnumAsByte<enum ECollisionChannel> InObjectType,
		FMaskFilter MaskFilter,
		const struct FCollisionResponseContainer& ResponseToChannels,
		uint32& BlockingBits,
		uint32& TounchingBits,
		uint32& Word3);
	static FCollisionFilterData QueryFilterGenerator(
		const uint8 MyChannel,
		const bool bTraceComplex,
		const FCollisionResponseContainer& InCollisionResponseContainer,
		const struct FCollisionQueryParams& QueryParam,
		const struct FCollisionObjectQueryParams& ObjectParam,
		const bool bMultitrace);
	static uint8 QueryFilterCallback(
		const FCollisionFilterData& QueryFilter,
		const FCollisionFilterData& ShapeFilter,
		bool bPreFilter);
private:
	static TMap<CollisionChannelProfile, uint32> ProfileMap;
	static TArray<CollisionChannelProfile> Profiles;
};