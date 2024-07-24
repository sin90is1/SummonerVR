// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AnimNode_ApplyLegsBlendSpace.generated.h"

class UVRIKSkeletalMeshTranslator;

/** Custom-to-universal skeleton bone orientation translation */
USTRUCT()
struct VRIKBODYRUNTIME_API FVRIKBoneOrientationTransform
{
	GENERATED_USTRUCT_BODY()

	// Forward vector
	EAxis::Type ForwardAxis;
	float ForwardDirection;

	// Right vector
	EAxis::Type HorizontalAxis;
	float RightDirection;

	// Up vector
	EAxis::Type VerticalAxis;
	float UpDirection;

	FVRIKBoneOrientationTransform()
		: ForwardAxis(EAxis::Type::X)
		, ForwardDirection(1.f)
		, HorizontalAxis(EAxis::Type::Y)
		, RightDirection(1.f)
		, VerticalAxis(EAxis::Type::Z)
		, UpDirection(1.f)
	{}
};

/** Settings used for leg Two-Bone IK, including offset of joint target */
USTRUCT()
struct VRIKBODYRUNTIME_API FVRIKLegIKSetup
{
	GENERATED_USTRUCT_BODY()

	// Convert generic XY-orientation to skeleton orientation of thigh and calf bones
	UPROPERTY()
	FTransform LegOrientationConverter;

	// Offset from calf bone to forward
	UPROPERTY()
	FTransform JointTargetOffset;

	// Right/left axis (relative to charakter looking forward) for thigh and calf bones
	EAxis::Type LegRightAxis;

	// Full data about orientation of foot bone
	UPROPERTY()
	FVRIKBoneOrientationTransform FootOrientation;

	FVRIKLegIKSetup()
	: LegRightAxis(EAxis::Type::X)
	{}
};

/**
 * Animation node: apply fingers relative transforms to animation data
 */
USTRUCT(BlueprintInternalUseOnly)
struct VRIKBODYRUNTIME_API FAnimNode_ApplyLegsBlendSpace : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	/** Input animation. Use Pose Snapshot for upper body **/
	UPROPERTY(EditAnywhere, Category = "Links")
	FPoseLink BodyPose;

	/** Legs Animation. Use BlendSpace2D **/
	UPROPERTY(EditAnywhere, Category = "Links")
	FPoseLink LegsPose;

	/** Name of hips/pelvis bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton")
	FBoneReference PelvisBone;

	/** Name of first spine bone after pelvis */
	UPROPERTY(EditAnywhere, Category = "Skeleton")
	FBoneReference FirstSpineBone;

	/** Name of right foot bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (EditCondition = "bAutoFootToGround"))
	FBoneReference RightFootBone;

	/** Name of left foot bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (EditCondition = "bAutoFootToGround"))
	FBoneReference LeftFootBone;

	/** Reference to VR IK Skeletal Mesh Translator (use YnnkAvatar or BodyTranslator) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinShownByDefault))
	UVRIKSkeletalMeshTranslator* BodyTranslator;

	/** Reference to Ynnk VR-Avatar (use YnnkAvatar or BodyTranslator) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	class UYnnkVrAvatarComponent* YnnkAvatar;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault, DisplayName="Manual Yaw"))
	float DebugYaw;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	FRotator OrientationCorrection;

	UPROPERTY(EditAnywhere, Category = "Legs")
	bool bAutoFootToGround;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bAutoFootToGround"), Category = "Legs")
	bool bEffectorsInWorldSpace;

	// Target position of right foot in world space or component space
	UPROPERTY(EditAnywhere, Category = "Legs", meta = (PinHiddenByDefault, EditCondition="bAutoFootToGround"))
	FVector FootRightTarget;

	// Target position of left foot in world space or component space
	UPROPERTY(EditAnywhere, Category = "Legs", meta = (PinHiddenByDefault, EditCondition = "bAutoFootToGround"))
	FVector FootLeftTarget;

	UPROPERTY(EditAnywhere, Category = "Legs", meta = (PinHiddenByDefault, EditCondition = "bAutoFootToGround"))
	float FootVerticalOffset = 0.f;

	/** Alpha value **/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	mutable float Alpha;

protected:
	UPROPERTY()
	bool bValidBones;

	TArray<FCompactPoseBoneIndex> BoneIndicesCache;

	// Compact pose bone indices for legs
	FCompactPoseBoneIndex FootRightPoseIndex;
	FCompactPoseBoneIndex CalfRightPoseIndex;
	FCompactPoseBoneIndex ThighRightPoseIndex;
	FCompactPoseBoneIndex FootLeftPoseIndex;
	FCompactPoseBoneIndex CalfLeftPoseIndex;
	FCompactPoseBoneIndex ThighLeftPoseIndex;

	FVRIKLegIKSetup RightLegIK;
	FVRIKLegIKSetup LeftLegIK;

#if ENGINE_MAJOR_VERSION < 5
	// Pointer to owning component
	class USkeletalMeshComponent* ControlledMesh;
#else
	// Pointer to owning component
	TObjectPtr<class USkeletalMeshComponent> ControlledMesh;
#endif

public:
	FAnimNode_ApplyLegsBlendSpace();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

		// Initialize FLegIKSetup struct
	bool InitializeLegIK(FVRIKLegIKSetup& LegIK, USkeleton* Skeleton, const FBoneReference& FootBone) const;
	// Update compact pose bone indices
	bool InitializeBoneIndices(const FBoneContainer& BoneContainer);

	EAxis::Type FindCoDirection(const FRotator& BoneRotator, const FVector& Direction, float& ResultMultiplier) const;
	FTransform GetBoneRefPositionInComponentSpaceByIndex(const USkeleton* Skeleton, int32 BoneIndex) const;
	FTransform GetBonePositionInComponentSpaceByCompactIndex(const FBoneContainer& BoneContainer, const FCompactPose& Pose, const FCompactPoseBoneIndex& BoneIndex) const;
	FTransform GetBonePositionInComponentSpaceByCompactIndexAndBase(
		const FBoneContainer& BoneContainer,
		const FCompactPose& Pose,
		const FCompactPoseBoneIndex& BoneIndex,
		const FTransform& BaseBonePosition,
		const FCompactPoseBoneIndex& BaseBoneIndex) const;
	void DoLegIK(const FBoneContainer& BoneContainer,
		FCompactPose& Pose,
		const FTransform& EffectorTarget,
		float PitchOffset,
		const FVRIKLegIKSetup& LegIK,
		const FCompactPoseBoneIndex& ThighIndex,
		const FCompactPoseBoneIndex& CalfIndex,
		const FCompactPoseBoneIndex& FootIndex,
		bool bRotationFromEffector,
		float ApplyAlpha) const;
};