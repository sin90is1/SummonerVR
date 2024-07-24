// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "ReferenceSkeleton.h"
#include "Engine/NetSerialization.h"
#include "YnnkVRIKTypes.generated.h"

#ifndef __ueversion_compatibility
	#define __ueversion_compatibility

	#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 26
		#define __uev_GetRefSkeleton(SkelMesh) SkelMesh->GetRefSkeleton()
	#else
		#define __uev_GetRefSkeleton(SkelMesh) SkelMesh->RefSkeleton
	#endif

	#if ENGINE_MAJOR_VERSION > 4
		#define __uev_IsServer (GetWorld()->GetNetMode() < ENetMode::NM_Client)
	#else
		#define __uev_IsServer GetWorld()->IsServer()
	#endif

	#if ENGINE_MAJOR_VERSION < 5
		#define FVector2f FVector2D
		#define FVector3f FVector
		#define FRotator3f FRotator
		#define FTransform3f FTransform
		#define FRotationMatrix44f FRotationMatrix
	#endif

	#if ENGINE_MAJOR_VERSION > 4 && ENGINE_MINOR_VERSION > 0
		#define __uev_GetSkeletalMesh(Comp) Cast<USkeletalMesh>(Comp->GetSkinnedAsset())
	#else
		#define __uev_GetSkeletalMesh(Comp) Comp->SkeletalMesh
	#endif
#endif

/* Common types of motion controllers */
UENUM(BlueprintType, Blueprintable)
enum class EMotionControllerModel : uint8
{
	MC_ViveController					UMETA(DisplayName = "HTC Vive Controller"),
	MC_ValveIndex						UMETA(DisplayName = "Valve Index"),
	MC_OculusTouch						UMETA(DisplayName = "Oculus Touch"),
	MC_ViveCosmos						UMETA(DisplayName = "HTC Vive Cosmos"),
	MC_Pico4							UMETA(DisplayName = "Pico 4")
};


/* Simplified Transform Data for networking */
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FNetworkTransform
{
	GENERATED_USTRUCT_BODY()

	/* Location in actor space */
	UPROPERTY()
	FVector_NetQuantize100 Location;
	/* Rotation in actor space */
	UPROPERTY()
	FVector_NetQuantize100 Rotation;

	FNetworkTransform()
	{
	}
	FNetworkTransform(const FTransform& InTr)
	{
		Location = InTr.GetTranslation();
		Rotation = FVector_NetQuantize100(InTr.Rotator().Roll, InTr.Rotator().Pitch, InTr.Rotator().Yaw);
	}
	FTransform AsTransform() const
	{		
		return FTransform(FRotator(Rotation.Y, Rotation.Z, Rotation.X), Location);
	}
};

/* Data set describing calibrated body state. Required to save and load body params on player respawn */
USTRUCT(Blueprintable)
struct VRIKBODYRUNTIME_API FYnnkBodyState
{
	GENERATED_USTRUCT_BODY()

	/* Position of head target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform Head;

	/* Position of right hand target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform HandRight;

	/* Position of left hand target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform HandLeft;

	/* Position of êøèñôïó target relative to GroundRef component (always proceduarl) */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform Ribcage;

	/* Position of pelvis target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform Pelvis;

	/* Position of right foot target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform FootRight;

	/* Position of left foot target relative to GroundRef component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk Body State")
	FTransform FootLeft;
};

/* Equal to FYnnkBodyState, but optimized for replication */
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FYnnkBodyStateRep
{
	GENERATED_USTRUCT_BODY()

	/* Position of head target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform Head;

	/* Position of right hand target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform HandRight;

	/* Position of left hand target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform HandLeft;

	/* Position of pelvis target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform Pelvis;

	/* Position of right foot target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform FootRight;

	/* Position of left foot target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform FootLeft;
};

/**
* Information about spine bones in skeleton, needed for spine/torso and neck IK
* Note: spine is calculated up-down, from head to hips, because position of head has a priority
*/
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FSpineBonePreset
{
	GENERATED_USTRUCT_BODY()

	/* distance to previous spine bone (located above, i. e. next bone in skeleton hierarhy) */
	UPROPERTY()
	float VerticalDistance;

	/* Offset from previous bone in component space */
	UPROPERTY()
	FTransform BoneOffset;

	FSpineBonePreset()
		: VerticalDistance(0.f)
	{}
};

/* Size of bones in the current skeletal mesh */
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FSkeletalMeshSizeData
{
	GENERATED_USTRUCT_BODY()

	/** Upperarms */
	UPROPERTY()
	float UpperarmLength;

	/** Lowerarms */
	UPROPERTY()
	float LowerarmLength;

	/** Thighs */
	UPROPERTY()
	float ThighLength;

	/** Calfs */
	UPROPERTY()
	float CalfLength;

	FSkeletalMeshSizeData()
		: UpperarmLength(0.f)
		, LowerarmLength(0.f)
		, ThighLength(0.f)
		, CalfLength(0.f)
	{}
};

namespace YnnkHelpers
{
	/** Get bone transform in ref pose in component space */
	FTransform VRIKBODYRUNTIME_API RestoreRefBonePose(const FReferenceSkeleton& RefSkeleton, const int32 InBoneIndex);

	/**
	* Calculate rotation in horizontal plane for root bone
	* Use Pelvis forward rotation when it's projection to horizontal plane is large enougth
	* or interpolated value between forward and up vectors in other casese
	*/
	FRotator VRIKBODYRUNTIME_API CalculateRootRotation(const FVector& ForwardVector, const FVector& UpVector);

	/**
	* Get A-B delta in Euler coordinates, independent from gimbal lock
	* Instead of direct delta, I use a very simple trick with one intermediate rotation
	*/
	void VRIKBODYRUNTIME_API GetRotationDeltaInEulerCoordinates(const FQuat& A, const FQuat& B, float& Roll, float& Pitch, float& Yaw);

	/** Rotate around local axis */
	void VRIKBODYRUNTIME_API AddRelativeRotation(FRotator& RotatorToModify, const FRotator& AddRotator);

	/** Simple two - bone IK solver, returns new location of effector(equal to ChainEnd if it's reachable) */
	FVector VRIKBODYRUNTIME_API CalculateTwoBoneIK(
		const FVector& ChainStart, const FVector& ChainEnd,
		const FVector& JointTarget,
		const float UpperBoneSize, const float LowerBoneSize,
		FTransform& UpperBone, FTransform& LowerBone,
		bool bIsRight, bool bSnapToChainEnd);
}
