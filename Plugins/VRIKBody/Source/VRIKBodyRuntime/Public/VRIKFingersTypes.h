// Copyright Yuri N K, 2022. All Rights Reserved.
// Support: ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "VRIKFingersTypes.generated.h"

// Backward compatibility
#ifndef __ue4_backward_compatibility_vrhands
	#define __ue4_backward_compatibility_vrhands
	#include "Runtime/Launch/Resources/Version.h"

	#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 26
		#define AccessRefSkeleton(SkelMeshComp) SkelMeshComp->GetRefSkeleton()
	#else
		#define AccessRefSkeleton(SkelMeshComp) SkelMeshComp->RefSkeleton
	#endif

	#if ENGINE_MAJOR_VERSION > 4 && ENGINE_MINOR_VERSION > 0
		#define AccessSkeletalMesh(comp) Cast<USkeletalMesh>(comp->GetSkinnedAsset())
		#define OwnedSkeletalMesh GetSkinnedAsset()
	#else
		#define AccessSkeletalMesh(comp) comp->SkeletalMesh
		#define OwnedSkeletalMesh SkeletalMesh
	#endif
#endif

/**
* Humanoid hand side, left or right
*/
UENUM(BlueprintType, meta=(DisplayName="VR Hand"))
enum class EVRIK_VRHand : uint8
{
	VRH_Left				UMETA(DisplayName = "Left"),
	VRH_Right				UMETA(DisplayName = "Right"),
	VRH_MAX					UMETA(Hidden)
};
/**
* Bone axis value
*/
UENUM(BlueprintType, meta = (DisplayName = "Bone Orientation Axis"))
enum class EVRIK_BoneOrientationAxis : uint8
{
	X				UMETA(DisplayName = "X+"),
	Y				UMETA(DisplayName = "Y+"),
	Z				UMETA(DisplayName = "Z+"),
	X_Neg			UMETA(DisplayName = "X-"),
	Y_Neg			UMETA(DisplayName = "Y-"),
	Z_Neg			UMETA(DisplayName = "Z-"),
	BOA_MAX			UMETA(Hidden)
};

/**
* Humanoid fingers
*/
UENUM(BlueprintType, meta = (DisplayName = "Finger Name"))
enum class EVRIKFingerName : uint8
{
	FN_Thumb		UMETA(DisplayName = "Thumb Finger"),
	FN_Index		UMETA(DisplayName = "Index Finger"),
	FN_Middle		UMETA(DisplayName = "Middle Finger"),
	FN_Ring			UMETA(DisplayName = "Ring Finger"),
	FN_Pinky		UMETA(DisplayName = "Pinky Finger"),
	EVRIKFingerName_MAX UMETA(Hidden)
};

/**
* Finger 3DOF rotation
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Finger Rotation"))
struct FVRIK_FingerRotation
{
	GENERATED_BODY()

	/** One unit is 90 degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Rotation")
	float CurlValue;

	/** First bone curl addend */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Rotation")
	float FirstKnuckleCurlAddend;

	/** One unit is 20 degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Rotation")
	float SpreadValue;

	/** One unit is 20 degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Rotation")
	float RollValue;

	FVRIK_FingerRotation()
	{
		FirstKnuckleCurlAddend = CurlValue = SpreadValue = RollValue = 0.f;
	}

	bool operator==(const FVRIK_FingerRotation& Other) const
	{
		return (CurlValue == Other.CurlValue && FirstKnuckleCurlAddend == Other.FirstKnuckleCurlAddend && SpreadValue == Other.SpreadValue && RollValue == Other.RollValue);
	}
	bool operator!=(const FVRIK_FingerRotation& Other) const
	{
		return !(*this == Other);
	}
};

/**
Transform struct with support of orientation transformations
*/
struct FVRIK_OrientTransform : FTransform
{
	EVRIK_BoneOrientationAxis ForwardAxis;
	EVRIK_BoneOrientationAxis RightAxis;
	EVRIK_BoneOrientationAxis UpAxis;
	float RightAxisMultiplier;

private:
	FORCEINLINE FVector GetAxisVector(EVRIK_BoneOrientationAxis Axis) const
	{
		return GetAxisVector(*this, Axis);
	}

public:

	static FVector GetAxisVector(const FTransform& InTransform, EVRIK_BoneOrientationAxis Axis);
	static FVector GetAxisVector(const FRotator& InRotator, EVRIK_BoneOrientationAxis Axis);

	FVRIK_OrientTransform();
	FVRIK_OrientTransform(const FVRIK_OrientTransform& OrientationBase, const FTransform& InTransform,
		float InRightAxisMultiplier = 1.f,
		EVRIK_BoneOrientationAxis InForwardAxis = EVRIK_BoneOrientationAxis::X, EVRIK_BoneOrientationAxis InUpAxis = EVRIK_BoneOrientationAxis::Z);

	void SetTransform(const FTransform& InTransform);

	static FVRIK_OrientTransform MakeOrientTransform(const FVRIK_OrientTransform& OrientationBase, const FTransform& InTransform);

	static FRotator ConvertRotator(const FVRIK_OrientTransform& SourceRot, const FVRIK_OrientTransform& TargetRot);

	FVector GetBoneForwardVector() const { return GetScaledAxis(EAxis::X); }
	FVector GetBoneRightVector() const { return GetScaledAxis(EAxis::Y); }
	FVector GetBoneUpVector() const { return GetScaledAxis(EAxis::Z); }

	FVector GetConvertedForwardVector() const { return GetAxisVector(ForwardAxis); }
	FVector GetConvertedRightVector() const { return GetAxisVector(RightAxis) * RightAxisMultiplier; }
	FVector GetConvertedUpVector() const { return GetAxisVector(UpAxis); }

	FVector GetConvertedForwardVectorUnscaled() const { return GetAxisVector(ForwardAxis); }
	FVector GetConvertedRightVectorUnscaled() const { return GetAxisVector(RightAxis); }
	FVector GetConvertedUpVectorUnscaled() const { return GetAxisVector(UpAxis); }

	/** Helper. Find axis in transform rotation co-directed with vector */
	static EVRIK_BoneOrientationAxis FindCoDirection(const FTransform& BoneRotator, const FVector Direction);
	/** Helper */
	static FRotator MakeRotFromTwoAxis(EVRIK_BoneOrientationAxis Axis1, const FVector& Vector1, EVRIK_BoneOrientationAxis Axis2, const FVector& Vector2);

	FVRIK_OrientTransform operator*(const FTransform& Other) const;
	FVRIK_OrientTransform& operator=(const FTransform& Other);
	FVRIK_OrientTransform& operator=(const FVRIK_OrientTransform& Other);
};

/**
* Description of finger knuckle. Initialized in UFingerFKIKSolver::Initialize.
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Knuckle"))
struct FVRIK_Knuckle
{
	GENERATED_BODY()

	/** Name of skeleton bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FName BoneName;

	/** Index of bone in skeleton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	int32 BoneIndex;

	/** Knuckle radius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	float Radius;

	/** Distance to the next knuckle (or finger end) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	float Length;

	/** Instantaneous transform in world space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform WorldTransform;

	/** Current relative transform */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform RelativeTransform;

	/** Relative transform from input animation skeleton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform RefPoseRelativeTransform;

	/** Relative transform from input animation pose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform TraceRefPoseRelativeTransform;

	/** Relative transform for vr input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform InputRefPoseRelativeTransform;

	/** Relative transform for vr input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knuckle")
	FTransform InputGripPoseRelativeTransform;

	FVRIK_Knuckle()
		: BoneName(NAME_None)
		, BoneIndex(INDEX_NONE)
		, Radius(0.f)
		, Length(0.f)
	{}
};

/**
* Figer description. Used both for input and output
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Finger Solver"))
struct FVRIK_FingerSolver
{
	GENERATED_BODY()

	/** Name of last knuckle bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FName TipBoneName;

	/** Number of knuckles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	int32 KnucklesNum;

	/** Radius of tip bone (smallest) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	float TipRadius;

	/** Radius of first bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	float RootRadius;

	/** Transform convertatation for finger */
	FVRIK_OrientTransform FingerOrientation;

	/** Outward local axis of the finger (normal to fingers plane) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	EVRIK_BoneOrientationAxis OutwardAxis;

	/** Should solver process this finger? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	bool bEnabled;

	/** Current alpha to blend between input finger pose and calculated finger pose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finger Solver")
	float Alpha;

	/** Calculated: array of knuckles */
	UPROPERTY()
	TArray<FVRIK_Knuckle> Knuckles;

	/** Calculated: hand bone name */
	UPROPERTY()
	FName RootBoneName;

	/** Calculated: relative offset of finger parent bone (Metacarpal) to wrist bone */
	UPROPERTY()
	FTransform RootBoneToWristOffset;

	FVRIK_FingerSolver()
	{
		KnucklesNum = 3; TipRadius = 0.4f; RootRadius = 0.8f; Alpha = 1.f; bEnabled = true;
		OutwardAxis = EVRIK_BoneOrientationAxis::Y_Neg;
	};
};

/** Struct to get knuckle transform by bone index */
USTRUCT()
struct VRIKBODYRUNTIME_API FVRIK_FingerKnucklePointer
{
	GENERATED_BODY()

	/** Name of finger in UHandSkeletalMeshComponent::Fingers map */
	UPROPERTY()
	EVRIKFingerName FingerId;

	/** Index in FVRIK_FingerSolver::Knuckles array */
	UPROPERTY()
	int32 KnuckleId;

	FVRIK_FingerKnucklePointer()
		: FingerId(EVRIKFingerName::FN_Thumb)
		, KnuckleId(INDEX_NONE)
	{}
};

/** Struct to get twist rotation by bone index */
USTRUCT()
struct VRIKBODYRUNTIME_API FVRIK_TwistData
{
	GENERATED_BODY()

	/** Pointer to twist variable */
	float* TwistSource;

	/** Twist multiplier from UHandSkeletalMeshComponent::UpperarmTwists or UHandSkeletalMeshComponent::LowerarmTwists */
	UPROPERTY()
	float Mulitplier;

	FVRIK_TwistData()
		: TwistSource(NULL)
		, Mulitplier(0.f)
	{}
};

/** Fingers pose description: rotation of all fingers */
USTRUCT(BlueprintType, meta = (DisplayName = "Finger Pose Preset"))
struct FVRIK_FingersPosePreset
{
	GENERATED_BODY()

	/** Thumb rotation. All axes usually require adjustment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FVRIK_FingerRotation ThumbRotation;

	/** Index finger rotation. Set curl value and others if necessary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FVRIK_FingerRotation IndexRotation;

	/** Middle finger rotation. Set curl value and others if necessary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FVRIK_FingerRotation MiddleRotation;

	/** Ring finger rotation. Set curl value and others if necessary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FVRIK_FingerRotation RingRotation;

	/** Pinky finger rotation. Set curl value and others if necessary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Solver")
	FVRIK_FingerRotation PinkyRotation;

	bool operator==(const FVRIK_FingersPosePreset& Other) const
	{
		return (ThumbRotation == Other.ThumbRotation && IndexRotation == Other.IndexRotation && MiddleRotation == Other.MiddleRotation && RingRotation == Other.RingRotation && PinkyRotation == Other.PinkyRotation);
	}
	bool operator!=(const FVRIK_FingersPosePreset& Other) const
	{
		return !(*this == Other);
	}
};


/**
* Local rotations of all bones of human hand. Applied to VR Input Reference Pose by ApplyDetailedVRInput function.
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Fingers Detailed Info"))
struct VRIKBODYRUNTIME_API FVRIK_FingersDetailedInfo
{
	GENERATED_BODY()

	/** Local rotations of thumb bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Detailed Info")
	TArray<FVRIK_FingerRotation> ThumbBones;

	/** Local rotations of index bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Detailed Info")
	TArray<FVRIK_FingerRotation> IndexBones;

	/** Local rotations of middle finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Detailed Info")
	TArray<FVRIK_FingerRotation> MiddleBones;

	/** Local rotations of ring finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Detailed Info")
	TArray<FVRIK_FingerRotation> RingBones;

	/** Local rotations of pinky finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Detailed Info")
	TArray<FVRIK_FingerRotation> PinkyBones;

	FVRIK_FingersDetailedInfo()
	{
		ThumbBones.SetNum(3);
		IndexBones.SetNum(3);
		MiddleBones.SetNum(3);
		RingBones.SetNum(3);
		PinkyBones.SetNum(3);
	}

	bool operator==(const FVRIK_FingersDetailedInfo& Other) const
	{
		if (ThumbBones != Other.ThumbBones) return false;
		if (IndexBones != Other.IndexBones) return false;
		if (MiddleBones != Other.MiddleBones) return false;
		if (RingBones != Other.RingBones) return false;
		if (PinkyBones != Other.PinkyBones) return false;
		return true;
	}
	bool operator!=(const FVRIK_FingersDetailedInfo& Other) const
	{
		return !(*this == Other);
	}
};

/**
* Raw state of finger bones with rotations relative to previous bone rather then reference pose
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Finger Raw Rotation"))
struct VRIKBODYRUNTIME_API FVRIK_FingerRawRotation
{
	GENERATED_BODY()

	/** Local rotation of first knuckle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Raw Rotation")
	float FingerRoll;

	/** Local rotation of first knuckle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Raw Rotation")
	float FingerYaw;

	/** Relative rotations of knuckles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finger Raw Rotation")
	TArray<float> Curls;

	FVRIK_FingerRawRotation()
		: FingerRoll(0.f), FingerYaw(0.f)
	{
		Curls.SetNum(3);
	}

	bool operator==(const FVRIK_FingerRawRotation& Other) const
	{
		return (FingerRoll == Other.FingerRoll && FingerYaw == Other.FingerYaw && Curls == Other.Curls);
	}
	bool operator!=(const FVRIK_FingerRawRotation& Other) const
	{
		return !(*this == Other);
	}
};

/**
* Rotations of all fingers relative to parent bones
*/
USTRUCT(BlueprintType, meta = (DisplayName = "Fingers Raw Preset"))
struct VRIKBODYRUNTIME_API FVRIK_FingersRawPreset
{
	GENERATED_BODY()

	/** Local rotations of index bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Raw Preset")
	FVRIK_FingerRawRotation Thumb;

	/** Local rotations of index bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Raw Preset")
	FVRIK_FingerRawRotation Index;

	/** Local rotations of middle finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Raw Preset")
	FVRIK_FingerRawRotation Middle;

	/** Local rotations of ring finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Raw Preset")
	FVRIK_FingerRawRotation Ring;

	/** Local rotations of pinky finger bones in degrees (relative to VR Input Reference Pose or previous bone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers Raw Preset")
	FVRIK_FingerRawRotation Pinky;

	bool operator==(const FVRIK_FingersRawPreset& Other) const
	{
		return (Thumb == Other.Thumb && Index == Other.Index && Middle == Other.Middle && Ring == Other.Ring && Pinky == Other.Pinky);
	}
	bool operator!=(const FVRIK_FingersRawPreset& Other) const
	{
		return !(*this == Other);
	}
};

namespace HandHelpers
{
	bool GetHandFingersRotation(UObject* WorldContext, bool bRightHand, FVRIK_FingersRawPreset& OutFingersRotation);

	/* Convert detailed fingers rotation to simplified rotation. This function is used for replication. */
	void PackFingerRotationToArray(const FVRIK_FingerRawRotation& InFingerRotation, TArray<uint8>& CompressedRotation, int32& Index);
	/* Extract finger rotation from array. This function is used for replication. */
	bool UnpackFingerRotationFromArray(FVRIK_FingerRawRotation& OutFingerRotation, const TArray<uint8>& FingersArray, int32& Index);

	void FingerInterpTo(FVRIK_FingerRawRotation& InOutFinger, const FVRIK_FingerRawRotation& Target, float DeltaTime, float Speed);
};