// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#include "YnnkVRIKTypes.h"
#include "Kismet/KismetMathLibrary.h"
#include "ReferenceSkeleton.h"

FTransform YnnkHelpers::RestoreRefBonePose(const FReferenceSkeleton& RefSkeleton, const int32 InBoneIndex)
{
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	int32 TransformIndex = InBoneIndex;
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

FRotator YnnkHelpers::CalculateRootRotation(const FVector& ForwardVector, const FVector& UpVector)
{
	FVector FxyN = FVector(ForwardVector.X, ForwardVector.Y, 0.f).GetSafeNormal();
	const FVector Uxy = FVector(UpVector.X, UpVector.Y, 0.f);
	const FVector UxyN = Uxy.GetSafeNormal();

	// squared limit value
	const float TurningTreshold = 0.7f * 0.7f;

	// projection of the forward vector on horizontal plane is too small
	const float ProjectionSize = Uxy.SizeSquared();
	if (ProjectionSize > TurningTreshold)
	{
		// interpolation alpha
		const float alpha = FMath::Clamp((ProjectionSize - TurningTreshold) / (1.f - TurningTreshold), 0.f, 1.f);

		// new forward vector
		FVector NewF;
		// choose down or up side and use interpolated vector between forward and up to find current horizontal forward direction
		if (ForwardVector.Z < 0.f)
		{
			NewF = FMath::Lerp((UpVector.Z > 0.f) ? ForwardVector : -ForwardVector, UpVector, alpha);
		}
		else
		{
			NewF = FMath::Lerp((UpVector.Z > 0.f) ? ForwardVector : -ForwardVector, -UpVector, alpha);
		}

		FxyN = FVector(NewF.X, NewF.Y, 0.f).GetSafeNormal();

		float outyaw = FMath::RadiansToDegrees(FMath::Atan2(FxyN.Y, FxyN.X));
		return FRotator(0.f, outyaw, 0.f);
	}
	// projection of the forward vector on horizontal plane is large enough to use it
	else
	{
		const float YawRet = FMath::RadiansToDegrees(FMath::Atan2(FxyN.Y, FxyN.X));

		float outyaw = (UpVector.Z > 0.f) ? YawRet : -YawRet;
		return FRotator(0.f, outyaw, 0.f);
	}
}

void YnnkHelpers::GetRotationDeltaInEulerCoordinates(const FQuat& A, const FQuat& B, float& Roll, float& Pitch, float& Yaw)
{
	FQuat halfRot = FQuat::Slerp(A, B, 0.5);
	FRotator delta1 = FTransform(halfRot).GetRelativeTransform(FTransform(B)).Rotator();
	FRotator delta2 = FTransform(A).GetRelativeTransform(FTransform(halfRot)).Rotator();

	Roll = delta1.Roll + delta2.Roll;
	Pitch = delta1.Pitch + delta2.Pitch;
	Yaw = delta1.Yaw + delta2.Yaw;
}

// Helper function for relative rotation 
void YnnkHelpers::AddRelativeRotation(FRotator& RotatorToModify, const FRotator& AddRotator)
{
	const FQuat CurRelRotQuat = RotatorToModify.Quaternion();
	const FQuat NewRelRotQuat = CurRelRotQuat * AddRotator.Quaternion();

	RotatorToModify = NewRelRotQuat.Rotator();
}

// Calculate IK transforms for two-bone chain
FVector YnnkHelpers::CalculateTwoBoneIK(
	const FVector& ChainStart, const FVector& ChainEnd,
	const FVector& JointTarget,
	const float UpperBoneSize, const float LowerBoneSize,
	FTransform& UpperBone, FTransform& LowerBone,
	bool bIsRight, bool bSnapToChainEnd)
{
	const float DirectSize = FVector::Dist(ChainStart, ChainEnd);
	const float DirectSizeSquared = DirectSize * DirectSize;
	const float a = UpperBoneSize * UpperBoneSize;
	const float b = LowerBoneSize * LowerBoneSize;
	const float SideMul = bIsRight ? 1.f : -1.f;

	// 1) upperbone and lowerbone plane angles
	float Angle1 = FMath::RadiansToDegrees(FMath::Acos((DirectSizeSquared + a - b) / (2.f * UpperBoneSize * DirectSize)));
	float Angle2 = FMath::RadiansToDegrees(FMath::Acos((a + b - DirectSizeSquared) / (2.f * UpperBoneSize * LowerBoneSize)));

	// 2) Find rotation plane
	FTransform ChainPlane = FTransform(ChainStart);
	// upperarm-hand as FORWARD direction
	FVector FrontVec = (ChainEnd - ChainStart).GetSafeNormal();
	// RIGHT direction
	FVector JointVec = (JointTarget - ChainStart).GetSafeNormal();
	// (temp: UP direction)
	FVector PlaneNormal = JointVec ^ FrontVec; PlaneNormal.Normalize();
	FVector RightVec = FrontVec ^ PlaneNormal; RightVec.Normalize();

	ChainPlane.SetRotation(UKismetMathLibrary::MakeRotFromXY(FrontVec, RightVec * SideMul).Quaternion());

	// direction to joint target at the rotation plane
	float RotationDirection = SideMul;

	// 3) upper bone
	FRotator RotUpperbone = FRotator::ZeroRotator;
	RotUpperbone.Yaw = Angle1 * RotationDirection;
	UpperBone = FTransform(RotUpperbone, FVector::ZeroVector) * ChainPlane;
	UpperBone.SetTranslation(ChainStart);

	// 4) lower bone
	ChainPlane = FTransform(UpperBone.GetRotation(), UpperBone.GetTranslation() + UpperBone.GetRotation().GetForwardVector() * UpperBoneSize);
	RotUpperbone.Yaw = (Angle2 - 180.f) * RotationDirection;
	LowerBone = FTransform(RotUpperbone, FVector::ZeroVector) * ChainPlane;
	LowerBone.SetTranslation(ChainStart + UpperBone.GetRotation().GetForwardVector() * UpperBoneSize);

	// Snap to end?
	if (bSnapToChainEnd)
	{
		const float HandLength = UpperBoneSize + LowerBoneSize;

		if (DirectSize > HandLength)
		{
			const FVector ArmDirection = ChainEnd - ChainStart;
			const FVector Adjustment = ArmDirection - ArmDirection.GetSafeNormal() * HandLength;

			UpperBone.AddToTranslation(Adjustment);
			LowerBone.AddToTranslation(Adjustment);
		}
	}

	return LowerBone.GetTranslation() + LowerBone.GetRotation().GetForwardVector() * LowerBoneSize;
}