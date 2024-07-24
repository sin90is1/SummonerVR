// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#include "AnimNode_AdjustFootToZ.h"
#include "VRIKBodyPrivatePCH.h"
#include "AnimationRuntime.h"
#include "AnimInstanceProxy.h"
#include "Runtime/Launch/Resources/Version.h"

DECLARE_CYCLE_STAT(TEXT("AdjustFootToGround Eval"), STAT_AdjustFootToGround_Eval, STATGROUP_Anim);

FAnimNode_AdjustFootToZ::FAnimNode_AdjustFootToZ()
	: IgnoreGroundBelowFoot(true)
	, GroundLevelZ(0.f)
	, PelvisUpAxis(EAxis::Type::X)
	, PelvisUpAxisMultiplier(1.f)
	, bOverridePelvisForwardDirection(false)
	, PelvisRotationOverride(0.f)
{
}

void FAnimNode_AdjustFootToZ::GatherDebugData(FNodeDebugData& DebugData)
{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 24
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
#endif

	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" IKBone: %s)"), *IKBone.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_AdjustFootToZ::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 24
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
#endif
	SCOPE_CYCLE_COUNTER(STAT_AdjustFootToGround_Eval);

	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	// Get indices of the lower and upper limb bones and check validity.
	bool bInvalidLimb = false;

	FCompactPoseBoneIndex IKBoneCompactPoseIndex = IKBone.GetCompactPoseIndex(BoneContainer);

	const FCompactPoseBoneIndex LowerLimbIndex = BoneContainer.GetParentBoneIndex(IKBoneCompactPoseIndex);
	if (LowerLimbIndex == INDEX_NONE)
	{
		bInvalidLimb = true;
	}

	const FCompactPoseBoneIndex UpperLimbIndex = BoneContainer.GetParentBoneIndex(LowerLimbIndex);
	if (UpperLimbIndex == INDEX_NONE)
	{
		bInvalidLimb = true;
	}
	FCompactPoseBoneIndex PelvisIndex = BoneContainer.GetParentBoneIndex(UpperLimbIndex);
	if (UpperLimbIndex == INDEX_NONE)
	{
		bInvalidLimb = true;
	}
	else
	{
		const FCompactPoseBoneIndex PelvisIndex1 = BoneContainer.GetParentBoneIndex(PelvisIndex);

		if (PelvisIndex1 != INDEX_NONE)
		{
			PelvisIndex = PelvisIndex1;
		}
	}

	const bool bInBoneSpace = false;
	const int32 EffectorBoneIndex = INDEX_NONE;
	const FCompactPoseBoneIndex EffectorSpaceBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(EffectorBoneIndex));

	// If we walked past the root, this controlled is invalid, so return no affected bones.
	if (bInvalidLimb)
	{
		return;
	}

	// Get Local Space transforms for our bones. We do this first in case they already are local.
	// As right after we get them in component space. (And that does the auto conversion).
	// We might save one transform by doing local first...

	const FTransform EndBoneLocalTransform = Output.Pose.GetLocalSpaceTransform(IKBoneCompactPoseIndex);

	// Now get those in component space...
	FTransform LowerLimbCSTransform = Output.Pose.GetComponentSpaceTransform(LowerLimbIndex);
	FTransform UpperLimbCSTransform = Output.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	FTransform EndBoneCSTransform = Output.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
	FTransform PelvisCSTransform = Output.Pose.GetComponentSpaceTransform(PelvisIndex);

	FVector JointTargetLocation = LowerLimbCSTransform.GetTranslation();

	//const FVector EffectorLocation = EndBoneCSTransform.GetTranslation();
	const FVector EffectorLocation = FVector(0.f, 0.f, GroundLevelZ);
	//EffectorLocation.Z = GroundLevelZ;

	// Get current position of root of limb.
	// All position are in Component space.
	const FVector RootPos = UpperLimbCSTransform.GetTranslation();
	const FVector InitialJointPos = LowerLimbCSTransform.GetTranslation();
	const FVector InitialEndPos = EndBoneCSTransform.GetTranslation();

	// Transform EffectorLocation from EffectorLocationSpace to ComponentSpace.
	FTransform EffectorTransform(EffectorLocation);

	const USkeletalMeshComponent* SkelMesh = Output.AnimInstanceProxy->GetSkelMeshComponent();
	if (!SkelMesh)
	{
		return;
	}

	FAnimationRuntime::ConvertBoneSpaceTransformToCS(SkelMesh->GetComponentTransform(), Output.Pose, EffectorTransform, EffectorSpaceBoneIndex, EBoneControlSpace::BCS_WorldSpace);

	// Return if ground is lower then foot
	if (IgnoreGroundBelowFoot && EffectorTransform.GetTranslation().Z < InitialEndPos.Z)
	{
		return;
	}

	EffectorTransform.SetTranslation(FVector(InitialEndPos.X, InitialEndPos.Y, EffectorTransform.GetTranslation().Z));

	// joint forward increment in a horizontal plane
	const float JointForwardOffset = (EffectorTransform.GetTranslation().Z - EndBoneCSTransform.GetTranslation().Z);
	float OffsetSin, OffsetCos;
	float PelvisRot = 0.f;

	float LegsToJointDP = FVector::DotProduct(JointTargetLocation - UpperLimbCSTransform.GetTranslation(), EndBoneCSTransform.GetTranslation() - UpperLimbCSTransform.GetTranslation());
	if (LegsToJointDP > 0.9f)
	{
		float BlendAlpha = (LegsToJointDP - 0.9f) * 10.f;
		if (bOverridePelvisForwardDirection)
		{
			PelvisRot = PelvisRotationOverride;
		}
		else
		{
			switch (PelvisUpAxis)
			{
				case EAxis::Type::X: PelvisRot = PelvisCSTransform.Rotator().Roll; break;
				case EAxis::Type::Y: PelvisRot = PelvisCSTransform.Rotator().Pitch; break;
				case EAxis::Type::Z: PelvisRot = PelvisCSTransform.Rotator().Roll; break;
			}
			PelvisRot *= PelvisUpAxisMultiplier;
		}
		FMath::SinCos(&OffsetSin, &OffsetCos, PelvisRot);

		FVector NewJointTargetLocation = FVector(JointTargetLocation.X + 15.f * OffsetSin, JointTargetLocation.Y + 15.f * OffsetCos, JointTargetLocation.Z);
		JointTargetLocation = FMath::Lerp(JointTargetLocation, NewJointTargetLocation, BlendAlpha);
	}

	// This is our reach goal.
	FVector DesiredPos = EffectorTransform.GetTranslation();
	FVector DesiredDelta = DesiredPos - RootPos;
	float DesiredLength = DesiredDelta.Size();

	// Perform IK if foot position is lower then ground only
	if (DesiredDelta.Z < 0.f) {

		// Check to handle case where DesiredPos is the same as RootPos.
		FVector	DesiredDir;
		if (DesiredLength < (float)KINDA_SMALL_NUMBER)
		{
			DesiredLength = (float)KINDA_SMALL_NUMBER;
			DesiredDir = FVector(1, 0, 0);
		}
		else
		{
			DesiredDir = DesiredDelta / DesiredLength;
		}

		// Get joint target (used for defining plane that joint should be in).
		FTransform JointTargetTransform(JointTargetLocation);
		FCompactPoseBoneIndex JointTargetSpaceBoneIndex(INDEX_NONE);

		FVector	JointTargetPos = JointTargetTransform.GetTranslation();
		FVector JointTargetDelta = JointTargetPos - RootPos;
		const float JointTargetLengthSqr = JointTargetDelta.SizeSquared();

		// Same check as above, to cover case when JointTarget position is the same as RootPos.
		FVector JointPlaneNormal, JointBendDir;
		if (JointTargetLengthSqr < FMath::Square((float)KINDA_SMALL_NUMBER))
		{
			JointBendDir = FVector(0, 1, 0);
			JointPlaneNormal = FVector(0, 0, 1);
		}
		else
		{
			// cross-product to get normal
			JointPlaneNormal = DesiredDir ^ JointTargetDelta;

			// If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
			// we have to just pick 2 random vector perp to DesiredDir and each other.
			if (JointPlaneNormal.SizeSquared() < FMath::Square((float)KINDA_SMALL_NUMBER))
			{
				DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
			}
			else
			{
				JointPlaneNormal.Normalize();

				// Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
				// This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
				JointBendDir = JointTargetDelta - ((JointTargetDelta | DesiredDir) * DesiredDir);
				JointBendDir.Normalize();
			}
		}

		// Find lengths of upper and lower limb in the ref skeleton.
		// Use actual sizes instead of ref skeleton, so we take into account translation and scaling from other bone controllers.
		float LowerLimbLength = (InitialEndPos - InitialJointPos).Size();
		float UpperLimbLength = (InitialJointPos - RootPos).Size();
		float MaxLimbLength = LowerLimbLength + UpperLimbLength;

		FVector OutEndPos = DesiredPos;
		FVector OutJointPos = InitialJointPos;

		// If we are trying to reach a goal beyond the length of the limb, clamp it to something solvable and extend limb fully.
		if (DesiredLength > MaxLimbLength)
		{
			OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
			OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
		}
		else
		{
			// So we have a triangle we know the side lengths of. We can work out the angle between DesiredDir and the direction of the upper limb
			// using the sin rule:
			const float TwoAB = 2.f * UpperLimbLength * DesiredLength;

			const float CosAngle = (TwoAB != 0.f) ? ((UpperLimbLength*UpperLimbLength) + (DesiredLength*DesiredLength) - (LowerLimbLength*LowerLimbLength)) / TwoAB : 0.f;

			// If CosAngle is less than 0, the upper arm actually points the opposite way to DesiredDir, so we handle that.
			const bool bReverseUpperBone = (CosAngle < 0.f);

			// If CosAngle is greater than 1.f, the triangle could not be made - we cannot reach the target.
			// We just have the two limbs double back on themselves, and EndPos will not equal the desired EffectorLocation.
			if ((CosAngle > 1.f) || (CosAngle < -1.f))
			{
				// Because we want the effector to be a positive distance down DesiredDir, we go back by the smaller section.
				if (UpperLimbLength > LowerLimbLength)
				{
					OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
					OutEndPos = OutJointPos - (LowerLimbLength * DesiredDir);
				}
				else
				{
					OutJointPos = RootPos - (UpperLimbLength * DesiredDir);
					OutEndPos = OutJointPos + (LowerLimbLength * DesiredDir);
				}
			}
			else
			{
				// Angle between upper limb and DesiredDir
				const float Angle = FMath::Acos(CosAngle);

				// Now we calculate the distance of the joint from the root -> effector line.
				// This forms a right-angle triangle, with the upper limb as the hypotenuse.
				const float JointLineDist = UpperLimbLength * FMath::Sin(Angle);

				// And the final side of that triangle - distance along DesiredDir of perpendicular.
				// ProjJointDistSqr can't be neg, because JointLineDist must be <= UpperLimbLength because appSin(Angle) is <= 1.
				const float ProjJointDistSqr = (UpperLimbLength*UpperLimbLength) - (JointLineDist*JointLineDist);
				// although this shouldn't be ever negative, sometimes Xbox release produces -0.f, causing ProjJointDist to be NaN
				// so now I branch it. 						
				float ProjJointDist = (ProjJointDistSqr > 0.f) ? FMath::Sqrt(ProjJointDistSqr) : 0.f;
				if (bReverseUpperBone)
				{
					ProjJointDist *= -1.f;
				}

				// So now we can work out where to put the joint!
				OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
			}
		}

		// Update transform for upper bone.
		{
			// Get difference in direction for old and new joint orientations
			FVector const OldDir = (InitialJointPos - RootPos).GetSafeNormal();
			FVector const NewDir = (OutJointPos - RootPos).GetSafeNormal();
			// Find Delta Rotation take takes us from Old to New dir
			FQuat const DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
			// Rotate our Joint quaternion by this delta rotation
			UpperLimbCSTransform.SetRotation(DeltaRotation * UpperLimbCSTransform.GetRotation());
			// And put joint where it should be.
			UpperLimbCSTransform.SetTranslation(RootPos);

			// Order important. First bone is upper limb.
			OutBoneTransforms.Add(FBoneTransform(UpperLimbIndex, UpperLimbCSTransform));
		}

		// Update transform for lower bone.
		{
			// Get difference in direction for old and new joint orientations
			FVector const OldDir = (InitialEndPos - InitialJointPos).GetSafeNormal();
			FVector const NewDir = (OutEndPos - OutJointPos).GetSafeNormal();

			// Find Delta Rotation take takes us from Old to New dir
			FQuat const DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
			// Rotate our Joint quaternion by this delta rotation
			LowerLimbCSTransform.SetRotation(DeltaRotation * LowerLimbCSTransform.GetRotation());
			// And put joint where it should be.
			LowerLimbCSTransform.SetTranslation(OutJointPos);

			// Order important. Second bone is lower limb.
			OutBoneTransforms.Add(FBoneTransform(LowerLimbIndex, LowerLimbCSTransform));
		}

		// Update transform for end bone.
		{
			// only allow bTakeRotationFromEffectorSpace during bone space

			// Set correct location for end bone.
			EndBoneCSTransform.SetTranslation(OutEndPos);

			// Order important. Third bone is End Bone.
			OutBoneTransforms.Add(FBoneTransform(IKBoneCompactPoseIndex, EndBoneCSTransform));
		}

		// Make sure we have correct number of bones
		check(OutBoneTransforms.Num() == 3);
	}
}

bool FAnimNode_AdjustFootToZ::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// if both bones are valid
	return (IKBone.IsValidToEvaluate(RequiredBones));
}

void FAnimNode_AdjustFootToZ::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
#if ENGINE_MINOR_VERSION >= 24
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)
#endif
	IKBone.Initialize(RequiredBones);
}

void FAnimNode_AdjustFootToZ::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
#if ENGINE_MINOR_VERSION >= 24
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
#endif
	Super::Initialize_AnyThread(Context);
}