// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "AnimNode_ApplyLegsBlendSpace.h"
#include "VRIKSkeletalMeshTranslator.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetAnimationLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SceneManagement.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

DECLARE_CYCLE_STAT(TEXT("ApplyLegsBlendSpace Eval"), STAT_ApplyLegsBlendSpace_Eval, STATGROUP_Anim);

#ifndef _uev_backward_comp
#define _uev_backward_comp

#if ENGINE_MAJOR_VERSION < 5
#define _uev_IsBoneIndexValid(bi) (bi.GetInt() != INDEX_NONE)
#define _uev_GetCrouchedHalfHeight(charmov) charmov->CrouchedHalfHeight
#else
#define _uev_IsBoneIndexValid(bi) bi.IsValid()
#define _uev_GetCrouchedHalfHeight(charmov) charmov->GetCrouchedHalfHeight()
#endif
#endif

#ifndef __rotator_direction
#define __rotator_direction(Rotator, Axis) FRotationMatrix(Rotator).GetScaledAxis(Axis)
#endif

#define __dir_forward(Rotator, FO) FRotationMatrix(Rotator).GetScaledAxis(FO.ForwardAxis) * FO.ForwardDirection
#define __dir_up(Rotator, FO) FRotationMatrix(Rotator).GetScaledAxis(FO.VerticalAxis) * FO.UpDirection

FAnimNode_ApplyLegsBlendSpace::FAnimNode_ApplyLegsBlendSpace()
	: BodyTranslator(nullptr)
	, DebugYaw(0.f)
	, OrientationCorrection(FRotator::ZeroRotator)
	, bAutoFootToGround(false)
	, bEffectorsInWorldSpace(false)
	, Alpha(1.f)
	, bValidBones(false)
	, FootRightPoseIndex(INDEX_NONE)
	, CalfRightPoseIndex(INDEX_NONE)
	, ThighRightPoseIndex(INDEX_NONE)
	, FootLeftPoseIndex(INDEX_NONE)
	, CalfLeftPoseIndex(INDEX_NONE)
	, ThighLeftPoseIndex(INDEX_NONE)
	, ControlledMesh(nullptr)
{
}

void FAnimNode_ApplyLegsBlendSpace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	BodyPose.Initialize(Context);
	LegsPose.Initialize(Context);

	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	RightFootBone.Initialize(BoneContainer);
	LeftFootBone.Initialize(BoneContainer);

	if (bAutoFootToGround)
	{
		ControlledMesh = Context.AnimInstanceProxy->GetSkelMeshComponent();
		if (IsValid(ControlledMesh) && bAutoFootToGround)
		{
			USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();
			if (IsValid(Skeleton))
			{
				bAutoFootToGround
					= InitializeLegIK(RightLegIK, Skeleton, RightFootBone)
					&& InitializeLegIK(LeftLegIK, Skeleton, LeftFootBone);

				if (!bAutoFootToGround)
				{
					UE_LOG(LogTemp, Log, TEXT("ApplyLegsBlendSpace: Couldn't initiailze legs for Foot IK"));
				}
			}
		}
	}
}

bool FAnimNode_ApplyLegsBlendSpace::InitializeBoneIndices(const FBoneContainer& BoneContainer)
{
	bValidBones = false;
	if (BoneContainer.IsValid())
	{
		int32 BonesLastIndex = BoneContainer.GetBoneIndicesArray().Num() - 1;

		FootRightPoseIndex = RightFootBone.GetCompactPoseIndex(BoneContainer);		
		if (!_uev_IsBoneIndexValid(FootRightPoseIndex) || FootRightPoseIndex.GetInt() > BonesLastIndex) return bValidBones;
		CalfRightPoseIndex = BoneContainer.GetParentBoneIndex(FootRightPoseIndex);
		if (!_uev_IsBoneIndexValid(CalfRightPoseIndex) || CalfRightPoseIndex.GetInt() > BonesLastIndex) return bValidBones;
		ThighRightPoseIndex = BoneContainer.GetParentBoneIndex(CalfRightPoseIndex);
		if (!_uev_IsBoneIndexValid(ThighRightPoseIndex) || ThighRightPoseIndex.GetInt() > BonesLastIndex) return bValidBones;

		FootLeftPoseIndex = LeftFootBone.GetCompactPoseIndex(BoneContainer);
		if (!_uev_IsBoneIndexValid(FootLeftPoseIndex) || FootLeftPoseIndex.GetInt() > BonesLastIndex) return bValidBones;
		CalfLeftPoseIndex = BoneContainer.GetParentBoneIndex(FootLeftPoseIndex);
		if (!_uev_IsBoneIndexValid(CalfLeftPoseIndex) || CalfLeftPoseIndex.GetInt() > BonesLastIndex) return bValidBones;
		ThighLeftPoseIndex = BoneContainer.GetParentBoneIndex(CalfLeftPoseIndex);
		if (!_uev_IsBoneIndexValid(ThighLeftPoseIndex) || ThighLeftPoseIndex.GetInt() > BonesLastIndex) return bValidBones;

		bValidBones = true;
	}

	return bValidBones;
}

void FAnimNode_ApplyLegsBlendSpace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 21
	GetEvaluateGraphExposedInputs().Execute(Context);
#else
	EvaluateGraphExposedInputs.Execute(Context);
#endif
	BodyPose.Update(Context);
	LegsPose.Update(Context);
}

void FAnimNode_ApplyLegsBlendSpace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	BodyPose.CacheBones(Context);
	LegsPose.CacheBones(Context);
}

void FAnimNode_ApplyLegsBlendSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("Alpha (%.1f%%)"), Alpha * 100.f);
	DebugData.AddDebugItem(DebugLine);

	BodyPose.GatherDebugData(DebugData);
}

void FAnimNode_ApplyLegsBlendSpace::Evaluate_AnyThread(FPoseContext& Output)
{
	// Evaluate incoming pose into our output buffer.
	BodyPose.Evaluate(Output);

	if (!FAnimWeight::IsRelevant(Alpha))
	{
		return;
	}
	if (PelvisBone.BoneName.IsNone() || FirstSpineBone.BoneName.IsNone())
	{
		return;
	}

	FRotator Pelvis_Add_CS = FRotator::ZeroRotator;
	if (IsValid(BodyTranslator))
	{
		if (BodyTranslator->bRootMotion)
		{
			if (BodyTranslator->bLockRootBoneRotation)
			{
				Pelvis_Add_CS.Yaw = BodyTranslator->PelvisRotator_WS.Yaw + BodyTranslator->ComponentForwardYaw;
			}
			else
			{
				Pelvis_Add_CS.Yaw = BodyTranslator->ComponentForwardYaw;
			}
		}
		else
		{
			Pelvis_Add_CS.Yaw = BodyTranslator->PelvisRotator_WS.Yaw;
		}
	}
	else
	{
		Pelvis_Add_CS.Yaw = DebugYaw;
	}
	Pelvis_Add_CS = UKismetMathLibrary::ComposeRotators(Pelvis_Add_CS, OrientationCorrection);

	const FBoneContainer& BoneContainer = Output.AnimInstanceProxy->GetRequiredBones();
	FPoseContext LegsContext(Output);
	LegsPose.Evaluate(LegsContext);

	// Array of bone transforms
	FCompactPose& OutPose = Output.Pose;
	const FCompactPose& LegsInPose = LegsContext.Pose;

	if (PelvisBone.BoneIndex == INDEX_NONE)
	{
		PelvisBone.Initialize(BoneContainer);
	}
	FCompactPoseBoneIndex Pelvis_PoseIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	if (FirstSpineBone.BoneIndex == INDEX_NONE)
	{
		FirstSpineBone.Initialize(BoneContainer);
	}
	FCompactPoseBoneIndex Spine_PoseIndex = FirstSpineBone.GetCompactPoseIndex(BoneContainer);

	const FTransform PelvisLocalTransform = OutPose[Pelvis_PoseIndex];
	const FTransform SpineLocalTransform = OutPose[Spine_PoseIndex];
	const FTransform SpineTransform = SpineLocalTransform * PelvisLocalTransform;
	FTransform PelvisLocalTransform_Legs = LegsInPose[Pelvis_PoseIndex];

	// apply to pelvis and spine
	OutPose[Pelvis_PoseIndex].SetRotation(PelvisLocalTransform_Legs.GetRotation());

	TSet<int32> BonesBeforePelvis;

	// get CS pelvis transform
	FTransform Pelvis_CS = OutPose[Pelvis_PoseIndex];
	FCompactPoseBoneIndex PelvisParent = OutPose.GetParentBoneIndex(Pelvis_PoseIndex);
	if (PelvisParent != INDEX_NONE)
	{
		BonesBeforePelvis.Add(PelvisParent.GetInt());

		Pelvis_CS = Pelvis_CS * OutPose[PelvisParent];
		int32 PrevIndex = PelvisParent.GetInt();
		int32 NextParent = OutPose.GetParentBoneIndex(FCompactPoseBoneIndex(PrevIndex)).GetInt();
		int32 Counter = 0;
		while (NextParent != INDEX_NONE && Counter++ < 255)
		{
			BonesBeforePelvis.Add(NextParent);

			FCompactPoseBoneIndex pi = FCompactPoseBoneIndex(PrevIndex);
			Pelvis_CS = Pelvis_CS * OutPose[pi];
			NextParent = OutPose.GetParentBoneIndex(pi).GetInt();

			if (pi.IsRootBone()) NextParent = INDEX_NONE;
		}
	}

	// add rotation to pelvis in component space
	Pelvis_CS.SetRotation(UKismetMathLibrary::ComposeRotators(Pelvis_CS.Rotator(), Pelvis_Add_CS).Quaternion());

	// convert back to local space
	if (PelvisParent == INDEX_NONE)
	{
		OutPose[Pelvis_PoseIndex].SetRotation(Pelvis_CS.GetRotation());
	}
	else
	{
		OutPose[Pelvis_PoseIndex].SetRotation(Pelvis_CS.GetRelativeTransform(OutPose[PelvisParent]).GetRotation());
	}

	// make sure spine has correct location and rotation
	OutPose[Spine_PoseIndex] = SpineTransform.GetRelativeTransform(OutPose[Pelvis_PoseIndex]);

	// blend legs
	for (const FCompactPoseBoneIndex BoneIndex : OutPose.ForEachBoneIndex())
	{
		if (BoneIndex.GetInt() == Pelvis_PoseIndex.GetInt())
		{
			continue;
		}
		if (BonesBeforePelvis.Contains(BoneIndex.GetInt()))
		{
			continue;
		}

		if (BoneContainer.GetDepthBetweenBones(BoneIndex.GetInt(), Spine_PoseIndex.GetInt()) == INDEX_NONE)
		{
			OutPose[BoneIndex] = LegsInPose[BoneIndex];
		}
	}

	// apply IK to legs?
	if (bAutoFootToGround)
	{
		if (InitializeBoneIndices(BoneContainer))
		{
			bool bRightLegIK = (FootRightTarget != FVector::ZeroVector);
			bool bLeftLegIK = (FootLeftTarget != FVector::ZeroVector);

			FTransform tIkFootR = GetBonePositionInComponentSpaceByCompactIndex(BoneContainer, OutPose, FootRightPoseIndex);
			FTransform tIkFootL = GetBonePositionInComponentSpaceByCompactIndex(BoneContainer, OutPose, FootLeftPoseIndex);

			const FVector FootAddend = FVector(0.f, 0.f, FootVerticalOffset);
			FVector RightTargetCS, LeftTargetCS;
			if (bEffectorsInWorldSpace && IsValid(ControlledMesh))
			{
				FTransform ComponentTr = ControlledMesh->GetComponentTransform();
				if (bRightLegIK) RightTargetCS = FTransform(FootRightTarget + FootAddend).GetRelativeTransform(ComponentTr).GetTranslation();
				if (bLeftLegIK) LeftTargetCS = FTransform(FootLeftTarget + FootAddend).GetRelativeTransform(ComponentTr).GetTranslation();
			}
			else
			{
				RightTargetCS = FootRightTarget + FootAddend;
				LeftTargetCS = FootLeftTarget + FootAddend;
			}

			if (FMath::Abs(RightTargetCS.Z) > 50.f)
			{
				bRightLegIK = false;
			}
			else
			{
				RightTargetCS.Z = FMath::Clamp(RightTargetCS.Z, -30.f, 30.f);
			}
			if (FMath::Abs(LeftTargetCS.Z) > 50.f)
			{
				bLeftLegIK = false;
			}
			else
			{
				LeftTargetCS.Z = FMath::Clamp(LeftTargetCS.Z, -30.f, 30.f);
			}

			if (bRightLegIK)
			{
				tIkFootR.SetTranslation(FVector(tIkFootR.GetTranslation().X, tIkFootR.GetTranslation().Y, FMath::Max(tIkFootR.GetTranslation().Z, RightTargetCS.Z)));
				DoLegIK(BoneContainer, OutPose, tIkFootR, 0.f, RightLegIK, ThighRightPoseIndex, CalfRightPoseIndex, FootRightPoseIndex, true, 1.f);
			}
			if (bLeftLegIK)
			{
				tIkFootL.SetTranslation(FVector(tIkFootL.GetTranslation().X, tIkFootL.GetTranslation().Y, FMath::Max(tIkFootL.GetTranslation().Z, LeftTargetCS.Z)));
				DoLegIK(BoneContainer, OutPose, tIkFootL, 0.f, LeftLegIK, ThighLeftPoseIndex, CalfLeftPoseIndex, FootLeftPoseIndex, true, 1.f);
			}
		}
	}
}

bool FAnimNode_ApplyLegsBlendSpace::InitializeLegIK(FVRIKLegIKSetup& LegIK, USkeleton* Skeleton, const FBoneReference& FootBone) const
{
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	int32 FootBoneSkeletonIndex = RefSkeleton.FindBoneIndex(FootBone.BoneName);
	if (FootBoneSkeletonIndex == INDEX_NONE) return false;
	int32 CalfBoneSkeletonIndex = RefSkeleton.GetParentIndex(FootBoneSkeletonIndex);
	if (CalfBoneSkeletonIndex == INDEX_NONE) return false;
	int32 ThighBoneSkeletonIndex = RefSkeleton.GetParentIndex(CalfBoneSkeletonIndex);
	if (ThighBoneSkeletonIndex == INDEX_NONE) return false;

	// Get bone transforms in component space
	const FTransform TrThigh = GetBoneRefPositionInComponentSpaceByIndex(Skeleton, ThighBoneSkeletonIndex);
	const FTransform TrCalf = RefPoseSpaceBaseTMs[CalfBoneSkeletonIndex] * TrThigh;
	const FTransform TrFoot = RefPoseSpaceBaseTMs[FootBoneSkeletonIndex] * TrCalf;

	// character orientation
	FVector CharForward = FVector::RightVector;
	FVector CharUp = FVector::UpVector;
	const FVector CharRight = CharUp ^ CharForward;

	float Val;

	// Leg right axis and orientation converter
	LegIK.LegRightAxis = FindCoDirection(TrThigh.Rotator(), CharRight, Val);
	FRotator GenericLegBoneRotation = UKismetMathLibrary::MakeRotFromXY(TrCalf.GetTranslation() - TrThigh.GetTranslation(), __rotator_direction(TrThigh.Rotator(), LegIK.LegRightAxis));
	LegIK.LegOrientationConverter = TrThigh.GetRelativeTransform(FTransform(GenericLegBoneRotation, TrThigh.GetTranslation()));

	// Joint target offset
	FVector JointTargetPos = TrCalf.GetTranslation() + CharForward * 15.f;
	LegIK.JointTargetOffset = FTransform(JointTargetPos).GetRelativeTransform(TrCalf);

	// Foot axis and direction
	LegIK.FootOrientation.ForwardAxis = FindCoDirection(TrFoot.Rotator(), CharForward, LegIK.FootOrientation.ForwardDirection);
	LegIK.FootOrientation.HorizontalAxis = FindCoDirection(TrFoot.Rotator(), CharRight, LegIK.FootOrientation.RightDirection);
	LegIK.FootOrientation.VerticalAxis = FindCoDirection(TrFoot.Rotator(), CharUp, LegIK.FootOrientation.UpDirection);

	return true;
}

/* Find axis of rotator the closest to being parallel to the specified vectors. Returns +1.f in Multiplier if co-directed and -1.f otherwise */
EAxis::Type FAnimNode_ApplyLegsBlendSpace::FindCoDirection(const FRotator& BoneRotator, const FVector& Direction, float& ResultMultiplier) const
{
	EAxis::Type RetAxis;
	FVector dir = Direction;
	dir.Normalize();

	const FVector v1 = __rotator_direction(BoneRotator, EAxis::X);
	const FVector v2 = __rotator_direction(BoneRotator, EAxis::Y);
	const FVector v3 = __rotator_direction(BoneRotator, EAxis::Z);

	const float dp1 = FVector::DotProduct(v1, dir);
	const float dp2 = FVector::DotProduct(v2, dir);
	const float dp3 = FVector::DotProduct(v3, dir);

	if (FMath::Abs(dp1) > FMath::Abs(dp2) && FMath::Abs(dp1) > FMath::Abs(dp3)) {
		RetAxis = EAxis::X;
		ResultMultiplier = dp1 > 0.f ? 1.f : -1.f;
	}
	else if (FMath::Abs(dp2) > FMath::Abs(dp1) && FMath::Abs(dp2) > FMath::Abs(dp3)) {
		RetAxis = EAxis::Y;
		ResultMultiplier = dp2 > 0.f ? 1.f : -1.f;
	}
	else {
		RetAxis = EAxis::Z;
		ResultMultiplier = dp3 > 0.f ? 1.f : -1.f;
	}

	return RetAxis;
}

FTransform FAnimNode_ApplyLegsBlendSpace::GetBoneRefPositionInComponentSpaceByIndex(const USkeleton* Skeleton, int32 BoneIndex) const
{
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	int32 TransformIndex = BoneIndex;
	FTransform tr_bone = FTransform::Identity;

	// stop on root
	while (TransformIndex != INDEX_NONE)
	{
		tr_bone *= RefPoseSpaceBaseTMs[TransformIndex];
		TransformIndex = RefSkeleton.GetParentIndex(TransformIndex);
	}
	tr_bone.NormalizeRotation();

	return tr_bone;
}

FTransform FAnimNode_ApplyLegsBlendSpace::GetBonePositionInComponentSpaceByCompactIndex(const FBoneContainer& BoneContainer, const FCompactPose& Pose, const FCompactPoseBoneIndex& BoneIndex) const
{
	return GetBonePositionInComponentSpaceByCompactIndexAndBase(BoneContainer, Pose, BoneIndex, FTransform::Identity, FCompactPoseBoneIndex(INDEX_NONE));
}

FTransform FAnimNode_ApplyLegsBlendSpace::GetBonePositionInComponentSpaceByCompactIndexAndBase(
	const FBoneContainer& BoneContainer,
	const FCompactPose& Pose,
	const FCompactPoseBoneIndex& BoneIndex,
	const FTransform& BaseBonePosition,
	const FCompactPoseBoneIndex& BaseBoneIndex) const
{
	FCompactPoseBoneIndex TransformIndex = BoneIndex;
	FTransform tr_bone = FTransform::Identity;

	// Stop on root or base index
	while (_uev_IsBoneIndexValid(TransformIndex) && TransformIndex != BaseBoneIndex)
	{
		tr_bone *= Pose[TransformIndex];
		TransformIndex = BoneContainer.GetParentBoneIndex(TransformIndex);
	}

	// Apply base transform
	if (_uev_IsBoneIndexValid(TransformIndex) && TransformIndex == BaseBoneIndex)
	{
		tr_bone *= BaseBonePosition;
	}
	tr_bone.NormalizeRotation();

	return tr_bone;
}

void FAnimNode_ApplyLegsBlendSpace::DoLegIK(const FBoneContainer& BoneContainer,
	FCompactPose& Pose,
	const FTransform& EffectorTarget,
	float PitchOffset,
	const FVRIKLegIKSetup& LegIK,
	const FCompactPoseBoneIndex& ThighIndex,
	const FCompactPoseBoneIndex& CalfIndex,
	const FCompactPoseBoneIndex& FootIndex,
	bool bRotationFromEffector,
	float ApplyAlpha) const
{
	FCompactPoseBoneIndex PelvisBoneIndex = BoneContainer.GetParentBoneIndex(ThighIndex);
	FTransform TrPelvis = GetBonePositionInComponentSpaceByCompactIndex(BoneContainer, Pose, PelvisBoneIndex);
	FTransform TrThigh = Pose[ThighIndex] * TrPelvis;
	FTransform TrCalf = Pose[CalfIndex] * TrThigh;
	FTransform TrFoot = Pose[FootIndex] * TrCalf;
	FVector JointTarget = (LegIK.JointTargetOffset * TrCalf).GetTranslation();

	//  compute Two Bone IK
	FVector OutCalfLocation, OutFootLocation;
	UKismetAnimationLibrary::K2_TwoBoneIK(
		TrThigh.GetTranslation(),
		TrCalf.GetTranslation(),
		TrFoot.GetTranslation(),
		JointTarget,
		EffectorTarget.GetTranslation(),
		OutCalfLocation,
		OutFootLocation,
		false, 0.5f, 1.f);

	FRotator ThighRotGeneric = UKismetMathLibrary::MakeRotFromXY(OutCalfLocation - TrThigh.GetTranslation(), __rotator_direction(TrThigh.Rotator(), LegIK.LegRightAxis));
	FRotator CalfRotGeneric = UKismetMathLibrary::MakeRotFromXY(OutFootLocation - OutCalfLocation, __rotator_direction(TrCalf.Rotator(), LegIK.LegRightAxis));
	FTransform OutTrThigh = TrThigh; OutTrThigh.SetRotation((LegIK.LegOrientationConverter * FTransform(ThighRotGeneric, TrThigh.GetTranslation())).GetRotation());
	FTransform OutTrCalf = FTransform((LegIK.LegOrientationConverter * FTransform(CalfRotGeneric, TrCalf.GetTranslation())).GetRotation(), OutCalfLocation);
	FTransform OutTrFoot = FTransform(TrFoot.GetRotation(), OutFootLocation);

	// should apply pitch offset
	if (bRotationFromEffector)
	{
		OutTrFoot.SetRotation(EffectorTarget.GetRotation());
	}
	if (PitchOffset != 0.f)
	{
		float PitchValue = PitchOffset * LegIK.FootOrientation.RightDirection;
		if (LegIK.FootOrientation.HorizontalAxis == EAxis::Type::Z) PitchValue *= -1.f;
		FRotator r = OutTrFoot.Rotator();
		r.Pitch += PitchValue;
		r.Normalize();
		OutTrFoot.SetRotation(r.Quaternion());
	}

	if (ApplyAlpha == 1.f)
	{
		Pose[ThighIndex].SetRotation(OutTrThigh.GetRelativeTransform(TrPelvis).GetRotation());
		Pose[CalfIndex].SetRotation(OutTrCalf.GetRelativeTransform(OutTrThigh).GetRotation());
		Pose[FootIndex].SetRotation(OutTrFoot.GetRelativeTransform(OutTrCalf).GetRotation());
	}
	else
	{
		FQuat OutRotThigh = OutTrThigh.GetRelativeTransform(TrPelvis).GetRotation();
		FQuat OutRotCalf = OutTrCalf.GetRelativeTransform(OutTrThigh).GetRotation();
		FQuat OutRotFoot = OutTrFoot.GetRelativeTransform(OutTrCalf).GetRotation();

		Pose[ThighIndex].SetRotation(FQuat::Slerp(Pose[ThighIndex].GetRotation(), OutRotThigh, ApplyAlpha));
		Pose[CalfIndex].SetRotation(FQuat::Slerp(Pose[CalfIndex].GetRotation(), OutRotCalf, ApplyAlpha));
		Pose[FootIndex].SetRotation(FQuat::Slerp(Pose[FootIndex].GetRotation(), OutRotFoot, ApplyAlpha));
	}
}