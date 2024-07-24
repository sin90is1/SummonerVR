// Copyright Yuri N K, 2022. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "VRIKFingersTypes.h"
#include "YnnkVRIKTypes.h"
#include "HeadMountedDisplayTypes.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"

#define __pack_euler2byte(value) static_cast<uint8>(FMath::TruncToInt(255.f * (value + 180.f) / 360.f))
#define __unpack_byte2euler(value) static_cast<float>(360.f * (float)value / 255.f - 180.f)

FVRIK_OrientTransform::FVRIK_OrientTransform()
	: ForwardAxis(EVRIK_BoneOrientationAxis::X)
	, RightAxis(EVRIK_BoneOrientationAxis::Y)
	, UpAxis(EVRIK_BoneOrientationAxis::Z)
	, RightAxisMultiplier(1.f)
{
	FTransform();
}

FVRIK_OrientTransform::FVRIK_OrientTransform(const FVRIK_OrientTransform& OrientationBase, const FTransform& InTransform,
	float InRightAxisMultiplier, EVRIK_BoneOrientationAxis InForwardAxis, EVRIK_BoneOrientationAxis InUpAxis)
{
	*this = InTransform;

	ForwardAxis = OrientationBase.ForwardAxis; RightAxis = OrientationBase.RightAxis; UpAxis = OrientationBase.UpAxis;
	RightAxisMultiplier = InRightAxisMultiplier;

	const FVector V = GetAxisVector(InTransform, InForwardAxis);
	const FVector U = GetAxisVector(InTransform, InUpAxis);
	const FRotator ConvertedRot = MakeRotFromTwoAxis(ForwardAxis, V, UpAxis, U);
	SetRotation(ConvertedRot.Quaternion());
}

FVector FVRIK_OrientTransform::GetAxisVector(const FTransform& InTransform, EVRIK_BoneOrientationAxis Axis)
{
	float DirMul = 1.f;
	uint8 convAxis = (uint8)Axis;

	if (Axis >= EVRIK_BoneOrientationAxis::X_Neg)
	{
		DirMul = -1.f;
		convAxis -= (uint8)EVRIK_BoneOrientationAxis::X_Neg;
	}
	return DirMul * InTransform.GetScaledAxis((EAxis::Type)(convAxis + 1));
}

FVector FVRIK_OrientTransform::GetAxisVector(const FRotator& InRotator, EVRIK_BoneOrientationAxis Axis)
{
	switch (Axis)
	{
		case EVRIK_BoneOrientationAxis::X:
			return InRotator.Quaternion().GetForwardVector();
		case EVRIK_BoneOrientationAxis::Y:
			return InRotator.Quaternion().GetRightVector();
		case EVRIK_BoneOrientationAxis::Z:
			return InRotator.Quaternion().GetUpVector();
		case EVRIK_BoneOrientationAxis::X_Neg:
			return InRotator.Quaternion().GetForwardVector() * -1.f;
		case EVRIK_BoneOrientationAxis::Y_Neg:
			return InRotator.Quaternion().GetRightVector() * -1.f;
		case EVRIK_BoneOrientationAxis::Z_Neg:
			return InRotator.Quaternion().GetUpVector() * -1.f;
	}
	return FVector();
}

void FVRIK_OrientTransform::SetTransform(const FTransform& InTransform)
{
	CopyTranslation(InTransform);
	CopyRotation(InTransform);
	CopyScale3D(InTransform);
}

FVRIK_OrientTransform FVRIK_OrientTransform::MakeOrientTransform(const FVRIK_OrientTransform& OrientationBase, const FTransform& InTransform)
{
	FVRIK_OrientTransform ret;
	ret.CopyTranslation(InTransform);
	ret.CopyRotation(InTransform);
	ret.CopyScale3D(InTransform);
	ret.ForwardAxis = OrientationBase.ForwardAxis; ret.RightAxis = OrientationBase.RightAxis; ret.UpAxis = OrientationBase.UpAxis;
	ret.RightAxisMultiplier = OrientationBase.RightAxisMultiplier;

	return ret;
}

FRotator FVRIK_OrientTransform::ConvertRotator(const FVRIK_OrientTransform& SourceRot, const FVRIK_OrientTransform& TargetRot)
{
	return MakeRotFromTwoAxis(TargetRot.ForwardAxis, SourceRot.GetConvertedForwardVector(), TargetRot.RightAxis, SourceRot.GetConvertedRightVector());
}

FRotator FVRIK_OrientTransform::MakeRotFromTwoAxis(EVRIK_BoneOrientationAxis Axis1, const FVector& Vector1, EVRIK_BoneOrientationAxis Axis2, const FVector& Vector2)
{
	FRotator r;
	FVector V1, V2;

	if (Axis1 == EVRIK_BoneOrientationAxis::X_Neg) {
		Axis1 = EVRIK_BoneOrientationAxis::X; V1 = -Vector1;
	}
	else if (Axis1 == EVRIK_BoneOrientationAxis::Y_Neg) {
		Axis1 = EVRIK_BoneOrientationAxis::Y; V1 = -Vector1;
	}
	else if (Axis1 == EVRIK_BoneOrientationAxis::Z_Neg) {
		Axis1 = EVRIK_BoneOrientationAxis::Z; V1 = -Vector1;
	}
	else V1 = Vector1;

	if (Axis2 == EVRIK_BoneOrientationAxis::X_Neg) {
		Axis2 = EVRIK_BoneOrientationAxis::X; V2 = -Vector2;
	}
	else if (Axis2 == EVRIK_BoneOrientationAxis::Y_Neg) {
		Axis2 = EVRIK_BoneOrientationAxis::Y; V2 = -Vector2;
	}
	else if (Axis2 == EVRIK_BoneOrientationAxis::Z_Neg) {
		Axis2 = EVRIK_BoneOrientationAxis::Z; V2 = -Vector2;
	}
	else V2 = Vector2;

	if (Axis1 == EVRIK_BoneOrientationAxis::X) {
		if (Axis2 == EVRIK_BoneOrientationAxis::Y)
			r = FRotationMatrix::MakeFromXY(V1, V2).Rotator();
		else if (Axis2 == EVRIK_BoneOrientationAxis::Z)
			r = FRotationMatrix::MakeFromXZ(V1, V2).Rotator();
	}
	else if (Axis1 == EVRIK_BoneOrientationAxis::Y) {
		if (Axis2 == EVRIK_BoneOrientationAxis::X)
			r = FRotationMatrix::MakeFromYX(V1, V2).Rotator();
		else if (Axis2 == EVRIK_BoneOrientationAxis::Z)
			r = FRotationMatrix::MakeFromYZ(V1, V2).Rotator();
	}
	else if (Axis1 == EVRIK_BoneOrientationAxis::Z) {
		if (Axis2 == EVRIK_BoneOrientationAxis::X)
			r = FRotationMatrix::MakeFromZX(V1, V2).Rotator();
		else if (Axis2 == EVRIK_BoneOrientationAxis::Y)
			r = FRotationMatrix::MakeFromZY(V1, V2).Rotator();
	}

	return r;
}

/** Helper. Find axis in transform rotation co-directed with vector */
EVRIK_BoneOrientationAxis FVRIK_OrientTransform::FindCoDirection(const FTransform& BoneRotator, const FVector Direction)
{
	EVRIK_BoneOrientationAxis RetAxis;
	const FVector dir = Direction.GetSafeNormal();
	const FVector v1 = FVRIK_OrientTransform::GetAxisVector(BoneRotator, EVRIK_BoneOrientationAxis::X);
	const FVector v2 = FVRIK_OrientTransform::GetAxisVector(BoneRotator, EVRIK_BoneOrientationAxis::Y);
	const FVector v3 = FVRIK_OrientTransform::GetAxisVector(BoneRotator, EVRIK_BoneOrientationAxis::Z);
	const float dp1 = FVector::DotProduct(v1, dir);
	const float dp2 = FVector::DotProduct(v2, dir);
	const float dp3 = FVector::DotProduct(v3, dir);

	if (FMath::Abs(dp1) > FMath::Abs(dp2) && FMath::Abs(dp1) > FMath::Abs(dp3))
	{
		RetAxis = dp1 > 0.f ? EVRIK_BoneOrientationAxis::X : EVRIK_BoneOrientationAxis::X_Neg;
	}
	else if (FMath::Abs(dp2) > FMath::Abs(dp1) && FMath::Abs(dp2) > FMath::Abs(dp3))
	{
		RetAxis = dp2 > 0.f ? EVRIK_BoneOrientationAxis::Y : EVRIK_BoneOrientationAxis::Y_Neg;
	}
	else
	{
		RetAxis = dp3 > 0.f ? EVRIK_BoneOrientationAxis::Z : EVRIK_BoneOrientationAxis::Z_Neg;
	}

	return RetAxis;
}

FVRIK_OrientTransform FVRIK_OrientTransform::operator*(const FTransform& Other) const
{
	FVRIK_OrientTransform ret;
	ret = (FTransform)*this * Other;
	ret.ForwardAxis = ForwardAxis; ret.RightAxis = RightAxis; ret.UpAxis = UpAxis; ret.RightAxisMultiplier = RightAxisMultiplier;
	return ret;
}

FVRIK_OrientTransform& FVRIK_OrientTransform::operator=(const FTransform& Other)
{
	CopyTranslation(Other);
	CopyRotation(Other);
	CopyScale3D(Other);
	return *this;
}

FVRIK_OrientTransform& FVRIK_OrientTransform::operator=(const FVRIK_OrientTransform& Other)
{
	CopyTranslation(Other);
	CopyRotation(Other);
	CopyScale3D(Other);
	ForwardAxis = Other.ForwardAxis; RightAxis = Other.RightAxis; UpAxis = Other.UpAxis; RightAxisMultiplier = Other.RightAxisMultiplier;
	return *this;
}

bool HandHelpers::GetHandFingersRotation(UObject* WorldContext, bool bRightHand, FVRIK_FingersRawPreset& OutFingersRotation)
{
	// Get OpenXR fingers data
	FXRMotionControllerData MotionControllerData;
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->GetMotionControllerData(WorldContext, bRightHand ? EControllerHand::Right : EControllerHand::Left, MotionControllerData);
	}

	if (!MotionControllerData.bValid || MotionControllerData.DeviceName.IsNone() || MotionControllerData.HandKeyRotations.Num() == 0)
	{
		return false;
	}

	TArray<EHandKeypoint> KeypointTips = { EHandKeypoint::IndexDistal, EHandKeypoint::MiddleDistal, EHandKeypoint::RingDistal, EHandKeypoint::LittleDistal, EHandKeypoint::ThumbDistal };
	TArray< FVRIK_FingerRawRotation* > TargetRotationArrays = { &OutFingersRotation.Index, &OutFingersRotation.Middle, &OutFingersRotation.Ring, &OutFingersRotation.Pinky, &OutFingersRotation.Thumb };

	for (int32 FingerIndex = 0; FingerIndex < 5; ++FingerIndex)
	{
		int32 FingerTipKeypoint = static_cast<int32>(KeypointTips[FingerIndex]);
		FVRIK_FingerRawRotation& TargetFingerArray = *TargetRotationArrays[FingerIndex];

		// Ensure the array of rotators has valid size (3 bones for each finger)
		TargetFingerArray.Curls.SetNumUninitialized(3);

		for (int32 BoneIndex = 0; BoneIndex < 3; ++BoneIndex)
		{
			bool bFirstFingerBone = (BoneIndex == 2);
			int32 BoneKeypoint = FingerTipKeypoint - BoneIndex;
			int32 PrevBoneKeypoint = bFirstFingerBone ? static_cast<int32>(EHandKeypoint::Palm) : BoneKeypoint - 1;
			int32 OutArrInex = 2 - BoneIndex;

			// We need a rotation relative to previous bone
			FQuat BoneRot = MotionControllerData.HandKeyRotations[BoneKeypoint];
			FQuat ParentBoneRot = MotionControllerData.HandKeyRotations[PrevBoneKeypoint];

			if (bFirstFingerBone)
			{
				FTransform ParentTr = FTransform(ParentBoneRot, MotionControllerData.HandKeyPositions[PrevBoneKeypoint]);
				FTransform CurrTr = FTransform(BoneRot, MotionControllerData.HandKeyPositions[BoneKeypoint]);

				FRotator r = CurrTr.GetRelativeTransform(ParentTr).Rotator();
				TargetFingerArray.FingerRoll = r.Roll; TargetFingerArray.FingerYaw = r.Yaw;
				TargetFingerArray.Curls[0] = -r.Pitch;
			}
			else
			{
				float R, P, Y;
				YnnkHelpers::GetRotationDeltaInEulerCoordinates(BoneRot, ParentBoneRot, R, P, Y);
				TargetFingerArray.Curls[2 - BoneIndex] = -P;
			}
		}
	}

	return true;
}

void HandHelpers::PackFingerRotationToArray(const FVRIK_FingerRawRotation& InFingerRotation, TArray<uint8>& CompressedRotation, int32& Index)
{
	float MeanCurl = 0.f;
	for (const auto& Curl : InFingerRotation.Curls)
	{
		MeanCurl += Curl;
	}
	MeanCurl /= (float)InFingerRotation.Curls.Num();

	CompressedRotation[Index++] = __pack_euler2byte(FRotator::NormalizeAxis(InFingerRotation.FingerRoll));
	CompressedRotation[Index++] = __pack_euler2byte(FRotator::NormalizeAxis(InFingerRotation.FingerYaw));
	CompressedRotation[Index++] = __pack_euler2byte(FRotator::NormalizeAxis(MeanCurl));
}

bool HandHelpers::UnpackFingerRotationFromArray(FVRIK_FingerRawRotation& OutFingerRotation, const TArray<uint8>& FingersArray, int32& Index)
{
	if (FingersArray.Num() < Index + 3)
	{
		return false;
	}

	OutFingerRotation.FingerRoll = __unpack_byte2euler(FingersArray[Index++]);
	OutFingerRotation.FingerYaw = __unpack_byte2euler(FingersArray[Index++]);
	float Curl = __unpack_byte2euler(FingersArray[Index++]);
	OutFingerRotation.Curls.Init(Curl, 3);

	return true;
}

void HandHelpers::FingerInterpTo(FVRIK_FingerRawRotation& InOutFinger, const FVRIK_FingerRawRotation& Target, float DeltaTime, float Speed)
{
	InOutFinger.FingerRoll = FMath::FInterpTo(InOutFinger.FingerRoll, Target.FingerRoll, DeltaTime, Speed);
	InOutFinger.FingerYaw = FMath::FInterpTo(InOutFinger.FingerYaw, Target.FingerYaw, DeltaTime, Speed);

	if (InOutFinger.Curls.Num() != Target.Curls.Num())
	{
		InOutFinger.Curls.SetNum(Target.Curls.Num());
	}

	for (int32 Index = 0; Index < InOutFinger.Curls.Num(); ++Index)
	{
		InOutFinger.Curls[Index] = FMath::FInterpTo(InOutFinger.Curls[Index], Target.Curls[Index], DeltaTime, Speed);
	}
}