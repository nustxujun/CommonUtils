#include "FCustomPhysicsFiltering.h"
#include "CollisionQueryFilterCallbackCore.h"

DEFINE_LOG_CATEGORY(LogPhysicsFiltering);

// if we want to use new filter methods all around the runtime, turn it on
#define AUTO_REGISTER WITH_EXTRA_CUSTOM_COLLISION_CHANNEL

// maybe we'd have to access collision profiles on different threads
#define NEED_THREAD_SAFE 1

#if AUTO_REGISTER 
auto Register = []{ FCustomPhysicsFiltering::Setup(true); return 0; }();
#endif

#if NEED_THREAD_SAFE 
FRWLock Locker;
#define SCOPE_LOCK(SLT) FRWScopeLock Lock(Locker, SLT);
#else
#define SCOPE_LOCK(SLT)
#endif

/**
	Simulation and Query FilterData:
		word0: (depend on the Collision/Query method)
		word1: Collision Profile ID
		word2: (depend on the Collision/Query method)
		word3: flags for external logic

*/


inline uint32 GetTypeHash(const FCustomPhysicsFiltering::CollisionChannelProfile& Profile)
{
	return HashCombine(GetTypeHash(Profile.Blocking), HashCombine(GetTypeHash(Profile.Touching), GetTypeHash(Profile.Type)));
}

TMap<FCustomPhysicsFiltering::CollisionChannelProfile, uint32> FCustomPhysicsFiltering::ProfileMap;
TArray<FCustomPhysicsFiltering::CollisionChannelProfile> FCustomPhysicsFiltering::Profiles = { FCustomPhysicsFiltering::CollisionChannelProfile(0,0,ECollisionChannel::ECC_WorldStatic) };

static uint32 GenerateIndex()
{
	// reserve 1 slot for zero index 
	static uint32 StartIndex = 1;
	return StartIndex++;
}


/**
	ATTENTION!!!
	These is a filter logic in function applyFilterEquation in PhysX. it will cause random testing failure.
	so we make a tricky on ID which makes the highest bit field of word1 being 1 

	see more infomation in function applyFilterEquation in function applyAllPreFiltersSQ in file NpSceneQueries.cpp in PhysX
*/
const uint32 ID_HEAD = 1 << 31;
const uint32 ID_MASK = ~ID_HEAD;

const FCustomPhysicsFiltering::CollisionChannelProfile& FCustomPhysicsFiltering::GetCollisionProfileByID(uint32 ID)
{
	return Profiles[ID & ID_MASK];
}

uint32 FCustomPhysicsFiltering::GetCollisionProfileID(TEnumAsByte<enum ECollisionChannel> InObjectType, uint64 Blocking, uint64 Touching)
{
	CollisionChannelProfile Profile = { Blocking, Touching, InObjectType };
	uint32* Index;
	{
		SCOPE_LOCK(SLT_ReadOnly);
		Index = ProfileMap.Find(Profile);
	}
	if (Index)
		return *Index;
	else
	{
#if NEED_THREAD_SAFE
		SCOPE_LOCK(SLT_Write);
		// check again in write lock
		Index = ProfileMap.Find(Profile);
		if (Index)
			return *Index;
#endif
		auto NewIndex = GenerateIndex() | ID_HEAD;
		ProfileMap.Add(Profile, NewIndex);
		Profiles.Add(Profile);
		return NewIndex;
	}
}

uint32 FCustomPhysicsFiltering::GetCollisionProfileID(TEnumAsByte<enum ECollisionChannel> InObjectType, const FCollisionResponseContainer& ResponseToChannels)
{
	uint64 Blocking = 0;
	uint64 Touching = 0;
	for (uint64 i = 0; i < UE_ARRAY_COUNT(ResponseToChannels.EnumArray); i++)
	{
		if (ResponseToChannels.EnumArray[i] == ECR_Block)
		{
			const uint64 ChannelBit = CRC_TO_BITFIELD(i);
			Blocking |= ChannelBit;
		}
		else if (ResponseToChannels.EnumArray[i] == ECR_Overlap)
		{
			const uint64 ChannelBit = CRC_TO_BITFIELD(i);
			Touching |= ChannelBit;
		}
	}
	return GetCollisionProfileID(InObjectType, Blocking, Touching);
}


void FCustomPhysicsFiltering::Setup(bool enable)
{
	if (enable)
	{
		GPhysicsFilterDataGenerator = &FCustomPhysicsFiltering::FilterDataGenerator;
		GSimulationFilterShader = &FCustomPhysicsFiltering::SimulationFilterShader;
		GPhysicsQueryFilterGenerator = &FCustomPhysicsFiltering::QueryFilterGenerator;
		GCollisionQueryFilterCallback = &FCustomPhysicsFiltering::QueryFilterCallback;
	}
	else
	{
		GPhysicsFilterDataGenerator = nullptr;
		GSimulationFilterShader = nullptr;
		GPhysicsQueryFilterGenerator = nullptr;
		GCollisionQueryFilterCallback = nullptr;
	}
}

PxFilterFlags FCustomPhysicsFiltering::SimulationFilterShader(PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	bool k0 = PxFilterObjectIsKinematic(attributes0);
	bool k1 = PxFilterObjectIsKinematic(attributes1);

	PxU32 FilterFlags0 = (filterData0.word3 & 0xFFFFFF);
	PxU32 FilterFlags1 = (filterData1.word3 & 0xFFFFFF);

	if (k0 && k1)
	{
		//Ignore kinematic kinematic pairs unless they are explicitly requested
		if (!(FilterFlags0 & EPDF_KinematicKinematicPairs) && !(FilterFlags1 & EPDF_KinematicKinematicPairs))
		{
			return PxFilterFlag::eSUPPRESS;	//NOTE: Waiting on physx fix for refiltering on aggregates. For now use supress which automatically tests when changes to simulation happen
		}
	}

	bool s0 = PxGetFilterObjectType(attributes0) == PxFilterObjectType::eRIGID_STATIC;
	bool s1 = PxGetFilterObjectType(attributes1) == PxFilterObjectType::eRIGID_STATIC;

	//ignore static-kinematic (this assumes that statics can't be flagged as kinematics)
	// should return eSUPPRESS here instead eKILL so that kinematics vs statics will still be considered once kinematics become dynamic (dying ragdoll case)
	if ((k0 || k1) && (s0 || s1))
	{
		return PxFilterFlag::eSUPPRESS;
	}

	// if these bodies are from the same component, use the disable table to see if we should disable collision. This case should only happen for things like skeletalmesh and destruction. The table is only created for skeletal mesh components at the moment
#if !WITH_CHAOS
	if (filterData0.word2 == filterData1.word2)
	{
		check(constantBlockSize == sizeof(FPhysSceneShaderInfo));
		const FPhysSceneShaderInfo* PhysSceneShaderInfo = (const FPhysSceneShaderInfo*)constantBlock;
		check(PhysSceneShaderInfo);
		FPhysScene* PhysScene = PhysSceneShaderInfo->PhysScene;
		check(PhysScene);

		const TMap<uint32, TMap<FRigidBodyIndexPair, bool>*>& CollisionDisableTableLookup = PhysScene->GetCollisionDisableTableLookup();
		TMap<FRigidBodyIndexPair, bool>* const* DisableTablePtrPtr = CollisionDisableTableLookup.Find(filterData1.word2);
		if (DisableTablePtrPtr)		//Since collision table is deferred during sub-stepping it's possible that we won't get the collision disable table until the next frame
		{
			TMap<FRigidBodyIndexPair, bool>* DisableTablePtr = *DisableTablePtrPtr;
			FRigidBodyIndexPair BodyPair(filterData0.word0, filterData1.word0); // body indexes are stored in word 0
			if (DisableTablePtr->Find(BodyPair))
			{
				return PxFilterFlag::eKILL;
			}

		}
	}
#endif

	// Find out which channels the objects are in
	const CollisionChannelProfile& Channel0 = GetCollisionProfileByID(filterData0.word1);
	const CollisionChannelProfile& Channel1 = GetCollisionProfileByID(filterData1.word1);

	// see if 0/1 would like to block the other 
	PxU64 BlockFlagTo1 = (ECC_TO_BITFIELD(Channel1.Type) & Channel0.Blocking);
	PxU64 BlockFlagTo0 = (ECC_TO_BITFIELD(Channel0.Type) & Channel1.Blocking);

	bool bDoesWantToBlock = (BlockFlagTo1 && BlockFlagTo0);

	// if don't want to block, suppress
	if (!bDoesWantToBlock)
	{
		return PxFilterFlag::eSUPPRESS;
	}



	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	//todo enabling CCD objects against everything else for now
	if (!(k0 && k1) && ((FilterFlags0 & EPDF_CCD) || (FilterFlags1 & EPDF_CCD)))
	{
		pairFlags |= PxPairFlag::eDETECT_CCD_CONTACT | PxPairFlag::eSOLVE_CONTACT;
	}


	if ((FilterFlags0 & EPDF_ContactNotify) || (FilterFlags1 & EPDF_ContactNotify))
	{
		pairFlags |= (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS);
	}


	if ((FilterFlags0 & EPDF_ModifyContacts) || (FilterFlags1 & EPDF_ModifyContacts))
	{
		pairFlags |= (PxPairFlag::eMODIFY_CONTACTS);
	}

	return PxFilterFlags();
}


inline uint32 CreateMaskFilter(uint8 CustomMaskFilter, FMaskFilter MaskFilter)
{
	uint32 ResultMask = (uint32(MaskFilter) << NumCollisionChannelBits) | ((uint32)CustomMaskFilter & (uint32)ECustomMaskFilter::Mask);
	return ResultMask << NumFilterDataFlagBits;
}

void FCustomPhysicsFiltering::FilterDataGenerator(
	TEnumAsByte<enum ECollisionChannel> InObjectType,
	FMaskFilter MaskFilter,
	const struct FCollisionResponseContainer& ResponseToChannels,
	uint32& BlockingBits,
	uint32& TounchingBits,
	uint32& Word3)
{
	// BlockingBits will be setted to word1, just record ID on it
	// We will find touching data from table,so we do not care about TouchingBits here
	BlockingBits = GetCollisionProfileID(InObjectType, ResponseToChannels);
	// we can use channel(old) to store custom mask filter
	Word3 = CreateMaskFilter(0, MaskFilter);
}

#define TRACE_MULTI		1
#define TRACE_SINGLE	0

FCollisionFilterData FCustomPhysicsFiltering::QueryFilterGenerator(
	const uint8 MyChannel,
	const bool bTraceComplex,
	const FCollisionResponseContainer& InCollisionResponseContainer,
	const struct FCollisionQueryParams& QueryParam,
	const struct FCollisionObjectQueryParams& ObjectParam,
	const bool bMultitrace)
{
	const int32 MultiTrace = bMultitrace ? TRACE_MULTI : TRACE_SINGLE;
	FCollisionFilterData NewData;

	if (bTraceComplex)
	{
		NewData.Word3 |= EPDF_ComplexCollision;
	}
	else
	{
		NewData.Word3 |= EPDF_SimpleCollision;
	}

	/*	word0: ECollisionQuery type value
		word1: CollisionChannel ID
		word2: only for Object Query
		word3: flags
	*/
	if (ObjectParam.IsValid())// ObjectQuery
	{
		NewData.Word0 = (uint32)ECollisionQuery::ObjectQuery;

		NewData.Word1 = GetCollisionProfileID((ECollisionChannel)MyChannel, ObjectParam.GetQueryBitfield(), 0);
		NewData.Word2 = MultiTrace;
		// set flags, with default collision channel which we do not care
		NewData.Word3 |= CreateMaskFilter(0, ObjectParam.IgnoreMask);
	}
	else // TraceQuery
	{
		NewData.Word0 = (uint32)ECollisionQuery::TraceQuery;

		NewData.Word1 = GetCollisionProfileID((ECollisionChannel)MyChannel, InCollisionResponseContainer);

		// set flags
		NewData.Word3 |= CreateMaskFilter(0, QueryParam.IgnoreMask);


	}
	return NewData;
}

inline FMaskFilter GetExtraFilter(uint32 Word3)
{
	return Word3 >> (32 - NumExtraFilterBits);
}

inline uint32 GetCustomMaskFilter(uint32 Word3)
{
	return ((Word3 >> NumFilterDataFlagBits) & (uint32)ECustomMaskFilter::Mask);
}

uint8 FCustomPhysicsFiltering::QueryFilterCallback(
	const FCollisionFilterData& QueryFilter,
	const FCollisionFilterData& ShapeFilter,
	bool bPreFilter)
{
	ECollisionQuery QueryType = (ECollisionQuery)QueryFilter.Word0;
	FMaskFilter QuerierMaskFilter = GetExtraFilter(QueryFilter.Word3);
	const CollisionChannelProfile& QuerierChannel = GetCollisionProfileByID(QueryFilter.Word1);

	FMaskFilter ShapeMaskFilter = GetExtraFilter(ShapeFilter.Word3);
	const CollisionChannelProfile& ShapeChannel = GetCollisionProfileByID(ShapeFilter.Word1);

	if ((QuerierMaskFilter & ShapeMaskFilter) != 0)	//If ignore mask hit something, ignore it
	{
		return (uint8)ECollisionQueryHitType::None;
	}

	const uint64 ShapeBit = ECC_TO_BITFIELD(ShapeChannel.Type);
	if (QueryType == ECollisionQuery::ObjectQuery)
	{
		const int32 MultiTrace = (int32)QueryFilter.Word2;
		// do I belong to one of objects of interest?
		if (ShapeBit & QuerierChannel.Blocking)
		{
			if (bPreFilter)	//In the case of an object query we actually want to return all object types (or first in single case). So in PreFilter we have to trick physx by not blocking in the multi case, and blocking in the single case.
			{
				return MultiTrace ? (uint8)ECollisionQueryHitType::Touch : (uint8)ECollisionQueryHitType::Block;
			}
			else
			{
				return (uint8)ECollisionQueryHitType::Block;	//In the case where an object query is being resolved for the user we just return a block because object query doesn't have the concept of overlap at all and block seems more natural
			}
		}
	}
	else
	{
		// if query channel is Touch All, then just return touch
		if (QuerierChannel.Type == ECC_OverlapAll_Deprecated)
		{
			return (uint8)ECollisionQueryHitType::Touch;
		}

		uint64 const QuerierBit = ECC_TO_BITFIELD(QuerierChannel.Type);
		ECollisionQueryHitType QuerierHitType = ECollisionQueryHitType::None;
		ECollisionQueryHitType ShapeHitType = ECollisionQueryHitType::None;

		// check if Querier wants a hit
		if ((QuerierBit & ShapeChannel.Blocking) != 0)
		{
			QuerierHitType = ECollisionQueryHitType::Block;
		}
		else if ((QuerierBit & ShapeChannel.Touching) != 0)
		{
			QuerierHitType = ECollisionQueryHitType::Touch;
		}

		if ((ShapeBit & QuerierChannel.Blocking) != 0)
		{
			ShapeHitType = ECollisionQueryHitType::Block;
		}
		else if ((ShapeBit & QuerierChannel.Touching) != 0)
		{
			ShapeHitType = ECollisionQueryHitType::Touch;
		}

		// return minimum agreed-upon interaction
		return (uint8)FMath::Min(QuerierHitType, ShapeHitType);
	}

	return (uint8)ECollisionQueryHitType::None;
}


void FCustomPhysicsFiltering::SetCustomMaskFilter(uint32& Word3, ECustomMaskFilter MaskFilter, bool Enable)
{
	if (Enable)
		Word3 |= ((uint32)ECustomMaskFilter::Mask & (uint32)MaskFilter) << NumFilterDataFlagBits;
	else
		Word3 &= ~(((uint32)ECustomMaskFilter::Mask & (uint32)MaskFilter) << NumFilterDataFlagBits);
}

