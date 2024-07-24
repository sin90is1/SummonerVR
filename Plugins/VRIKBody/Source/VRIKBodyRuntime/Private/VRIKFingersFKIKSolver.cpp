// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "VRIKFingersFKIKSolver.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "VRIKFingersSolverSetup.h"
#include "DrawDebugHelpers.h"
#include "YnnkVRIKTypes.h"
#include "Curves/CurveFloat.h"
#include "VRIKFingersTypes.h"

#define _refpose_input 0
#define _refpose_trace 1
#define _refpose_grip  2

UVRIKFingersFKIKSolver::UVRIKFingersFKIKSolver()
	: Hand(EVRIK_VRHand::VRH_Right)
	, InputMinRotation(0.f)
	, InputMaxRotation(75.f)
	, PosesInterpolationSpeed(12.f)
	, HandSideMultiplier(1.f)
{
}

UVRIKFingersFKIKSolver* UVRIKFingersFKIKSolver::CreateFingersFKIKSolver(UVRIKFingersSolverSetup* SolverSetup, USkeletalMeshComponent* SkeletalMeshComp)
{
	UVRIKFingersFKIKSolver* NewSolver = NewObject<UVRIKFingersFKIKSolver>(UVRIKFingersFKIKSolver::StaticClass());
	NewSolver->Initialize(SolverSetup, SkeletalMeshComp);
	return NewSolver;
}

bool UVRIKFingersFKIKSolver::Initialize(UVRIKFingersSolverSetup* SolverSetup, USkeletalMeshComponent* SkeletalMeshComp)
{
	bIsInitialized = false;
	if (!SkeletalMeshComp || !SolverSetup)
	{
		return false;
	}

	Fingers = SolverSetup->Fingers;
	TraceChannel = SolverSetup->TraceChannel;
	TraceHalfDistance = SolverSetup->TraceHalfDistance;
	Hand = SolverSetup->Hand;
	Mesh = SkeletalMeshComp;
	FingersSolverSetup = SolverSetup;
	bTraceByObjectType = SolverSetup->bTraceByObjectType;
	InputMinRotation = SolverSetup->InputMinRotation;
	InputMaxRotation = SolverSetup->InputMaxRotation;
	PosesInterpolationSpeed = SolverSetup->PosesInterpolationSpeed;
	BlendCurve = SolverSetup->BlendCurve;
	HandSideMultiplier = (Hand == EVRIK_VRHand::VRH_Right) ? 1.f : -1.f;

	const TArray<FTransform>& ComponentSpaceTMs = Mesh->GetComponentSpaceTransforms();
	const FReferenceSkeleton& RefSkeleton = AccessRefSkeleton(AccessSkeletalMesh(Mesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	for (auto& Finger : Fingers)
	{
		Finger.Value.bEnabled = true;
		Finger.Value.Knuckles.Empty();

		FName CurrKnuckle = Finger.Value.TipBoneName;
		FTransform PreviousBoneTransform;
		for (int32 i = 0; i < Finger.Value.KnucklesNum; i++)
		{
			const int32 CurrentBoneIndex = RefSkeleton.FindBoneIndex(CurrKnuckle);
			if (CurrentBoneIndex == INDEX_NONE)
			{
				return false;
			}
			FVRIK_Knuckle NewKnuckle;
			NewKnuckle.BoneIndex = CurrentBoneIndex;
			NewKnuckle.BoneName = CurrKnuckle;
			NewKnuckle.Radius = FMath::Lerp(Finger.Value.TipRadius, Finger.Value.RootRadius, (float)i / (float)(Finger.Value.KnucklesNum - 1));

			FTransform BoneTransform = RefPoseSpaceBaseTMs[CurrentBoneIndex];
			NewKnuckle.RefPoseRelativeTransform
				= NewKnuckle.InputRefPoseRelativeTransform
				= NewKnuckle.TraceRefPoseRelativeTransform
				= NewKnuckle.RelativeTransform
				= BoneTransform;
			NewKnuckle.WorldTransform = Mesh->GetSocketTransform(CurrKnuckle);

			if (NewKnuckle.WorldTransform.Equals(Mesh->GetComponentTransform()))
			{
				return false;
			}

			if (i > 0)
			{
				NewKnuckle.Length = PreviousBoneTransform.GetTranslation().Size();
			}

			Finger.Value.Knuckles.Insert(NewKnuckle, 0);

			PreviousBoneTransform = BoneTransform;
			const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(CurrentBoneIndex);
			if (ParentBoneIndex == INDEX_NONE)
			{
				return false;
			}
			CurrKnuckle = RefSkeleton.GetBoneName(ParentBoneIndex);
		}

		if (Finger.Value.Knuckles.Num() == 0 || Finger.Value.Knuckles.Num() != Finger.Value.KnucklesNum)
		{
			return false;
		}

		// root bone
		const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(Finger.Value.Knuckles[0].BoneIndex);
		if (ParentBoneIndex == INDEX_NONE)
		{
			return false;
		}
		Finger.Value.RootBoneName = RefSkeleton.GetBoneName(ParentBoneIndex);

		// find right axis

		const FTransform Knuckle0Tr = Mesh->GetSocketTransform(Finger.Value.Knuckles[0].BoneName);
		Finger.Value.FingerOrientation.UpAxis = Finger.Value.OutwardAxis;

		const FVector ForwardKnuckle0 = (Finger.Value.KnucklesNum > 1)
			? Mesh->GetSocketLocation(Finger.Value.Knuckles[1].BoneName) - Knuckle0Tr.GetTranslation()
			: Knuckle0Tr.GetTranslation() - Mesh->GetSocketLocation(Finger.Value.RootBoneName);
		Finger.Value.FingerOrientation.ForwardAxis = FVRIK_OrientTransform::FindCoDirection(Knuckle0Tr, ForwardKnuckle0);

		const FVector FingerForwardVector = FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), Finger.Value.FingerOrientation.ForwardAxis);
		const FVector FingerUpVector = FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), Finger.Value.FingerOrientation.UpAxis);
		const FVector FingerRightVector = (FingerUpVector ^ FingerForwardVector).GetSafeNormal();

		EVRIK_BoneOrientationAxis FingerRightAxis;
		if (Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::X && Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::X_Neg &&
			Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::X && Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::X_Neg) {
			FingerRightAxis =
				(FVector::DotProduct(FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), EVRIK_BoneOrientationAxis::X), FingerRightVector) > 0.f) ? EVRIK_BoneOrientationAxis::X : EVRIK_BoneOrientationAxis::X_Neg;
		}
		else if (Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::Y && Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::Y_Neg &&
			Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::Y && Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::Y_Neg) {
			FingerRightAxis =
				(FVector::DotProduct(FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), EVRIK_BoneOrientationAxis::Y), FingerRightVector) > 0.f) ? EVRIK_BoneOrientationAxis::Y : EVRIK_BoneOrientationAxis::Y_Neg;
		}
		else /*if (Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::Z && Finger.Value.FingerOrientation.ForwardAxis != EVRIK_BoneOrientationAxis::Z_Neg &&
			Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::Z && Finger.Value.FingerOrientation.UpAxis != EVRIK_BoneOrientationAxis::Z_Neg)*/ {
			FingerRightAxis =
				(FVector::DotProduct(FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), EVRIK_BoneOrientationAxis::Z), FingerRightVector) > 0.f) ? EVRIK_BoneOrientationAxis::Z : EVRIK_BoneOrientationAxis::Z_Neg;
		}
			
		Finger.Value.FingerOrientation.RightAxis = FingerRightAxis;

		// tip length
		if (Finger.Value.Knuckles.Num() > 2)
		{
			const int32 LastInd = Finger.Value.Knuckles.Num() - 1;
			float Length2 = Finger.Value.Knuckles[LastInd - 1].Length;
			float Length1 = Finger.Value.Knuckles[LastInd - 2].Length;

			Finger.Value.Knuckles.Last().Length = FMath::Clamp(Length2 + (Length2 - Length1), 0.f, FMath::Max(Length1, Length2));
		}
		else if (Finger.Value.Knuckles.Num() > 1)
		{
			Finger.Value.Knuckles.Last().Length = Finger.Value.Knuckles[0].Length;
		}
		else
		{
			Finger.Value.Knuckles.Last().Length = 1.f;
		}
	}

	// find hand bone and index and orientation
	int32 Index1 = RefSkeleton.FindBoneIndex(Fingers[EVRIKFingerName::FN_Middle].RootBoneName);
	int32 Index2 = RefSkeleton.FindBoneIndex(Fingers[EVRIKFingerName::FN_Index].RootBoneName);
	while (Index1 != Index2 && Index1 != INDEX_NONE && Index2 != INDEX_NONE)
	{
		Index1 = RefSkeleton.GetParentIndex(Index1);
		Index2 = RefSkeleton.GetParentIndex(Index2);
	}
	WristBoneIndex = Index1;
	if (WristBoneIndex != INDEX_NONE)
	{
		WristBoneName = RefSkeleton.GetBoneName(WristBoneIndex);
		if (Fingers[EVRIKFingerName::FN_Middle].KnucklesNum > 0)
		{
			FTransform BoneWristTr = Mesh->GetSocketTransform(WristBoneName);

			const auto& FingerTest = Fingers[EVRIKFingerName::FN_Middle];
			FVector WristForward = Mesh->GetSocketLocation(FingerTest.Knuckles[0].BoneName) - BoneWristTr.GetTranslation();
			FVector WristRight = Mesh->GetSocketLocation(Fingers[EVRIKFingerName::FN_Pinky].Knuckles[0].BoneName) - Mesh->GetSocketLocation(Fingers[EVRIKFingerName::FN_Index].Knuckles[0].BoneName);
			if (Hand == EVRIK_VRHand::VRH_Left) WristRight *= -1.f;

			EVRIK_BoneOrientationAxis WristForwAxis = FVRIK_OrientTransform::FindCoDirection(BoneWristTr, WristForward);
			EVRIK_BoneOrientationAxis WristRightAxis = FVRIK_OrientTransform::FindCoDirection(BoneWristTr, WristRight);

			FRotator WristGen = UKismetMathLibrary::MakeRotFromXY(
				FVRIK_OrientTransform::GetAxisVector(BoneWristTr.Rotator(), WristForwAxis),
				FVRIK_OrientTransform::GetAxisVector(BoneWristTr.Rotator(), WristRightAxis)
			);

			FTransform GenericWristTr = FTransform(WristGen, BoneWristTr.GetTranslation());
			WristFromGenericConverter = BoneWristTr.GetRelativeTransform(GenericWristTr);
			WristFromGenericConverter.SetTranslation(FVector::ZeroVector);
		}
	}
	for (auto& Finger : Fingers)
	{
		Finger.Value.RootBoneToWristOffset = FTransform::Identity;
		if (Finger.Value.RootBoneName != WristBoneName)
		{
			int32 BoneIndex = RefSkeleton.FindBoneIndex(Finger.Value.RootBoneName);
			if (BoneIndex != INDEX_NONE)
			{
				Finger.Value.RootBoneToWristOffset = RefPoseSpaceBaseTMs[BoneIndex];
			}
		}
	}

	// init tracing map
	int32 FingersMax = (int32)EVRIKFingerName::EVRIKFingerName_MAX;
	for (int32 i = 0; i < FingersMax; i++)
	{
		EVRIKFingerName FingerKey = static_cast<EVRIKFingerName>(i);
		TracingStatus.Add(FingerKey, false);
		VRStatus.Add(FingerKey, false);
		CachedHandPose.Add(FingerKey);

		CachedHandPose[FingerKey].SetNumUninitialized(Fingers[FingerKey].KnucklesNum);
		for (int32 BoneIndex = 0; BoneIndex < Fingers[FingerKey].KnucklesNum; ++BoneIndex)
		{
			CachedHandPose[FingerKey][BoneIndex] = Fingers[FingerKey].Knuckles[BoneIndex].RefPoseRelativeTransform.GetRotation();
		}
	}

	bIsInitialized = true;
	return true;
}

FVector UVRIKFingersFKIKSolver::TraceKnuckle(UWorld* World, const FVRIK_FingerSolver& Finger, const FVRIK_Knuckle& Knuckle, const FTransform& KnuckleTr, FHitResult& HitResult, int32& Pass, int32 KnuckleIndex)
{
	const FTransform HandPalm_WS = Mesh->GetSocketTransform(Finger.RootBoneName);
	FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
	Params.AddIgnoredComponent(Mesh);

	FTransform TraceCenter = KnuckleTr;
	TraceCenter.AddToTranslation(FVRIK_OrientTransform::GetAxisVector(TraceCenter.Rotator(), Finger.FingerOrientation.ForwardAxis) * Knuckle.Length);

	FTransform TraceRotationSource;
	if (KnuckleIndex == 0)
		TraceRotationSource = KnuckleTr;
	else
		TraceRotationSource = Finger.Knuckles[KnuckleIndex].TraceRefPoseRelativeTransform * Finger.Knuckles[KnuckleIndex - 1].WorldTransform;

	const FVector UpDirection = FVRIK_OrientTransform::GetAxisVector(TraceRotationSource.Rotator(), Finger.OutwardAxis);
	const FVector ForwardDirection = FVRIK_OrientTransform::GetAxisVector(TraceRotationSource.Rotator(), Finger.FingerOrientation.ForwardAxis);

	FRotator TraceDirectionSpace = UKismetMathLibrary::MakeRotFromXZ(ForwardDirection, UpDirection);
	float UpMultiplier = (KnuckleIndex == 0 ? FMath::Clamp(0.f, Knuckle.Length, TraceHalfDistance * 0.5f) : TraceHalfDistance);
	
	FHitResult MinResult;
	FVector TraceDir;
	FVector TraceVector;
	float LocalPitch = 0;
	while (LocalPitch < 90.f)
	{
		LocalPitch += 45.f;

		float SinA, CosA;
		FMath::SinCos(&SinA, &CosA, LocalPitch);

		TraceVector = TraceDirectionSpace.RotateVector(FVector(CosA, 0.f, SinA));
		if (bTraceByObjectType)
		{
			World->SweepSingleByObjectType(HitResult,
				TraceCenter.GetTranslation() + TraceVector * UpMultiplier,
				TraceCenter.GetTranslation() - TraceVector * TraceHalfDistance,
				TraceDirectionSpace.Quaternion(), FCollisionObjectQueryParams(TraceChannel), FCollisionShape::MakeSphere(Knuckle.Radius), Params);
		}
		else
		{
			World->SweepSingleByChannel(HitResult,
				TraceCenter.GetTranslation() + TraceVector * UpMultiplier,
				TraceCenter.GetTranslation() - TraceVector * TraceHalfDistance,
				TraceDirectionSpace.Quaternion(), TraceChannel, FCollisionShape::MakeSphere(Knuckle.Radius), Params);
		}
		if (IsValid(FilterObject) && HitResult.GetComponent() != FilterObject) HitResult.bBlockingHit = false;

		if (bDrawDebugGeometry)
		{
			DrawDebugLine(World, TraceCenter.GetTranslation(), TraceCenter.GetTranslation() - TraceVector * TraceHalfDistance,
				FColor((uint8)TraceDirectionSpace.Pitch, (uint8)TraceDirectionSpace.Pitch, (uint8)TraceDirectionSpace.Pitch)
				, false, 0.1f, 0, 0.1f);
		}

		if (HitResult.bBlockingHit)
		{
			if (!MinResult.bBlockingHit || MinResult.Distance > HitResult.Distance)
			{
				MinResult = HitResult;
				TraceDir = TraceVector;
			}
		}
	}
	if (MinResult.bBlockingHit)
		HitResult = MinResult;
	else if (HitResult.bBlockingHit)
		TraceDir = TraceVector;
	else
		TraceDir = UpDirection;

	if (HitResult.bBlockingHit && KnuckleIndex == 0 && FVector::DotProduct(-TraceDir, UpDirection) > 0.4f)
	{
		HitResult.bBlockingHit = false;
	}

	if (bDrawDebugGeometry)
	{
		DrawDebugLine(World, TraceCenter.GetTranslation(), TraceCenter.GetTranslation() + TraceDir * TraceHalfDistance, FColor::Blue, false, 0.1f, 0, 0.1f);
		if (HitResult.bBlockingHit)
		{
			DrawDebugLine(World, TraceCenter.GetTranslation(), HitResult.ImpactPoint, FColor::Red, false, 0.1f, 0, 0.1f);
		}
	}

	return TraceDir;
}

void UVRIKFingersFKIKSolver::Trace(bool bTracingInTick)
{
	if (!bIsInitialized)
	{
		return;
	}

	UWorld* World = Mesh->GetOwner()->GetWorld();
	if (!World)
	{
		// LOG: "invalid world"
		return;
	}

	const float DeltaTime = World->DeltaTimeSeconds;

	FHitResult hit;

	// trace all fingers
	for (auto& FingerRef : Fingers)
	{
		FVRIK_FingerSolver& Finger = FingerRef.Value;
		bool bGrabObject = false;

		// disabled finger?
		if (!Finger.bEnabled)
		{
			if (Finger.Alpha > 0.f)
			{
				if (bTracingInTick)
				{
					Finger.Alpha = FMath::FInterpConstantTo(Finger.Alpha, 0.f, DeltaTime, 4.f);
					if (Finger.Alpha < 0.01f) Finger.Alpha = 0.f;
				}
				else
					Finger.Alpha = 0.f;
			}
			continue;
		}

		if (Finger.Knuckles.Num() == 0) continue;

		const FTransform HandPalm_WS = Mesh->GetSocketTransform(Finger.RootBoneName);
		const FTransform FingerBase_WS = Finger.Knuckles[0].TraceRefPoseRelativeTransform * HandPalm_WS;

		// trace all knuckles
		// FK Solver
		int32 FKPass = 1;
		for (int32 Index = 0; Index < Finger.Knuckles.Num(); Index++)
		{
			auto& Knuckle = Finger.Knuckles[Index];
			FTransform KnuckleTr;
			if (Index == 0)
			{
				KnuckleTr = FKPass == 1 ? Knuckle.TraceRefPoseRelativeTransform * HandPalm_WS : Knuckle.WorldTransform;
			}
			else
			{
				KnuckleTr = Knuckle.TraceRefPoseRelativeTransform * Finger.Knuckles[Index - 1].WorldTransform;
			}
			FVector UpDirection = TraceKnuckle(World, Finger, Knuckle, KnuckleTr, hit, FKPass, Index);

			if (hit.bBlockingHit)
			{
				FVector KnuckleTargetPoint = hit.ImpactPoint + hit.ImpactNormal * Knuckle.Radius;
				const FVector TraceForward = (KnuckleTargetPoint - KnuckleTr.GetTranslation()).GetSafeNormal();
				FRotator WorldRotator = FVRIK_OrientTransform::MakeRotFromTwoAxis(Finger.FingerOrientation.ForwardAxis, TraceForward, Finger.FingerOrientation.RightAxis, FVRIK_OrientTransform::GetAxisVector(FingerBase_WS.Rotator(), Finger.FingerOrientation.RightAxis));

				FTransform NewWorldTransform = FTransform(WorldRotator, KnuckleTr.GetTranslation());

				// Avoid flickering
				FVector NewUpDirection = TraceKnuckle(World, Finger, Knuckle, NewWorldTransform, hit, FKPass, Index);
				if (hit.bBlockingHit)
				{
					Knuckle.WorldTransform = NewWorldTransform;
				}

				// Check if need IK Solver
				if (hit.bBlockingHit && Index > 0 && FKPass <= Finger.KnucklesNum)
				{
					UpDirection = FVRIK_OrientTransform::GetAxisVector(Finger.Knuckles[Index - 1].WorldTransform.Rotator(), Finger.FingerOrientation.UpAxis);
					const FVector ForwardDirection = FVRIK_OrientTransform::GetAxisVector(Knuckle.WorldTransform.Rotator(), Finger.FingerOrientation.ForwardAxis);

					// IK Solver
					if (FVector::DotProduct(UpDirection, ForwardDirection) > 0.1f)
					{
						if (bDrawDebugGeometry)
						{
							DrawDebugLine(World,
								Finger.Knuckles[Index - 1].WorldTransform.GetTranslation(),
								Finger.Knuckles[Index - 1].WorldTransform.GetTranslation() + FVRIK_OrientTransform::GetAxisVector(Finger.Knuckles[Index - 1].WorldTransform.Rotator(), Finger.FingerOrientation.ForwardAxis) * Finger.Knuckles[Index - 1].Length,
								FColor::Orange, false, 0.1f, 0, 0.2f
							);
							DrawDebugLine(World,
								Finger.Knuckles[Index].WorldTransform.GetTranslation(),
								Finger.Knuckles[Index].WorldTransform.GetTranslation() + FVRIK_OrientTransform::GetAxisVector(Finger.Knuckles[Index].WorldTransform.Rotator(), Finger.FingerOrientation.ForwardAxis) * Finger.Knuckles[Index].Length,
								FColor::Yellow, false, 0.1f, 0, 0.2f
							);
						}

						const FTransform Knuckle0Tr = Finger.Knuckles[0].WorldTransform;
						WorldRotator = FVRIK_OrientTransform::MakeRotFromTwoAxis(Finger.FingerOrientation.ForwardAxis, (KnuckleTargetPoint - Knuckle0Tr.GetTranslation()).GetSafeNormal(), Finger.FingerOrientation.UpAxis, FVRIK_OrientTransform::GetAxisVector(Knuckle0Tr.Rotator(), Finger.FingerOrientation.UpAxis));
						Finger.Knuckles[Index].WorldTransform.SetRotation(WorldRotator.Quaternion());

						if (bDrawDebugGeometry)
						{
							DrawDebugSphere(World, Finger.Knuckles[Index].WorldTransform.GetTranslation(), 0.5f, 4, FColor::Blue, false, 0.1f, 0, 0.2f);
						}

						FVector Loc = Finger.Knuckles[0].WorldTransform.GetTranslation();
						for (int32 IndexIK = 0; IndexIK < Index; IndexIK++)
						{
							Finger.Knuckles[IndexIK].WorldTransform = Knuckle.WorldTransform;
							Finger.Knuckles[IndexIK].WorldTransform.SetTranslation(Loc);
							Loc += ForwardDirection * Finger.Knuckles[IndexIK].Length;
						}
						Finger.Knuckles[Index].WorldTransform.SetTranslation(Loc);
						Index = 1; FKPass++;
					}
				}

				bGrabObject = true;
			}
			else
			{
				if (VRStatus[FingerRef.Key] && Index == 0) break;
				Knuckle.WorldTransform = KnuckleTr;
			}
		}

		// if grabbing - update relative transforms
		if (bGrabObject)
		{
			FTransform TargetTr;

			for (int32 Index = 0; Index < Finger.Knuckles.Num(); Index++)
			{
				if (Index == 0)
				{
					TargetTr = Finger.Knuckles[0].WorldTransform.GetRelativeTransform(HandPalm_WS);
				}
				else
				{
					TargetTr = Finger.Knuckles[Index].WorldTransform.GetRelativeTransform(Finger.Knuckles[Index - 1].WorldTransform);
				}

				if (bTracingInTick)
				{
					Finger.Knuckles[Index].RelativeTransform = UKismetMathLibrary::TInterpTo(Finger.Knuckles[Index].RelativeTransform, TargetTr, DeltaTime, 12.f);
				}
				else
				{
					Finger.Knuckles[Index].RelativeTransform = TargetTr;
				}
			}

			if (!VRStatus[FingerRef.Key])
			{
				if (Finger.Alpha < 1.f)
				{
					if (bTracingInTick)
					{
						Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 12.f);
						if (Finger.Alpha > 0.99f) Finger.Alpha = 1.f;
					}
					else Finger.Alpha = 1.f;
				}
			}
		}
		else
		{
			// not grabbing - relax finger
			if (!VRStatus[FingerRef.Key])
			{
				if (Finger.Alpha > 0.f)
				{
					if (bTracingInTick)
					{
						Finger.Alpha = FMath::FInterpConstantTo(Finger.Alpha, 0.f, DeltaTime, 8.f);
						if (Finger.Alpha < 0.01f) Finger.Alpha = 0.f;
					}
					else Finger.Alpha = 0.f;
				}
			}
		}

		TracingStatus[FingerRef.Key] = bGrabObject;
	}

	if (bDrawDebugGeometry)
	{
		for (auto& FingerRef : Fingers)
		{
			FVRIK_FingerSolver& Finger = FingerRef.Value;
			for (auto& Knuckle : Finger.Knuckles)
			{
				DrawDebugLine(World,
					Knuckle.WorldTransform.GetTranslation(),
					Knuckle.WorldTransform.GetTranslation() + FVRIK_OrientTransform::GetAxisVector(Knuckle.WorldTransform.Rotator(), Finger.FingerOrientation.ForwardAxis) * Knuckle.Length,
					FColor::Red, false, 0.1f, 0, 0.2f
				);
			}
		}
	}

	// adjust untraced fingers
	for (auto& FingerRef : Fingers)
	{
		if (true || TracingStatus[FingerRef.Key] || !FingerRef.Value.bEnabled || VRStatus[FingerRef.Key])
		{
			continue;
		}

		FVRIK_FingerSolver& Finger = FingerRef.Value;
		const FTransform HandPalm_WS = Mesh->GetSocketTransform(Finger.RootBoneName);

		EVRIKFingerName Src1 = EVRIKFingerName::EVRIKFingerName_MAX, Src2 = EVRIKFingerName::EVRIKFingerName_MAX;
		switch (FingerRef.Key)
		{
			case EVRIKFingerName::FN_Index:
				Src1 = EVRIKFingerName::FN_Middle;
				break;
			case EVRIKFingerName::FN_Middle:
				Src1 = EVRIKFingerName::FN_Index; Src2 = EVRIKFingerName::FN_Ring;
				break;
			case EVRIKFingerName::FN_Ring:
				Src1 = EVRIKFingerName::FN_Middle; Src2 = EVRIKFingerName::FN_Pinky;
				break;
			case EVRIKFingerName::FN_Pinky:
				Src1 = EVRIKFingerName::FN_Ring;
				break;
		}

		if ((Src1 != EVRIKFingerName::EVRIKFingerName_MAX && TracingStatus[Src1]) || (Src2 != EVRIKFingerName::EVRIKFingerName_MAX && TracingStatus[Src2]))
		{
			FTransform ReferenceTr = HandPalm_WS;
			// update world transforms
			for (int32 KnuckleIndex = 0; KnuckleIndex < Finger.KnucklesNum; KnuckleIndex++)
			{
				ReferenceTr = Finger.Knuckles[KnuckleIndex].TraceRefPoseRelativeTransform * ReferenceTr;

				const FVRIK_FingerSolver* FingerA = Fingers.Find(Src1);
				const FVRIK_FingerSolver* FingerB = Fingers.Find(Src2);
				
				const bool bFingerAHasTheSameAxes = !FingerA || (FingerA->FingerOrientation.ForwardAxis == Finger.FingerOrientation.ForwardAxis && FingerA->FingerOrientation.UpAxis == Finger.FingerOrientation.UpAxis);
				const bool bFingerBHasTheSameAxes = !FingerB || (FingerB->FingerOrientation.ForwardAxis == Finger.FingerOrientation.ForwardAxis && FingerB->FingerOrientation.UpAxis == Finger.FingerOrientation.UpAxis);

				FQuat NewRot, qA, qB;
				if (bFingerAHasTheSameAxes && bFingerBHasTheSameAxes)
				{
					// simple interpolation is possible
					qA = (FingerA && KnuckleIndex < FingerA->KnucklesNum) ? FingerA->Knuckles[KnuckleIndex].WorldTransform.GetRotation() : ReferenceTr.GetRotation();
					qB = (FingerB && KnuckleIndex < FingerB->KnucklesNum) ? FingerB->Knuckles[KnuckleIndex].WorldTransform.GetRotation() : ReferenceTr.GetRotation();
				}
				else
				{
					// convert A and B to current Finger's space
					if (FingerA && KnuckleIndex < FingerA->KnucklesNum)
					{
						const FRotator SrcRot = FingerA->Knuckles[KnuckleIndex].WorldTransform.Rotator();
						qA = FVRIK_OrientTransform::MakeRotFromTwoAxis(
							Finger.FingerOrientation.ForwardAxis, FVRIK_OrientTransform::GetAxisVector(SrcRot, FingerA->FingerOrientation.ForwardAxis),
							Finger.FingerOrientation.UpAxis, FVRIK_OrientTransform::GetAxisVector(SrcRot, FingerA->FingerOrientation.UpAxis)
						).Quaternion();
					}
					else qA = ReferenceTr.GetRotation();

					if (FingerB && KnuckleIndex < FingerB->KnucklesNum)
					{
						const FRotator SrcRot = FingerB->Knuckles[KnuckleIndex].WorldTransform.Rotator();
						qB = FVRIK_OrientTransform::MakeRotFromTwoAxis(
							Finger.FingerOrientation.ForwardAxis, FVRIK_OrientTransform::GetAxisVector(SrcRot, FingerB->FingerOrientation.ForwardAxis),
							Finger.FingerOrientation.UpAxis, FVRIK_OrientTransform::GetAxisVector(SrcRot, FingerB->FingerOrientation.UpAxis)
						).Quaternion();
					}
					else qB = ReferenceTr.GetRotation();
				}

				NewRot = FQuat::FastLerp(qA, qB, 0.5f);
				NewRot.Normalize();
				Finger.Knuckles[KnuckleIndex].WorldTransform.SetRotation(NewRot);

				if (KnuckleIndex > 0)
				{
					Finger.Knuckles[KnuckleIndex].WorldTransform.SetTranslation(
						Finger.Knuckles[KnuckleIndex - 1].WorldTransform.GetTranslation() +
						FVRIK_OrientTransform::GetAxisVector(Finger.Knuckles[KnuckleIndex - 1].WorldTransform.Rotator(), Finger.FingerOrientation.ForwardAxis) * Finger.Knuckles[KnuckleIndex - 1].Length
					);
				}
			}

			// update relative transforms
			FTransform TargetTr;
			for (int32 Index = 0; Index < Finger.Knuckles.Num(); Index++)
			{
				if (Index == 0)
					TargetTr = Finger.Knuckles[0].WorldTransform.GetRelativeTransform(HandPalm_WS);
				else
					TargetTr = Finger.Knuckles[Index].WorldTransform.GetRelativeTransform(Finger.Knuckles[Index - 1].WorldTransform);

				if (bTracingInTick)
					Finger.Knuckles[Index].RelativeTransform = UKismetMathLibrary::TInterpTo(Finger.Knuckles[Index].RelativeTransform, TargetTr, DeltaTime, 12.f);
				else
					Finger.Knuckles[Index].RelativeTransform = TargetTr;
			}

			// can't adjust - disable finger
			if (Finger.Alpha != 0.5f)
			{
				if (bTracingInTick)
				{
					Finger.Alpha = FMath::FInterpConstantTo(Finger.Alpha, 0.5f, DeltaTime, 4.f);
					if (FMath::Abs(Finger.Alpha - 0.5f) < 0.01f) Finger.Alpha = 0.5f;
				}
				else Finger.Alpha = 0.5f;
			}
		}
		else
		{
			// can't adjust - disable finger
			if (Finger.Alpha > 0.f)
			{
				if (bTracingInTick)
				{
					Finger.Alpha = FMath::FInterpConstantTo(Finger.Alpha, 0.f, DeltaTime, 4.f);
					if (Finger.Alpha < 0.01f) Finger.Alpha = 0.f;
				}
				else Finger.Alpha = 0.f;
			}
		}
	}
}

void UVRIKFingersFKIKSolver::UpdateFingers(bool bTrace, bool bUpdateFingersPose)
{
	if (!bIsInitialized && FingersSolverSetup && Mesh)
	{
		Initialize(FingersSolverSetup, Mesh);
	}

	if (!bIsInitialized)
	{
		return;
	}

	UWorld* World = Mesh->GetWorld();
	
	float CurrentTime = 0.f, DeltaTime = 0.f;
	if (IsValid(World))
	{
		CurrentTime = World->GetRealTimeSeconds();
		DeltaTime = World->DeltaTimeSeconds;
	}

	if (bTrace || (bUseRuntimeFingersPose && CurrentTime - TraceStartTime < 3.f))
	{
		Trace(true);
	}

	// Update interpolation alpha

	if (InterpolationAlpha < 1.f)
	{
		if (IsValid(BlendCurve))
		{
			float InTime = CurrentTime - InterpolationStartTime;
			InterpolationAlpha = BlendCurve->GetFloatValue(InTime);
			if (InterpolationAlpha > 1.f) InterpolationAlpha = 1.f;
		}
		else
		{
			InterpolationAlpha = FMath::FInterpTo(InterpolationAlpha, 1.f, DeltaTime, PosesInterpolationSpeed);
			if (InterpolationAlpha > 0.99f) InterpolationAlpha = 1.f;
		}
	}

	// Apply pose

	if (bHasVRInputInFrame)
	{
		ProcessVRInput();
	}
	else if (bHasDetailedVRInputInFrame)
	{
		if (bDetailedVRInputIsRaw)
		{
			ProcessFingersPoseRaw(VRInputRaw, false);
		}
		else
		{
			ProcessVRInputDetailed();
		}
	}
	else if (bUpdateFingersPose && bHasRawFingersPose)
	{
		ProcessFingersPoseRaw(CurrentFingersPoseRaw, true);
	}
	else if (bUpdateFingersPose && bHasDetailedFingersPose)
	{
		ProcessFingersPoseDetailed(true);
	}
	else if (bUpdateFingersPose && !bUseRuntimeFingersPose)
	{
		if (bHasFingersPose)
		{
			ProcessFingersPose(true);
		}
		else
		{
			// Don't have pose to apply
			// Disable fingers animation
			for (auto& FingerRef : Fingers)
			{
				FingerRef.Value.Alpha = FMath::Clamp(1.f - InterpolationAlpha, 0.f, 1.f);
			}
		}
	}

	bHasVRInputInFrame = false;
	bHasDetailedVRInputInFrame = false;
	bDetailedVRInputIsRaw = false;
}

bool UVRIKFingersFKIKSolver::SetFingersPose(const FVRIK_FingersPosePreset& NewPose)
{
	if (!bHasFingersPose || NewPose != CurrentFingersPose)
	{
		CacheFingersPose();
		CurrentFingersPose = NewPose;
		bHasFingersPose = true;
		bHasRawFingersPose = false;
		bHasDetailedFingersPose = false;
	}
	return true;
}

bool UVRIKFingersFKIKSolver::SetFingersPoseDetailed(const FVRIK_FingersDetailedInfo& NewPose)
{
	if (!bHasDetailedFingersPose || NewPose != CurrentFingersPoseDetailed)
	{
		CacheFingersPose();
		CurrentFingersPoseDetailed = NewPose;
		bHasFingersPose = false;
		bHasRawFingersPose = false;
		bHasDetailedFingersPose = true;
	}
	return true;
}

bool UVRIKFingersFKIKSolver::SetFingersPoseRaw(const FVRIK_FingersRawPreset& NewPose)
{
	if (!bHasRawFingersPose || NewPose != CurrentFingersPoseRaw)
	{
		CacheFingersPose();
		CurrentFingersPoseRaw = NewPose;
		bHasFingersPose = false;
		bHasRawFingersPose = true;
		bHasDetailedFingersPose = false;
	}
	return true;
}

void UVRIKFingersFKIKSolver::ResetFingersPose()
{
	CacheFingersPose();
	bHasFingersPose = false;
	bHasRawFingersPose = false;
	bHasDetailedFingersPose = false;
}

bool UVRIKFingersFKIKSolver::SetFingersTraceReferencePose(const FVRIK_FingersPosePreset& NewRefPose)
{
	return UpdateReferencePoseFromPoseName(NewRefPose, _refpose_trace);
}

bool UVRIKFingersFKIKSolver::SetVRInputReferencePose(const FVRIK_FingersPosePreset& NewRefPose)
{
	return UpdateReferencePoseFromPoseName(NewRefPose, _refpose_input);
}

bool UVRIKFingersFKIKSolver::SetVRInputGripPose(const FVRIK_FingersPosePreset& NewRefPose)
{
	bHasGripInputPose = UpdateReferencePoseFromPoseName(NewRefPose, _refpose_grip);
	return bHasGripInputPose;
}

void UVRIKFingersFKIKSolver::ResetFingersGripReferencePose()
{
	bHasGripInputPose = false;
}

void UVRIKFingersFKIKSolver::GrabObject(UPrimitiveComponent* Object)
{
	if (!Object)
	{
		return;
	}

	FilterObject = Object;
	bUseRuntimeFingersPose = true;

	if (UWorld* w = Mesh->GetWorld())
	{
		TraceStartTime = w->TimeSeconds;
	}
}

void UVRIKFingersFKIKSolver::ReleaseObject()
{
	FilterObject = nullptr;
	bUseRuntimeFingersPose = false;

	for (auto& ts : TracingStatus) ts.Value = false;
}

void UVRIKFingersFKIKSolver::GetOpenXRInputAsByteArray(TArray<uint8>& PosePacked) const
{
	const int32 ArrSize = 3 * 5; // 3 rot. coordinates for 5 fingers
	if (PosePacked.Num() != ArrSize)
	{
		PosePacked.SetNumUninitialized(ArrSize);
	}

	int32 Index = 0;
	HandHelpers::PackFingerRotationToArray(VRInputRaw.Thumb, PosePacked, Index);
	HandHelpers::PackFingerRotationToArray(VRInputRaw.Index, PosePacked, Index);
	HandHelpers::PackFingerRotationToArray(VRInputRaw.Middle, PosePacked, Index);
	HandHelpers::PackFingerRotationToArray(VRInputRaw.Ring, PosePacked, Index);
	HandHelpers::PackFingerRotationToArray(VRInputRaw.Pinky, PosePacked, Index);
}

void UVRIKFingersFKIKSolver::ApplyOpenXRInputFromByteArray(const TArray<uint8>& PosePacked)
{
	float DeltaTime = 0.f;
	UWorld* World = Mesh->GetWorld();
	if (IsValid(World)) DeltaTime = World->GetDeltaSeconds();

	FVRIK_FingersRawPreset NewVRInputRaw;
	FVRIK_FingersRawPreset CurrentVRInputRaw = VRInputRaw;

	const int32 ArrSizeFull = 3 * 5;	// 3 byte for 5 fingers
	if (PosePacked.Num() != ArrSizeFull)
	{
		return;
	}

	// unpack byte array to FFingersRawPreset
	int32 Index = 0;
	HandHelpers::UnpackFingerRotationFromArray(NewVRInputRaw.Thumb, PosePacked, Index);
	HandHelpers::UnpackFingerRotationFromArray(NewVRInputRaw.Index, PosePacked, Index);
	HandHelpers::UnpackFingerRotationFromArray(NewVRInputRaw.Middle, PosePacked, Index);
	HandHelpers::UnpackFingerRotationFromArray(NewVRInputRaw.Ring, PosePacked, Index);
	HandHelpers::UnpackFingerRotationFromArray(NewVRInputRaw.Pinky, PosePacked, Index);

	// interp smoothly pose
	HandHelpers::FingerInterpTo(CurrentVRInputRaw.Thumb, NewVRInputRaw.Thumb, DeltaTime, 8.f);
	HandHelpers::FingerInterpTo(CurrentVRInputRaw.Index, NewVRInputRaw.Index, DeltaTime, 8.f);
	HandHelpers::FingerInterpTo(CurrentVRInputRaw.Middle, NewVRInputRaw.Middle, DeltaTime, 8.f);
	HandHelpers::FingerInterpTo(CurrentVRInputRaw.Ring, NewVRInputRaw.Ring, DeltaTime, 8.f);
	HandHelpers::FingerInterpTo(CurrentVRInputRaw.Pinky, NewVRInputRaw.Pinky, DeltaTime, 8.f);

	// apply
	ApplyVRInputRaw(CurrentVRInputRaw);
}

bool UVRIKFingersFKIKSolver::UpdateReferencePoseFromPoseName(const FVRIK_FingersPosePreset& NewRefPose, uint8 PoseType)
{
	for (auto& FingerRef : Fingers)
	{
		FVRIK_FingerSolver& Finger = FingerRef.Value;
		int32 KnuckleIndex = 0;

		for (auto& Knuckle : Finger.Knuckles)
		{
			const FVRIK_OrientTransform& FingerOrient = Finger.FingerOrientation;

			const FVRIK_FingerRotation* fpp = nullptr;
			switch (FingerRef.Key)
			{
				case EVRIKFingerName::FN_Thumb: fpp = &NewRefPose.ThumbRotation; break;
				case EVRIKFingerName::FN_Index: fpp = &NewRefPose.IndexRotation; break;
				case EVRIKFingerName::FN_Middle: fpp = &NewRefPose.MiddleRotation; break;
				case EVRIKFingerName::FN_Ring: fpp = &NewRefPose.RingRotation; break;
				case EVRIKFingerName::FN_Pinky: fpp = &NewRefPose.PinkyRotation; break;
			}
			if (!fpp) continue;

			// For all knucles after first cut off negative curl
			const float CurlVal = (KnuckleIndex == 0) ? fpp->CurlValue + fpp->FirstKnuckleCurlAddend : FMath::Clamp(fpp->CurlValue, -0.1f, 2.f);

			FRotator AddRot = FRotator::ZeroRotator;
			SetRotationAxisValue(AddRot, FingerOrient.RightAxis, CurlVal * 90.f);
			if (KnuckleIndex == 0)
			{
				SetRotationAxisValue(AddRot, FingerOrient.UpAxis, fpp->SpreadValue * 20.f * HandSideMultiplier);
				SetRotationAxisValue(AddRot, FingerOrient.ForwardAxis, fpp->RollValue * 20.f * HandSideMultiplier);
			}

			const FRotator BaseRot = Knuckle.RefPoseRelativeTransform.Rotator();
			const FRotator NewRot = AddLocalRotation(AddRot, BaseRot);

			switch (PoseType)
			{
				case _refpose_input:
					Knuckle.TraceRefPoseRelativeTransform.SetRotation(NewRot.Quaternion());
					break;
				case _refpose_trace:
					Knuckle.TraceRefPoseRelativeTransform.SetRotation(NewRot.Quaternion());
					break;
				case _refpose_grip:
					Knuckle.InputGripPoseRelativeTransform.SetRotation(NewRot.Quaternion());
					break;
			}

			KnuckleIndex++;
		}
	}

	return true;
}

void UVRIKFingersFKIKSolver::ProcessFingersPoseDetailed(bool bUseInterpolation)
{
	float DeltaTime = 0.f;
	UWorld* World = Mesh->GetWorld();
	if (World) DeltaTime = World->DeltaTimeSeconds;

	bool bStateChecked = false;
	for (auto& FingerRef : Fingers)
	{
		FVRIK_FingerSolver& Finger = FingerRef.Value;

		if (Finger.Alpha < 1.f)
		{
			Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
		}

		int32 KnuckleIndex = 0;
		for (auto& Knuckle : Finger.Knuckles)
		{
			TArray<FVRIK_FingerRotation>* FingerRot = nullptr;
			switch (FingerRef.Key)
			{
				case EVRIKFingerName::FN_Thumb: FingerRot = &CurrentFingersPoseDetailed.ThumbBones; break;
				case EVRIKFingerName::FN_Index: FingerRot = &CurrentFingersPoseDetailed.IndexBones; break;
				case EVRIKFingerName::FN_Middle: FingerRot = &CurrentFingersPoseDetailed.MiddleBones; break;
				case EVRIKFingerName::FN_Ring: FingerRot = &CurrentFingersPoseDetailed.RingBones; break;
				case EVRIKFingerName::FN_Pinky: FingerRot = &CurrentFingersPoseDetailed.PinkyBones; break;
			}
			if (FingerRot->Num() <= KnuckleIndex) continue;

			const FVRIK_FingerRotation& fpp = (*FingerRot)[KnuckleIndex];

			// For all knucles after first cut off negative curl
			const float CurlVal = (KnuckleIndex == 0) ? fpp.CurlValue + fpp.FirstKnuckleCurlAddend : fpp.CurlValue;

			// Calculate target rotation in parent bone space
			FRotator AddRot = FRotator::ZeroRotator;
			SetRotationAxisValue(AddRot, Finger.FingerOrientation.RightAxis, CurlVal);
			if (KnuckleIndex == 0)
			{
				SetRotationAxisValue(AddRot, Finger.FingerOrientation.UpAxis, fpp.SpreadValue);
				SetRotationAxisValue(AddRot, Finger.FingerOrientation.ForwardAxis, fpp.RollValue);
			}

			const FRotator BaseRot = Knuckle.RefPoseRelativeTransform.Rotator();
			const FRotator NewRot = AddLocalRotation(AddRot, BaseRot);

			// Delta between current and target rotation
			const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(NewRot, Knuckle.RelativeTransform.Rotator());
			// Don't update
			if (!bStateChecked && Delta.IsZero()) continue;

			// Update relative transform
			if (InterpolationAlpha == 1.f || !bUseInterpolation || Delta.IsNearlyZero(0.01f))
			{
				// Is close? Set target value.
				Knuckle.RelativeTransform.SetRotation(NewRot.Quaternion());
			}
			else
			{
				// Not close? Interpolate.
				FQuat CurrRot = FQuat::Slerp(CachedHandPose[FingerRef.Key][KnuckleIndex], NewRot.Quaternion(), InterpolationAlpha);
				Knuckle.RelativeTransform.SetRotation(CurrRot);
			}

			// Index increment
			KnuckleIndex++;
		}
	}
}

void UVRIKFingersFKIKSolver::ProcessFingersPose(bool bUseInterpolation)
{
	float DeltaTime = 0.f;
	UWorld* World = Mesh->GetWorld();
	if (World) DeltaTime = World->DeltaTimeSeconds;

	bool bStateChecked = false;
	for (auto& FingerRef : Fingers)
	{
		FVRIK_FingerSolver& Finger = FingerRef.Value;

		if (Finger.Alpha < 1.f)
		{
			Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
		}

		int32 KnuckleIndex = 0;
		for (auto& Knuckle : Finger.Knuckles)
		{
			const FVRIK_FingerRotation* fpp = nullptr;
			switch (FingerRef.Key)
			{
				case EVRIKFingerName::FN_Thumb: fpp = &CurrentFingersPose.ThumbRotation; break;
				case EVRIKFingerName::FN_Index: fpp = &CurrentFingersPose.IndexRotation; break;
				case EVRIKFingerName::FN_Middle: fpp = &CurrentFingersPose.MiddleRotation; break;
				case EVRIKFingerName::FN_Ring: fpp = &CurrentFingersPose.RingRotation; break;
				case EVRIKFingerName::FN_Pinky: fpp = &CurrentFingersPose.PinkyRotation; break;
			}
			if (!fpp) continue;

			// For all knucles after first cut off negative curl
			const float CurlVal = (KnuckleIndex == 0) ? fpp->CurlValue + fpp->FirstKnuckleCurlAddend : FMath::Clamp(fpp->CurlValue, -0.1f, 2.f);

			// Calculate target rotation in parent bone space
			FRotator AddRot = FRotator::ZeroRotator;
			SetRotationAxisValue(AddRot, Finger.FingerOrientation.RightAxis, CurlVal * 90.f);
			if (KnuckleIndex == 0)
			{
				SetRotationAxisValue(AddRot, Finger.FingerOrientation.UpAxis, fpp->SpreadValue * 20.f * HandSideMultiplier);
				SetRotationAxisValue(AddRot, Finger.FingerOrientation.ForwardAxis, fpp->RollValue * 20.f * HandSideMultiplier);
			}

			const FRotator BaseRot = Knuckle.RefPoseRelativeTransform.Rotator();
			const FRotator NewRot = AddLocalRotation(AddRot, BaseRot);

			// Delta between current and target rotation
			const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(NewRot, Knuckle.RelativeTransform.Rotator());
			// Don't update
			if (!bStateChecked && Delta.IsZero()) continue;

			// Update relative transform
			if (InterpolationAlpha == 1.f || !bUseInterpolation || Delta.IsNearlyZero(0.01f))
			{
				// Is close? Set target value.
				Knuckle.RelativeTransform.SetRotation(NewRot.Quaternion());
			}
			else
			{
				// Not close? Interpolate.
				FQuat CurrRot = FQuat::Slerp(CachedHandPose[FingerRef.Key][KnuckleIndex], NewRot.Quaternion(), InterpolationAlpha);
				Knuckle.RelativeTransform.SetRotation(CurrRot);
			}

			// Index increment
			KnuckleIndex++;
		}
	}
}

void UVRIKFingersFKIKSolver::ApplyVRInput(const FVRIK_FingersPosePreset& NewFingersRotation)
{
	VRInput = NewFingersRotation;
	bHasVRInputInFrame = true;
}

void UVRIKFingersFKIKSolver::ApplyVRInputDetailed(const FVRIK_FingersDetailedInfo& NewFingersRotation)
{
	VRInputDetailed = NewFingersRotation;
	bHasDetailedVRInputInFrame = true;
	bDetailedVRInputIsRaw = false;
}

void UVRIKFingersFKIKSolver::ApplyVRInputRaw(const FVRIK_FingersRawPreset& RawFingersRotation)
{
	VRInputRaw = RawFingersRotation;
	bHasDetailedVRInputInFrame = true;
	bDetailedVRInputIsRaw = true;
}

bool UVRIKFingersFKIKSolver::ApplyOpenXRInput()
{
	FVRIK_FingersRawPreset Pose;
	if (HandHelpers::GetHandFingersRotation(Mesh, (Hand == EVRIK_VRHand::VRH_Right), Pose))
	{
		ApplyVRInputRaw(Pose);
		return true;
	}
	return false;
}

void UVRIKFingersFKIKSolver::ProcessVRInput()
{
	if (!bIsInitialized)
	{
		// Only in runtime
		return;
	}

	UWorld* World = Mesh->GetWorld();
	if (!World)
	{
		return;
	}

	const float DeltaTime = World->DeltaTimeSeconds;

	for (auto& VRSt : VRStatus)
	{
		VRSt.Value = false;
	}

	for (auto& FingerRef : Fingers)
	{
		auto& Finger = FingerRef.Value;
		if (!Finger.bEnabled || Finger.KnucklesNum == 0)
		{
			continue;
		}

		FVRIK_FingerRotation FRot;
		switch (FingerRef.Key)
		{
			case EVRIKFingerName::FN_Index: FRot = VRInput.IndexRotation; break;
			case EVRIKFingerName::FN_Middle: FRot = VRInput.MiddleRotation; break;
			case EVRIKFingerName::FN_Ring: FRot = VRInput.RingRotation; break;
			case EVRIKFingerName::FN_Pinky: FRot = VRInput.PinkyRotation; break;
			case EVRIKFingerName::FN_Thumb: default: FRot = VRInput.ThumbRotation; break;
		}

		VRStatus[FingerRef.Key] = true;

		// finger isn't traced or isn't gripping Cnuckles/Touch controller
		// set curl value
		FTransform BaseHandTr = Mesh->GetSocketTransform(Finger.RootBoneName);
		FTransform StartTransform = Finger.Knuckles[0].InputRefPoseRelativeTransform * BaseHandTr;

		// if has grip ref pose - interpolate between grip and non-grip
		if (bHasGripInputPose)
		{
			for (auto& Knuckle : Finger.Knuckles)
			{
				const FQuat TargetQuat = FMath::Lerp(
					Knuckle.InputRefPoseRelativeTransform.GetRotation(),
					Knuckle.InputGripPoseRelativeTransform.GetRotation(),
					FRot.CurlValue);

				Knuckle.RelativeTransform.SetRotation(TargetQuat);
			}

			if (Finger.Alpha < 1.f)
			{
				Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
			}
		}
		// finger is gripping in real reality and grabbing object in vr
		else if (TracingStatus[FingerRef.Key])
		{
			auto& Knuckle0 = Finger.Knuckles[0];
			float Value = FMath::Lerp(InputMaxRotation, InputMinRotation, 1.f - FRot.CurlValue);
			const FVRIK_OrientTransform FingerOrient = Finger.FingerOrientation;

			// reference vectors
			const FRotator& CurrentRot = Knuckle0.RelativeTransform.Rotator();
			float CurrentCurlAngle;
			switch (FingerOrient.RightAxis)
			{
				case EVRIK_BoneOrientationAxis::X: CurrentCurlAngle = -CurrentRot.Roll; break;
				case EVRIK_BoneOrientationAxis::X_Neg: CurrentCurlAngle = CurrentRot.Roll; break;
				case EVRIK_BoneOrientationAxis::Y: CurrentCurlAngle = -CurrentRot.Pitch; break;
				case EVRIK_BoneOrientationAxis::Y_Neg: CurrentCurlAngle = CurrentRot.Pitch; break;
				case EVRIK_BoneOrientationAxis::Z: CurrentCurlAngle = CurrentRot.Yaw; break;
				case EVRIK_BoneOrientationAxis::Z_Neg:
				default: CurrentCurlAngle = -CurrentRot.Yaw; break;
			}

			// adjust alpha
			if (CurrentCurlAngle < Value)
			{
				Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
			}
			else
			{
				Finger.Alpha = 1.f - (CurrentCurlAngle - Value) / (CurrentCurlAngle);
			}
		}
		else
		{
			float ValueCurl = FMath::Lerp(InputMinRotation, InputMaxRotation, FRot.CurlValue);
			const float ValueRoll = FMath::Lerp(InputMinRotation, InputMaxRotation, FRot.RollValue);
			const float ValueSpread = FMath::Lerp(InputMinRotation, InputMaxRotation, FRot.SpreadValue);

			for (int32 Index = 0; Index < Finger.KnucklesNum; Index++)
			{
				auto& Knuckle = Finger.Knuckles[Index];
				// last knuckle - half curl
				if (Index == Finger.KnucklesNum - 1) ValueCurl *= 0.5f;

				FRotator AddRot = FRotator::ZeroRotator;
				SetRotationAxisValue(AddRot, Finger.FingerOrientation.RightAxis, ValueCurl);
				if (Index == 0)
				{
					SetRotationAxisValue(AddRot, Finger.FingerOrientation.UpAxis, ValueSpread * HandSideMultiplier);
					SetRotationAxisValue(AddRot, Finger.FingerOrientation.ForwardAxis, ValueRoll * HandSideMultiplier);
				}
				const FRotator BaseRot = Knuckle.InputRefPoseRelativeTransform.Rotator();
				const FRotator NewRot = AddLocalRotation(AddRot, BaseRot);

				if (Finger.Alpha < 1.f)
				{
					Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
				}

				Knuckle.RelativeTransform.SetRotation(NewRot.Quaternion());
			}
		}
	}
}

void UVRIKFingersFKIKSolver::ProcessVRInputDetailed()
{
	if (!bIsInitialized)
	{
		// Only in runtime
		return;
	}

	UWorld* World = Mesh->GetWorld();
	if (!World)
	{
		return;
	}

	const float DeltaTime = World->DeltaTimeSeconds;

	for (auto& VRSt : VRStatus)
	{
		VRSt.Value = false;
	}

	for (auto& FingerRef : Fingers)
	{
		auto& Finger = FingerRef.Value;
		if (!Finger.bEnabled || Finger.KnucklesNum == 0)
		{
			continue;
		}

		if (TracingStatus[FingerRef.Key])
		{
			continue;
		}
		
		const TArray<FVRIK_FingerRotation>* FRot;
		switch (FingerRef.Key)
		{
			case EVRIKFingerName::FN_Index: FRot = &VRInputDetailed.IndexBones; break;
			case EVRIKFingerName::FN_Middle: FRot = &VRInputDetailed.MiddleBones; break;
			case EVRIKFingerName::FN_Ring: FRot = &VRInputDetailed.RingBones; break;
			case EVRIKFingerName::FN_Pinky: FRot = &VRInputDetailed.PinkyBones; break;
			case EVRIKFingerName::FN_Thumb: default: FRot = &VRInputDetailed.ThumbBones; break;
		}

		VRStatus[FingerRef.Key] = true;

		// finger isn't traced or isn't gripping Cnuckles/Touch controller
		// set curl value

		for (int32 Index = 0; Index < Finger.KnucklesNum; Index++)
		{
			FVRIK_FingerRotation rot = FRot->Num() > Index ? (*FRot)[Index] : FVRIK_FingerRotation();
			auto& Knuckle = Finger.Knuckles[Index];

			FRotator AddRot = FRotator::ZeroRotator;
			float SideMultiplier = HandSideMultiplier;
			float FirstBoneCurlOffset = (Index == 0) ? rot.FirstKnuckleCurlAddend : 0.f;
			SetRotationAxisValue(AddRot, Finger.FingerOrientation.RightAxis, rot.CurlValue + FirstBoneCurlOffset);
			SetRotationAxisValue(AddRot, Finger.FingerOrientation.UpAxis, rot.SpreadValue * SideMultiplier);
			SetRotationAxisValue(AddRot, Finger.FingerOrientation.ForwardAxis, rot.RollValue * SideMultiplier);

			const FRotator BaseRot = Knuckle.InputRefPoseRelativeTransform.Rotator();
			const FRotator NewRot = AddLocalRotation(AddRot, BaseRot);

			if (Finger.Alpha < 1.f)
			{
				Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
			}

			Knuckle.RelativeTransform.SetRotation(NewRot.Quaternion());
		}
	}
}

void UVRIKFingersFKIKSolver::ProcessFingersPoseRaw(const FVRIK_FingersRawPreset& Pose, bool bUseInterpolation)
{
	if (!bIsInitialized)
	{
		// Only in runtime
		return;
	}

	UWorld* World = Mesh->GetWorld();
	if (!World)
	{
		return;
	}

	const float DeltaTime = World->DeltaTimeSeconds;

	for (auto& VRSt : VRStatus)
	{
		VRSt.Value = false;
	}

	for (auto& FingerRef : Fingers)
	{
		auto& Finger = FingerRef.Value;
		if (!Finger.bEnabled || Finger.KnucklesNum == 0)
		{
			continue;
		}

		if (TracingStatus[FingerRef.Key])
		{
			continue;
		}

		const FVRIK_FingerRawRotation* FingerRot;
		switch (FingerRef.Key)
		{
			case EVRIKFingerName::FN_Index: FingerRot = &Pose.Index; break;
			case EVRIKFingerName::FN_Middle: FingerRot = &Pose.Middle; break;
			case EVRIKFingerName::FN_Ring: FingerRot = &Pose.Ring; break;
			case EVRIKFingerName::FN_Pinky: FingerRot = &Pose.Pinky; break;
			case EVRIKFingerName::FN_Thumb: default: FingerRot = &Pose.Thumb; break;
		}

		VRStatus[FingerRef.Key] = true;

		if (FingerRot->Curls.Num() == 0) continue;

		FTransform GenericFromWristConverter = FTransform::Identity.GetRelativeTransform(WristFromGenericConverter);
		// @TODO: use bone converter instead of WristFromGenericConverter
		FRotator FirstKnuckleRot = FRotator(FingerRot->Curls[0] * -1.f, FingerRot->FingerYaw, FingerRot->FingerRoll);
		FTransform RelToHandBone = WristFromGenericConverter * FTransform(FirstKnuckleRot) * GenericFromWristConverter;

		// Has metacarpal bone?
		if (Finger.RootBoneName != WristBoneName)
		{
			RelToHandBone = RelToHandBone.GetRelativeTransform(Finger.RootBoneToWristOffset);
		}

		if (bUseInterpolation && InterpolationAlpha < 1.f)
		{
			// Interpolation is used with poses
			FQuat CurrRot = FQuat::Slerp(CachedHandPose[FingerRef.Key][0], RelToHandBone.GetRotation(), InterpolationAlpha);
			Finger.Knuckles[0].RelativeTransform.SetRotation(CurrRot);
		}
		else
		{
			Finger.Knuckles[0].RelativeTransform.SetRotation(RelToHandBone.GetRotation());
		}

		for (int32 Index = 1; Index < Finger.KnucklesNum; Index++)
		{
			float RotCurl = FingerRot->Curls.Num() > Index ? FingerRot->Curls[Index] : 0.f;
			auto& Knuckle = Finger.Knuckles[Index];

			FRotator RelativeRot = FRotator::ZeroRotator;
			SetRotationAxisValue(RelativeRot, Finger.FingerOrientation.RightAxis, RotCurl);

			if (Finger.Alpha < 1.f)
			{
				Finger.Alpha = FMath::FInterpTo(Finger.Alpha, 1.f, DeltaTime, 18.f);
			}

			if (bUseInterpolation && InterpolationAlpha < 1.f)
			{
				// Interpolation is used with poses
				FQuat CurrRot = FQuat::Slerp(CachedHandPose[FingerRef.Key][Index], RelativeRot.Quaternion(), InterpolationAlpha);
				Knuckle.RelativeTransform.SetRotation(CurrRot);
			}
			else
			{
				Knuckle.RelativeTransform.SetRotation(RelativeRot.Quaternion());
			}
		}
	}
}

void UVRIKFingersFKIKSolver::AddRotationAxisValue(FRotator& OutRot, EVRIK_BoneOrientationAxis Axis, float Value)
{
	switch (Axis)
	{
		case EVRIK_BoneOrientationAxis::X: OutRot.Roll -= Value; break;
		case EVRIK_BoneOrientationAxis::X_Neg: OutRot.Roll += Value; break;
		case EVRIK_BoneOrientationAxis::Y: OutRot.Pitch -= Value; break;
		case EVRIK_BoneOrientationAxis::Y_Neg: OutRot.Pitch += Value; break;
		case EVRIK_BoneOrientationAxis::Z: OutRot.Yaw += Value; break;
		case EVRIK_BoneOrientationAxis::Z_Neg: OutRot.Yaw -= Value; break;
	}
}

void UVRIKFingersFKIKSolver::CacheFingersPose()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Log, TEXT("CacheFingersPose called for non-initialized object"));
		return;
	}
	UWorld* World = Mesh->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("CacheFingersPose error: can't get world"));
		return;
	}

	InterpolationStartTime = World->GetRealTimeSeconds();
	InterpolationAlpha = 0.f;

	for (const auto& FingerRef : Fingers)
	{
		TArray<FQuat>& FingerRotators = CachedHandPose[FingerRef.Key];
		int32 BonesNum = FingerRef.Value.KnucklesNum;

		if (FingerRotators.Num() < BonesNum)
		{
			FingerRotators.SetNumUninitialized(BonesNum);
		}

		for (int32 BoneIndex = 0; BoneIndex < BonesNum; ++BoneIndex)
		{
			FingerRotators[BoneIndex] = FingerRef.Value.Knuckles[BoneIndex].RelativeTransform.GetRotation();
		}
	}
}

// Helper function to apply rotation
void UVRIKFingersFKIKSolver::SetRotationAxisValue(FRotator& OutRot, EVRIK_BoneOrientationAxis Axis, float Value)
{
	switch (Axis)
	{
		case EVRIK_BoneOrientationAxis::X: OutRot.Roll = -Value; break;
		case EVRIK_BoneOrientationAxis::X_Neg: OutRot.Roll = Value; break;
		case EVRIK_BoneOrientationAxis::Y: OutRot.Pitch = -Value; break;
		case EVRIK_BoneOrientationAxis::Y_Neg: OutRot.Pitch = Value; break;
		case EVRIK_BoneOrientationAxis::Z: OutRot.Yaw = Value; break;
		case EVRIK_BoneOrientationAxis::Z_Neg: OutRot.Yaw = -Value; break;
	}
}

FRotator UVRIKFingersFKIKSolver::AddLocalRotation(const FRotator& AdditionRot, const FRotator& BaseRot)
{
	return UKismetMathLibrary::ComposeRotators(BaseRot.GetInverse(), AdditionRot.GetInverse()).GetInverse();
}

void UVRIKFingersFKIKSolver::GetFingerKnuckles(EVRIKFingerName FingerName, TArray<FVRIK_Knuckle>& OutKnuckles)
{
	FVRIK_FingerSolver* Finger = Fingers.Find(FingerName);

	if (Finger)
	{
		OutKnuckles = Finger->Knuckles;
	}
	else
	{
		OutKnuckles.Empty();
	}
}

FString UVRIKFingersFKIKSolver::GetFingerDescription(EVRIKFingerName FingerName) const
{
	if (!Fingers.Contains(FingerName))
	{
		return "";
	}

	const auto& Finger = Fingers[FingerName];
	FString sz = "A [" + FString::SanitizeFloat(Finger.Alpha) + "] ";
	for (auto& Knuckle : Finger.Knuckles)
	{
		sz += (Knuckle.BoneName.ToString() + " [ loc=" + Knuckle.RelativeTransform.GetTranslation().ToString() + " rot=" + Knuckle.RelativeTransform.Rotator().ToString() + "] ");
	}

	return sz.TrimEnd();
}

// Get reference transform of knuckle in world space
FTransform UVRIKFingersFKIKSolver::GetKnuckleRefTransform(const FVRIK_FingerSolver& Finger, int32 KnuckleIndex)
{
	FTransform ret = FTransform::Identity;
	for (int32 Index = KnuckleIndex; Index >= 0; Index++)
	{
		ret = ret * Finger.Knuckles[Index].RelativeTransform;
	}
	ret = ret * Mesh->GetSocketTransform(Finger.RootBoneName);
	return ret;
}

#undef _refpose_input
#undef _refpose_trace
#undef _refpose_grip