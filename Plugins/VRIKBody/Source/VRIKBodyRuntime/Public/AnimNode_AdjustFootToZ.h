// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_AdjustFootToZ.generated.h"

/* The Two Bone IK control applies an inverse kinematic (IK) solver to a leg 3-joint chain to adjust foot z to desired ground level */
USTRUCT()
struct VRIKBODYRUNTIME_API FAnimNode_AdjustFootToZ : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

public:
	FAnimNode_AdjustFootToZ();

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category = "IK")
	FBoneReference IKBone;

	/** Set to true to disable IK adjustment for ground Z coordinate below current foot Z coordinate ***/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector", meta = (PinHiddenByDefault))
	bool IgnoreGroundBelowFoot;

	/** Effector Location. Target Location to reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector", meta = (PinShownByDefault))
	float GroundLevelZ;

	/** Vertical rotation axis for pelvis bone (for Mannequin it's X) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector")
	TEnumAsByte<EAxis::Type> PelvisUpAxis;

	/** Use 1 or -1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector")
	float PelvisUpAxisMultiplier;

	/** Should use pelvis rotation from variable? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector")
	bool bOverridePelvisForwardDirection;

	/** Rotation of pelvis around up axis in component space, typically 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndEffector", meta = (PinHiddenByDefault))
	float PelvisRotationOverride;

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface
};