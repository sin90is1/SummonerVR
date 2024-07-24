// VR IK Body Plugin
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#include "VRIKSkeletalMeshTranslator.h"
// ENGINE
#include "GameFramework/Pawn.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/PoseSnapshot.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/Engine.h"
// PROJECT
#include "VRIKBodyMath.h"
#include "VRIKBody.h"
#include "RangeLimitedFABRIK.h"

DEFINE_LOG_CATEGORY(LogVRIKSkeletalMeshTranslator);

#ifndef AccessRefSkeleton
	#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 26
		#define AccessRefSkeleton(SkelMesh) SkelMesh->GetRefSkeleton()
	#else
		#define AccessRefSkeleton(SkelMesh) SkelMesh->RefSkeleton
	#endif
#endif

#define RotatorDirection(Rotator, Axis) FRotationMatrix(Rotator).GetScaledAxis(Axis)
// universal macros to get bones direction in 'normal' space independent from a skeleton
#define PelvisForwardVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.PelvisRotation.VerticalAxis) * BodySetup.PelvisRotation.UpDirection
#define PelvisUpVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.PelvisRotation.ForwardAxis) * BodySetup.PelvisRotation.ForwardDirection
#define PelvisRightVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.PelvisRotation.HorizontalAxis) * BodySetup.PelvisRotation.RightDirection
#define RibcageForwardVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.RibcageRotation.VerticalAxis) * BodySetup.RibcageRotation.UpDirection
#define RibcageUpVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.RibcageRotation.ForwardAxis) * BodySetup.RibcageRotation.ForwardDirection
#define RibcageRightVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.RibcageRotation.HorizontalAxis) * BodySetup.RibcageRotation.RightDirection
#define FootRightForwardVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.FootRightRotation.ForwardAxis) * BodySetup.FootRightRotation.ForwardDirection
#define FootRightUpVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.FootRightRotation.VerticalAxis) * BodySetup.FootRightRotation.UpDirection
#define FootLeftForwardVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.FootLeftRotation.ForwardAxis) * BodySetup.FootLeftRotation.ForwardDirection
#define FootLeftUpVector(Rotator) FRotationMatrix(Rotator).GetScaledAxis(BodySetup.FootLeftRotation.VerticalAxis) * BodySetup.FootLeftRotation.UpDirection

#define _CHECKBONE(BoneName) if (sk->FindBoneIndex(BoneName) == INDEX_NONE) { UE_LOG(LogVRIKSkeletalMeshTranslator, Error, TEXT("[UpdateSkeletonBonesSetup] Invalid bone name: %s"), *BoneName.ToString()); return false; }
#define _CHECKTRIAL if (bTrial) { const FDateTime dt = UKismetMathLibrary::UtcNow(); if (dt.GetYear() > TrialYear || (dt.GetYear() == TrialYear && dt.GetMonth() >= TrialMonth)) return false; }

UVRIKSkeletalMeshTranslator::UVRIKSkeletalMeshTranslator()
{
	// don't need tick
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	// default bone names for UE4 Mannequin
	RootBone = TEXT("root");
	Pelvis = TEXT("pelvis");
	Ribcage = TEXT("spine_03");
	Head = TEXT("head");
	ShoulderRight = TEXT("clavicle_r");
	UpperarmRight = TEXT("upperarm_r");
	LowerarmRight = TEXT("lowerarm_r");
	PalmRight = TEXT("hand_r");
	ShoulderLeft = TEXT("clavicle_l");
	UpperarmLeft = TEXT("upperarm_l");
	LowerarmLeft = TEXT("lowerarm_l");
	PalmLeft = TEXT("hand_l");
	ThighRight = TEXT("thigh_r");
	CalfRight = TEXT("calf_r");
	FootRight = TEXT("foot_r");
	ThighLeft = TEXT("thigh_l");
	CalfLeft = TEXT("calf_l");
	FootLeft = TEXT("foot_l");
	ScaleMultiplierVertical = ScaleMultiplierHorizontal = 1.f;
	bHandScaleFromUpperbone = false;

	bDisableFlexibleTorso = false;
	bCalculateAnimatedLegs = true;

	bTrial = false;
	TrialMonth = 1;
	TrialYear = 2022;

	bRescaleMesh = true;
	bRootMotion = true;
	bInvertElbows = false;
	bHandsIKLateUpdate = true;
	bLockRootBoneRotation = false;
	bComponentWasMovedInTick = false;

	SpineVerticalOffset = 0.f;
	MeshHeadHalfHeight = 10.f;
	HandTwistLimit = 70.f;
	CurrentBlendAnimationAlpha = 0.f;

	ComponentForwardYaw = -90.f;
	SpineBendingMultiplier = 0.2f;
}

void UVRIKSkeletalMeshTranslator::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);
	SetTickGroup(ETickingGroup::TG_LastDemotable);
	bIsInitialized = bIsCalibrated = false;
}

void UVRIKSkeletalMeshTranslator::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsInitialized && bIsCalibrated && VRIKSolver && VRIKSolver->IsInitialized())
	{
		UpdateSkeleton();
	}
}

void UVRIKSkeletalMeshTranslator::UpdateSkeleton()
{
	if (bIsInitialized && bIsCalibrated && VRIKSolver && VRIKSolver->IsInitialized())
	{
		// Load bones from VRIKBody, apply reorientation and calc intermediate bones
		UpdateBonesState();

		bComponentWasMovedInTick = true;
		BodyMesh->SetRelativeLocationAndRotation(ComponentActorTransform.GetTranslation(), ComponentActorTransform.GetRotation());
	}
}

bool UVRIKSkeletalMeshTranslator::Initialize(class USkinnedMeshComponent* ControlledSkeletalMesh, class UVRIKBody* VRIKBodyComponent)
{
	_CHECKTRIAL;

	if (!VRIKBodyComponent || ControlledSkeletalMesh)
	{
		TSet<UActorComponent*> Components = GetOwner()->GetComponents();

		// try to get from owner pawn
		if (!VRIKBodyComponent)
		{
			for (auto& Comp : Components)
			{
				if (Comp->IsA(UVRIKBody::StaticClass()))
				{
					VRIKBodyComponent = Cast<UVRIKBody>(Comp);
					break;
				}
			}
		}
		// try to get from owner pawn
		if (!ControlledSkeletalMesh)
		{
			for (auto& Comp : Components)
			{
				if (Comp->IsA(UVRIKBody::StaticClass()))
				{
					UE_LOG(LogVRIKSkeletalMeshTranslator, Warning, TEXT("[Initialize] Invalid Skeletal Mesh reference. Using first Skeletal Mesh in owning actor instead."));
					ControlledSkeletalMesh = Cast<USkinnedMeshComponent>(Comp);
					break;
				}
			}
		}
	}

	// if reinitialize - remove previous delegate binding
	if (VRIKSolver)
	{
		VRIKSolver->OnCalibrationComplete.RemoveDynamic(this, &UVRIKSkeletalMeshTranslator::VRIKBody_OnCalibrationComplete);
	}

	// update references
	VRIKSolver = VRIKBodyComponent;
	BodyMesh = ControlledSkeletalMesh;

	if (bUpdateAutomaticallyInTick)
	{
		AddTickPrerequisiteComponent(VRIKSolver);
		SetComponentTickEnabled(true);
	}

	VRIKSolver->OnCalibrationComplete.AddDynamic(this, &UVRIKSkeletalMeshTranslator::VRIKBody_OnCalibrationComplete);

	// calculate bones orientation
	bIsInitialized = UpdateSkeletonBonesSetup();

	return bIsInitialized;
}


bool UVRIKSkeletalMeshTranslator::UpdateSkeletonBonesSetup()
{
	if (!VRIKSolver || !BodyMesh || !IsValid(__uev_GetSkeletalMesh(BodyMesh)))
	{
		return false;
	}

	_CHECKTRIAL;

	FTransform t1, t2, t3;
	FVector v0;

	// Return false if bone names aren't initlized
	const FReferenceSkeleton* sk = &AccessRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	_CHECKBONE(Pelvis);
	_CHECKBONE(Ribcage);
	_CHECKBONE(Head);
	_CHECKBONE(ShoulderRight);
	_CHECKBONE(UpperarmRight);
	_CHECKBONE(LowerarmRight);
	_CHECKBONE(PalmRight);
	_CHECKBONE(ShoulderLeft);
	_CHECKBONE(UpperarmLeft);
	_CHECKBONE(LowerarmLeft);
	_CHECKBONE(PalmLeft);
	_CHECKBONE(ThighRight);
	_CHECKBONE(CalfRight);
	_CHECKBONE(FootRight);
	_CHECKBONE(ThighLeft);
	_CHECKBONE(CalfLeft);
	_CHECKBONE(FootLeft);

	// get major bones transforms
	// Skeletal mesh MUST BE in T Pose or /|\ Pose (A-Pose)
	FTransform tPelvis = BodyMesh->GetSocketTransform(Pelvis); tPelvis.SetScale3D(FVector::OneVector);
	const FTransform tRibcage = BodyMesh->GetSocketTransform(Ribcage);
	const FTransform tHead = BodyMesh->GetSocketTransform(Head);
	const FTransform tShoulderRight = BodyMesh->GetSocketTransform(ShoulderRight);
	const FTransform tUpperarmRight = BodyMesh->GetSocketTransform(UpperarmRight);
	const FTransform tForearmRight = BodyMesh->GetSocketTransform(LowerarmRight);
	const FTransform tPalmRight = BodyMesh->GetSocketTransform(PalmRight);
	const FTransform tShoulderLeft = BodyMesh->GetSocketTransform(ShoulderLeft);
	const FTransform tUpperarmLeft = BodyMesh->GetSocketTransform(UpperarmLeft);
	const FTransform tForearmLeft = BodyMesh->GetSocketTransform(LowerarmLeft);
	const FTransform tPalmLeft = BodyMesh->GetSocketTransform(PalmLeft);
	const FTransform tThighRight = BodyMesh->GetSocketTransform(ThighRight);
	const FTransform tCalfRight = BodyMesh->GetSocketTransform(CalfRight);
	const FTransform tFootRight = BodyMesh->GetSocketTransform(FootRight);
	const FTransform tThighLeft = BodyMesh->GetSocketTransform(ThighLeft);
	const FTransform tCalfLeft = BodyMesh->GetSocketTransform(CalfLeft);
	const FTransform tFootLeft = BodyMesh->GetSocketTransform(FootLeft);

	// initialize skeleton bones relationship

	// 1. Distances and scales.

	BodySetup.MeshUpperarmLength = FVector::Dist(tUpperarmRight.GetTranslation(), tForearmRight.GetTranslation());
	BodySetup.MeshForearmLength = FVector::Dist(tForearmRight.GetTranslation(), tPalmRight.GetTranslation());
	BodySetup.MeshHandsFullLength = (BodySetup.MeshUpperarmLength + BodySetup.MeshForearmLength) * 2.f + FVector::Dist(tUpperarmRight.GetTranslation(), tUpperarmLeft.GetTranslation());
	BodySetup.MeshHeight = tHead.GetTranslation().Z - BodyMesh->GetComponentLocation().Z;

	BodySetup.fUpperarmLength = BodySetup.MeshUpperarmLength;
	BodySetup.fForearmLength = BodySetup.MeshForearmLength;

	BodySetup.SpineLength = (tRibcage.GetTranslation() - tPelvis.GetTranslation()).Size();

	// 2. Relative Transforms.

	BodySetup.ShoulderToRibcageRight = tShoulderRight.GetRelativeTransform(tRibcage);
	BodySetup.ShoulderToRibcageLeft = tShoulderLeft.GetRelativeTransform(tRibcage);

	BodySetup.UpperarmToShoulderRight = tUpperarmRight.GetRelativeTransform(tShoulderRight);
	BodySetup.UpperarmToShoulderLeft = tUpperarmLeft.GetRelativeTransform(tShoulderLeft);

	t1 = BodyMesh->GetSocketTransform(BodyMesh->GetParentBone(Head));
	BodySetup.HeadToNeck = tHead.GetRelativeTransform(t1);

	t1 = BodyMesh->GetSocketTransform(BodyMesh->GetParentBone(Ribcage));
	BodySetup.RibcageToSpine = tRibcage.GetRelativeTransform(t1);

	// 3. Bones orientation.

	// root
	if (!RootBone.IsNone() && RootBone != Pelvis)
	{
		BodySetup.RootBoneRotation = BodyMesh->GetSocketTransform(RootBone, ERelativeTransformSpace::RTS_Component).Rotator();
		bHasRootBone = true;
	}
	else
	{
		BodySetup.RootBoneRotation = BodyMesh->GetSocketTransform(Pelvis, ERelativeTransformSpace::RTS_Component).Rotator();
		bHasRootBone = false;
	}

	// Spine Up-Down
	BodySetup.PelvisRotation.ForwardAxis = FindCoDirection(tPelvis.Rotator(), FVector::UpVector, BodySetup.PelvisRotation.ForwardDirection);
	BodySetup.RibcageRotation.ForwardAxis = FindCoDirection(tRibcage.Rotator(), FVector::UpVector, BodySetup.RibcageRotation.ForwardDirection);
	BodySetup.HeadRotation.ForwardAxis = FindCoDirection(tHead.Rotator(), FVector::UpVector, BodySetup.HeadRotation.ForwardDirection);

	// spine forward

	// mesh/pelvis forward axis
	FVector SkMForward = (tPalmRight.GetTranslation() - tRibcage.GetTranslation()).GetSafeNormal2D();
	SkMForward = SkMForward.RotateAngleAxis(-90.f, FVector(0.f, 0.f, 1.f));

	if (bDrawSkeletonSetup)
	{
		DrawDebugLine(GetWorld(), tPelvis.GetTranslation(), tPelvis.GetTranslation() + SkMForward * 30.f, FColor::Red, false, 5.f, 0, 0.5f);
	}

	BodySetup.PelvisRotation.VerticalAxis = FindCoDirection(tPelvis.Rotator(), SkMForward, BodySetup.PelvisRotation.UpDirection);
	BodySetup.RibcageRotation.VerticalAxis = FindCoDirection(tRibcage.Rotator(), SkMForward, BodySetup.RibcageRotation.UpDirection);
	BodySetup.HeadRotation.VerticalAxis = FindCoDirection(tHead.Rotator(), SkMForward, BodySetup.HeadRotation.UpDirection);

	// spine right
	v0 = (tPalmRight.GetTranslation() - tRibcage.GetTranslation()).GetSafeNormal2D();

	BodySetup.PelvisRotation.HorizontalAxis = FindCoDirection(tPelvis.Rotator(), v0, BodySetup.PelvisRotation.RightDirection);
	BodySetup.RibcageRotation.HorizontalAxis = FindCoDirection(tRibcage.Rotator(), v0, BodySetup.RibcageRotation.RightDirection);
	BodySetup.HeadRotation.HorizontalAxis = FindCoDirection(tHead.Rotator(), v0, BodySetup.HeadRotation.RightDirection);

	// right hand forward
	v0 = tPalmRight.GetTranslation() - tUpperarmRight.GetTranslation();
	BodySetup.RightHandRotation.ForwardAxis = FindCoDirection(tUpperarmRight.Rotator(), v0, BodySetup.RightHandRotation.ForwardDirection);

	// left hand forward
	v0 = tPalmLeft.GetTranslation() - tUpperarmLeft.GetTranslation();
	BodySetup.LeftHandRotation.ForwardAxis = FindCoDirection(tUpperarmLeft.Rotator(), v0, BodySetup.LeftHandRotation.ForwardDirection);

	// right hand plane axes
	v0 = PelvisForwardVector(tPelvis.Rotator()) * -1.f; // back direction
	BodySetup.RightHandRotation.HorizontalAxis = FindCoDirection(tUpperarmRight.Rotator(), v0, BodySetup.RightHandRotation.ExternalDirection);

	// left hand rotation axes
	BodySetup.LeftHandRotation.HorizontalAxis = FindCoDirection(tUpperarmLeft.Rotator(), v0, BodySetup.LeftHandRotation.ExternalDirection);

	// right arm up axis		
	if (BodySetup.RightHandRotation.ForwardAxis != EAxis::X && BodySetup.RightHandRotation.HorizontalAxis != EAxis::X)
		BodySetup.RightHandRotation.VerticalAxis = EAxis::X;
	else if (BodySetup.RightHandRotation.ForwardAxis != EAxis::Y && BodySetup.RightHandRotation.HorizontalAxis != EAxis::Y)
		BodySetup.RightHandRotation.VerticalAxis = EAxis::Y;
	else if (BodySetup.RightHandRotation.ForwardAxis != EAxis::Z && BodySetup.RightHandRotation.HorizontalAxis != EAxis::Z)
		BodySetup.RightHandRotation.VerticalAxis = EAxis::Z;

	if (FVector::DotProduct(FVector(0.f, 0.f, 1.f), RotatorDirection(tUpperarmRight.Rotator(), BodySetup.RightHandRotation.VerticalAxis)) > 0.f) BodySetup.RightHandRotation.UpDirection = 1.f; else BodySetup.RightHandRotation.UpDirection = -1.f;

	// left arm up axis		
	if (BodySetup.LeftHandRotation.ForwardAxis != EAxis::X && BodySetup.LeftHandRotation.HorizontalAxis != EAxis::X)
		BodySetup.LeftHandRotation.VerticalAxis = EAxis::X;
	else if (BodySetup.LeftHandRotation.ForwardAxis != EAxis::Y && BodySetup.LeftHandRotation.HorizontalAxis != EAxis::Y)
		BodySetup.LeftHandRotation.VerticalAxis = EAxis::Y;
	else if (BodySetup.LeftHandRotation.ForwardAxis != EAxis::Z && BodySetup.LeftHandRotation.HorizontalAxis != EAxis::Z)
		BodySetup.LeftHandRotation.VerticalAxis = EAxis::Z;

	if (FVector::DotProduct(FVector(0.f, 0.f, 1.f), RotatorDirection(tUpperarmLeft.Rotator(), BodySetup.LeftHandRotation.VerticalAxis)) > 0.f) BodySetup.LeftHandRotation.UpDirection = 1.f; else BodySetup.LeftHandRotation.UpDirection = -1.f;

	// right shoulder forward
	v0 = tUpperarmRight.GetTranslation() - tShoulderRight.GetTranslation();
	BodySetup.RightShoulderRotation.ForwardAxis = FindCoDirection(tShoulderRight.Rotator(), v0, BodySetup.RightShoulderRotation.ForwardDirection);

	// left shoulder forward
	v0 = tUpperarmLeft.GetTranslation() - tShoulderLeft.GetTranslation();
	BodySetup.LeftShoulderRotation.ForwardAxis = FindCoDirection(tShoulderLeft.Rotator(), v0, BodySetup.LeftShoulderRotation.ForwardDirection);

	// right shoulder plane axes
	v0 = PelvisForwardVector(tPelvis.Rotator()) * -1.f; // back direction
	BodySetup.RightShoulderRotation.HorizontalAxis = FindCoDirection(tShoulderRight.Rotator(), v0, BodySetup.RightShoulderRotation.ExternalDirection);

	// left shoulder rotation axes
	BodySetup.LeftShoulderRotation.HorizontalAxis = FindCoDirection(tShoulderLeft.Rotator(), v0, BodySetup.LeftShoulderRotation.ExternalDirection);

	// right shoulder up axis		
	if (BodySetup.RightShoulderRotation.ForwardAxis != EAxis::X && BodySetup.RightShoulderRotation.HorizontalAxis != EAxis::X)
		BodySetup.RightShoulderRotation.VerticalAxis = EAxis::X;
	else if (BodySetup.RightShoulderRotation.ForwardAxis != EAxis::Y && BodySetup.RightShoulderRotation.HorizontalAxis != EAxis::Y)
		BodySetup.RightShoulderRotation.VerticalAxis = EAxis::Y;
	else if (BodySetup.RightShoulderRotation.ForwardAxis != EAxis::Z && BodySetup.RightShoulderRotation.HorizontalAxis != EAxis::Z)
		BodySetup.RightShoulderRotation.VerticalAxis = EAxis::Z;

	if (FVector::DotProduct(FVector(0.f, 0.f, 1.f), RotatorDirection(tShoulderRight.Rotator(), BodySetup.RightShoulderRotation.VerticalAxis)) > 0.f) BodySetup.RightShoulderRotation.UpDirection = 1.f; else BodySetup.RightShoulderRotation.UpDirection = -1.f;

	// left shoulder up axis		
	if (BodySetup.LeftShoulderRotation.ForwardAxis != EAxis::X && BodySetup.LeftShoulderRotation.HorizontalAxis != EAxis::X)
		BodySetup.LeftShoulderRotation.VerticalAxis = EAxis::X;
	else if (BodySetup.LeftShoulderRotation.ForwardAxis != EAxis::Y && BodySetup.LeftShoulderRotation.HorizontalAxis != EAxis::Y)
		BodySetup.LeftShoulderRotation.VerticalAxis = EAxis::Y;
	else if (BodySetup.LeftShoulderRotation.ForwardAxis != EAxis::Z && BodySetup.LeftShoulderRotation.HorizontalAxis != EAxis::Z)
		BodySetup.LeftShoulderRotation.VerticalAxis = EAxis::Z;

	if (FVector::DotProduct(FVector(0.f, 0.f, 1.f), RotatorDirection(tShoulderLeft.Rotator(), BodySetup.LeftShoulderRotation.VerticalAxis)) > 0.f) BodySetup.LeftShoulderRotation.UpDirection = 1.f; else BodySetup.LeftShoulderRotation.UpDirection = -1.f;

	// right leg down
	BodySetup.RightLegRotation.ForwardAxis = FindCoDirection(tThighRight.Rotator(), FVector(0.f, 0.f, -1.f), BodySetup.RightLegRotation.ForwardDirection);

	// left leg down
	BodySetup.LeftLegRotation.ForwardAxis = FindCoDirection(tThighLeft.Rotator(), FVector(0.f, 0.f, -1.f), BodySetup.LeftLegRotation.ForwardDirection);

	// right Leg forward axis
	v0 = PelvisForwardVector(tPelvis.Rotator());
	BodySetup.RightLegRotation.HorizontalAxis = FindCoDirection(tThighRight.Rotator(), v0, BodySetup.RightLegRotation.ExternalDirection);

	// left Leg forward axis
	BodySetup.LeftLegRotation.HorizontalAxis = FindCoDirection(tThighLeft.Rotator(), v0, BodySetup.LeftLegRotation.ExternalDirection);

	// right leg side axis
	v0 = PelvisRightVector(tPelvis.Rotator());
	BodySetup.RightLegRotation.VerticalAxis = FindCoDirection(tThighRight.Rotator(), v0, BodySetup.RightLegRotation.UpDirection);
	BodySetup.RightLegRotation.UpDirection *= -1.f;

	// left leg side axis
	BodySetup.LeftLegRotation.VerticalAxis = FindCoDirection(tThighLeft.Rotator(), v0, BodySetup.LeftLegRotation.UpDirection);
	BodySetup.LeftLegRotation.UpDirection *= -1.f;

	// right palm up
	v0 = PelvisForwardVector(tPelvis.Rotator());
	BodySetup.PalmRightRotation.VerticalAxis = FindCoDirection(tPalmRight.Rotator(), v0, BodySetup.PalmRightRotation.UpDirection);

	// right palm forward
	v0 = tPalmRight.GetTranslation() - tRibcage.GetTranslation();
	BodySetup.PalmRightRotation.ForwardAxis = FindCoDirection(tPalmRight.Rotator(), v0, BodySetup.PalmRightRotation.ForwardDirection);

	// right palm right
	v0 = (RotatorDirection(tPalmRight.Rotator(), BodySetup.PalmRightRotation.VerticalAxis) * BodySetup.PalmRightRotation.UpDirection) ^
		(RotatorDirection(tPalmRight.Rotator(), BodySetup.PalmRightRotation.ForwardAxis) * BodySetup.PalmRightRotation.ForwardDirection);
	BodySetup.PalmRightRotation.HorizontalAxis = FindCoDirection(tPalmRight.Rotator(), v0, BodySetup.PalmRightRotation.RightDirection);

	// left palm up
	v0 = PelvisForwardVector(tPelvis.Rotator());
	BodySetup.PalmLeftRotation.VerticalAxis = FindCoDirection(tPalmLeft.Rotator(), v0, BodySetup.PalmLeftRotation.UpDirection);

	// left palm forward
	v0 = tPalmLeft.GetTranslation() - tRibcage.GetTranslation();
	BodySetup.PalmLeftRotation.ForwardAxis = FindCoDirection(tPalmLeft.Rotator(), v0, BodySetup.PalmLeftRotation.ForwardDirection);

	// left palm right
	v0 = (RotatorDirection(tPalmLeft.Rotator(), BodySetup.PalmLeftRotation.VerticalAxis) * BodySetup.PalmLeftRotation.UpDirection) ^
		(RotatorDirection(tPalmLeft.Rotator(), BodySetup.PalmLeftRotation.ForwardAxis) * BodySetup.PalmLeftRotation.ForwardDirection);
	BodySetup.PalmLeftRotation.HorizontalAxis = FindCoDirection(tPalmLeft.Rotator(), v0, BodySetup.PalmLeftRotation.RightDirection);

	// foot right
	v0 = PelvisRightVector(tPelvis.Rotator());

	BodySetup.FootRightRotation.VerticalAxis = FindCoDirection(tFootRight.Rotator(), FVector(0.f, 0.f, 1.f), BodySetup.FootRightRotation.UpDirection);
	BodySetup.FootRightRotation.ForwardAxis = FindCoDirection(tFootRight.Rotator(), SkMForward, BodySetup.FootRightRotation.ForwardDirection);
	BodySetup.FootRightRotation.HorizontalAxis = FindCoDirection(tFootRight.Rotator(), v0, BodySetup.FootRightRotation.RightDirection);

	// foot left
	BodySetup.FootLeftRotation.VerticalAxis = FindCoDirection(tFootLeft.Rotator(), FVector(0.f, 0.f, 1.f), BodySetup.FootLeftRotation.UpDirection);
	BodySetup.FootLeftRotation.ForwardAxis = FindCoDirection(tFootLeft.Rotator(), SkMForward, BodySetup.FootLeftRotation.ForwardDirection);
	BodySetup.FootLeftRotation.HorizontalAxis = FindCoDirection(tFootLeft.Rotator(), v0, BodySetup.FootLeftRotation.RightDirection);

	// right foot offset
	v0 = tFootRight.GetTranslation(); v0.Z = BodyMesh->GetComponentLocation().Z;
	FRotator r0 = UKismetMathLibrary::MakeRotFromX(FootRightForwardVector(tFootRight.Rotator()));
	r0.Pitch = r0.Roll = 0.f;
	t1.SetTranslation(v0);
	t1.SetRotation(r0.Quaternion());
	BodySetup.FootToGroundRight = tFootRight.GetRelativeTransform(t1);

	// left foot offset
	v0 = tFootLeft.GetTranslation(); v0.Z = BodyMesh->GetComponentLocation().Z;
	r0 = UKismetMathLibrary::MakeRotFromX(FootLeftForwardVector(tFootLeft.Rotator()));
	r0.Pitch = r0.Roll = 0.f;
	t1.SetTranslation(v0);
	t1.SetRotation(r0.Quaternion());
	BodySetup.FootToGroundLeft = tFootLeft.GetRelativeTransform(t1);

	// set limb describing structs at the end

	BodySetup.RightHandRotation.RightDirection = BodySetup.RightHandRotation.ExternalDirection;
	BodySetup.LeftHandRotation.RightDirection = BodySetup.LeftHandRotation.ExternalDirection * -1.f;

	BodySetup.RightShoulderRotation.RightDirection = BodySetup.RightShoulderRotation.ExternalDirection;
	BodySetup.LeftShoulderRotation.RightDirection = BodySetup.LeftShoulderRotation.ExternalDirection * -1.f;

	BodySetup.RightLegRotation.RightDirection = BodySetup.RightLegRotation.ExternalDirection;
	BodySetup.LeftLegRotation.RightDirection = BodySetup.LeftLegRotation.ExternalDirection;

	// 4. Initialize additional bones arrays.

	CapturedSkeletonR.SetNum(3);
	CapturedSkeletonL.SetNum(3);

	// find additional spine bones
	FName CurrentBoneName = BodyMesh->GetParentBone(Ribcage);

	BodySetup.SpineBones.Empty();
	BodySetup.SpineBoneNames.Empty();
	while (CurrentBoneName != Pelvis)
	{
		t1 = BodyMesh->GetSocketTransform(CurrentBoneName);
		t2 = BodyMesh->GetSocketTransform(BodyMesh->GetParentBone(CurrentBoneName));
		t3 = t1.GetRelativeTransform(t2);

		BodySetup.SpineBones.Add(CurrentBoneName, t3);
		BodySetup.SpineBoneNames.Insert(CurrentBoneName, 0);

		CurrentBoneName = BodyMesh->GetParentBone(CurrentBoneName);
	}

	// ribcage bone offset
	if (BodySetup.SpineBoneNames.Num() > 0)
	{
		t1 = BodyMesh->GetSocketTransform(BodySetup.SpineBoneNames.Last());
		t2 = tRibcage;
		BodySetup.RelativeRibcageOffset = t2.GetRelativeTransform(t1);
	}
	else
	{
		BodySetup.RelativeRibcageOffset = tRibcage.GetRelativeTransform(tPelvis);
	}

	// find additional neck bones
	CurrentBoneName = BodyMesh->GetParentBone(Head);

	BodySetup.NeckBones.Empty();
	BodySetup.NeckBoneNames.Empty();
	while (CurrentBoneName != Ribcage)
	{
		t1 = BodyMesh->GetSocketTransform(CurrentBoneName);
		t2 = BodyMesh->GetSocketTransform(BodyMesh->GetParentBone(CurrentBoneName));
		t3 = t1.GetRelativeTransform(t2);

		BodySetup.NeckBones.Add(CurrentBoneName, t3);
		BodySetup.NeckBoneNames.Insert(CurrentBoneName, 0);

		CurrentBoneName = BodyMesh->GetParentBone(CurrentBoneName);
	}

	// find first pass bones
	const FReferenceSkeleton& RefSkeleton = AccessRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const int32 NumSpaceBases = BodyMesh->GetComponentSpaceTransforms().Num();

	// fore each bone
	BodySetup.FirstPassBones.SetNum(NumSpaceBases);
	/////////////////////////////////////BodySetup.FirstPassBones
	for (int32 ComponentSpaceIdx = 0; ComponentSpaceIdx < NumSpaceBases; ++ComponentSpaceIdx)
	{
		const FName BoneName = RefSkeleton.GetBoneName(ComponentSpaceIdx);
		const bool bShouldEvaluate =
			(  BoneName == RootBone
			|| BoneName == Pelvis
			|| BoneName == Ribcage
			|| BoneName == ShoulderRight
			|| BoneName == UpperarmRight
			|| BoneName == LowerarmRight
			|| BoneName == PalmRight
			|| BoneName == ShoulderLeft
			|| BoneName == UpperarmLeft
			|| BoneName == LowerarmLeft
			|| BoneName == PalmLeft
			|| BodySetup.SpineBoneNames.Contains(BoneName));

		BodySetup.FirstPassBones[ComponentSpaceIdx] = bShouldEvaluate;
	}

	// initialize bones map
	CapturedBody.Empty();
	CapturedBody.Add(RootBone);
	if (RootBone != Pelvis) CapturedBody.Add(Pelvis);

	if (CapturedBody.Contains(Ribcage)) return false; else CapturedBody.Add(Ribcage);
	if (CapturedBody.Contains(Head)) return false; else CapturedBody.Add(Head);
	if (CapturedBody.Contains(ShoulderRight)) return false; else CapturedBody.Add(ShoulderRight);
	if (CapturedBody.Contains(UpperarmRight)) return false; else CapturedBody.Add(UpperarmRight);
	if (CapturedBody.Contains(LowerarmRight)) return false; else CapturedBody.Add(LowerarmRight);
	if (CapturedBody.Contains(PalmRight)) return false; else CapturedBody.Add(PalmRight);
	if (CapturedBody.Contains(ShoulderLeft)) return false; else CapturedBody.Add(ShoulderLeft);
	if (CapturedBody.Contains(UpperarmLeft)) return false; else CapturedBody.Add(UpperarmLeft);
	if (CapturedBody.Contains(LowerarmLeft)) return false; else CapturedBody.Add(LowerarmLeft);
	if (CapturedBody.Contains(PalmLeft)) return false; else CapturedBody.Add(PalmLeft);
	if (CapturedBody.Contains(ThighRight)) return false; else CapturedBody.Add(ThighRight);
	if (CapturedBody.Contains(CalfRight)) return false; else CapturedBody.Add(CalfRight);
	if (CapturedBody.Contains(FootRight)) return false; else CapturedBody.Add(FootRight);
	if (CapturedBody.Contains(ThighLeft)) return false; else CapturedBody.Add(ThighLeft);
	if (CapturedBody.Contains(CalfLeft)) return false; else CapturedBody.Add(CalfLeft);
	if (CapturedBody.Contains(FootLeft)) return false; else CapturedBody.Add(FootLeft);

	for (const auto& b : BodySetup.NeckBones) CapturedBody.Add(b.Key);
	for (const auto& b : BodySetup.SpineBones) CapturedBody.Add(b.Key);

	// init skeletal mesh name for a snapshot pose
	BodySetup.SkeletalMeshName = __uev_GetSkeletalMesh(BodyMesh)->GetFName();

	return true;
}

void UVRIKSkeletalMeshTranslator::OverrideMeshScale(float HeightScale, float ArmScale)
{
	BodySetup.MeshScale.X = BodySetup.MeshScale.Y = ArmScale;
	BodySetup.MeshScale.Z = HeightScale;
}

void UVRIKSkeletalMeshTranslator::SetVerticalAxisRotation(FRotator& OutRot, float Value) const
{
	OutRot = BodySetup.RootBoneRotation;
	OutRot.Yaw += Value;
	/*
	float UpDir;
	EAxis::Type VertAxis;
	VertAxis = FindCoDirection(BodySetup.RootBoneRotation, FVector::UpVector, UpDir);

	FRotator r;
	switch (VertAxis)
	{
	case EAxis::Z:
		r = FRotator(0.f, Value * UpDir, 0.f);
		break;
	case EAxis::X:
		r = FRotator(0.f, Value * UpDir, 0.f);
		break;
	case EAxis::Y:
		r = FRotator(0.f, Value * UpDir, 0.f);
		break;
	}
	OutRot = UKismetMathLibrary::ComposeRotators(BodySetup.RootBoneRotation, FRotator(0.f, Value, 0.f));
	*/
}

// Player Calibration Process Complete: change skeletal mesh scale
void UVRIKSkeletalMeshTranslator::VRIKBody_OnCalibrationComplete()
{
	float RealHeight, RealWidth;

	if (VRIKSolver)
	{
		if (bRescaleMesh && VRIKSolver->IsBodyCalibrated())
		{
			RealHeight = VRIKSolver->GetClearCharacterHeight();
			RealWidth = VRIKSolver->GetClearArmSpan();
		}
		else
		{
			RealWidth = BodySetup.MeshHandsFullLength;
			RealHeight = BodySetup.MeshHeight + MeshHeadHalfHeight;
		}

		UE_LOG(LogVRIKSkeletalMeshTranslator, Log, TEXT("VRIKBody_OnCalibrationComplete: %f, %f"), RealWidth, RealHeight);

		bool bShouldReplicate = false;
		bool bIsLocal = true;
		if (GetIsReplicated())
		{
			if (APawn* PlayerActor = Cast<APawn>(GetOwner()))
			{
				bIsLocal = PlayerActor->IsLocallyControlled();
				if (bIsLocal)
				{
					bShouldReplicate = true;
				}
			}
		}

		if (bIsLocal)
		{
			Local_CalibrateComponent(RealWidth, RealHeight);
		}
		if (bShouldReplicate)
		{
			Server_CalibrateComponent(RealWidth, RealHeight);
		}
	}
}

void UVRIKSkeletalMeshTranslator::Local_CalibrateComponent(float RealWidth, float RealHeight)
{
	UE_LOG(LogVRIKSkeletalMeshTranslator, Log, TEXT("Local_CalibrateComponent"));
	if (VRIKSolver)
	{
		if (bRescaleMesh)
		{
			BodySetup.MeshScale.X = BodySetup.MeshScale.Y = (RealWidth / BodySetup.MeshHandsFullLength);
			BodySetup.MeshScale.Z = (RealHeight / (BodySetup.MeshHeight + MeshHeadHalfHeight));

			if (BodyMesh)
			{
				BodyMesh->SetRelativeScale3D(FVector::OneVector);
			}
		}
		else
		{
			BodySetup.MeshScale = FVector::OneVector;
		}

		BodySetup.fUpperarmLength = BodySetup.MeshUpperarmLength * BodySetup.MeshScale.X;
		BodySetup.fForearmLength = BodySetup.MeshForearmLength * BodySetup.MeshScale.X;

		bIsCalibrated = true;
	}
}

void UVRIKSkeletalMeshTranslator::Server_CalibrateComponent_Implementation(float RealWidth, float RealHeight)
{
	Clients_CalibrateComponent(RealWidth, RealHeight);

	if (GetIsReplicated() && bIsInitialized)
	{
		if (APawn* PlayerActor = Cast<APawn>(GetOwner()))
		{
			if (!PlayerActor->IsLocallyControlled())
			{
				Local_CalibrateComponent(RealWidth, RealHeight);
			}
		}
	}
}

void UVRIKSkeletalMeshTranslator::Clients_CalibrateComponent_Implementation(float RealWidth, float RealHeight)
{
	if (GetIsReplicated() && bIsInitialized)
	{
		if (APawn* PlayerActor = Cast<APawn>(GetOwner()))
		{
			if (!PlayerActor->IsLocallyControlled())
			{
				Local_CalibrateComponent(RealWidth, RealHeight);
			}
		}
	}
}

void UVRIKSkeletalMeshTranslator::Local_UpdateCalibration(float HeightScale, float ArmScale)
{
	if (VRIKSolver)
	{
		if (bRescaleMesh)
		{
			BodySetup.MeshScale.X = BodySetup.MeshScale.Y = ArmScale;
			BodySetup.MeshScale.Z = HeightScale;
		}
		else
		{
			BodySetup.MeshScale = FVector::OneVector;
		}

		BodySetup.fUpperarmLength = BodySetup.MeshUpperarmLength * BodySetup.MeshScale.X;
		BodySetup.fForearmLength = BodySetup.MeshForearmLength * BodySetup.MeshScale.X;

		if (BodyMesh)
		{
			BodyMesh->SetRelativeScale3D(FVector::OneVector);
		}

		bIsCalibrated = true;
	}
}

void UVRIKSkeletalMeshTranslator::RestoreSkeletalMeshSetup(const FSkeletalMeshSetup& SkeletalMeshSetup)
{
	BodySetup = SkeletalMeshSetup;
	if (VRIKSolver) bIsInitialized = true;
}

void UVRIKSkeletalMeshTranslator::RefreshComponentPosition(bool bUpdateCapturedBones)
{
	if (bIsInitialized && bIsCalibrated)
	{
		const FTransform NewPos = BodyMesh->GetComponentTransform();
		if (FVector::DistSquared(NewPos.GetTranslation(), ComponentWorldTransform.GetTranslation()) > 0.f)
		{
			bComponentWasMovedInTick = false;

			if (bUpdateCapturedBones)
			{
				FTransform DeltaTr;
				DeltaTr.SetTranslation(NewPos.GetTranslation() - ComponentWorldTransform.GetTranslation());
				DeltaTr.SetRotation(UKismetMathLibrary::NormalizedDeltaRotator(NewPos.Rotator(), ComponentWorldTransform.Rotator()).Quaternion());
				DeltaTr.SetScale3D(FVector(1.f, 1.f, 1.f));

				for (auto& bone : CapturedBody)
				{
					bone.Value = DeltaTr * bone.Value;
				}
			}

			ComponentWorldTransform = NewPos;
		}
	}
}

FTransform UVRIKSkeletalMeshTranslator::GetLastPelvisBone() const
{
	return LastPelvisBonePosition;
}

FTransform UVRIKSkeletalMeshTranslator::GetLastRibcageBone() const
{
	return LastRibcageBonePosition;
}

void UVRIKSkeletalMeshTranslator::SetChairTransform(const FTransform& ChairInWorldSpace)
{
	bHasChair = true;
	ChairTransform = ChairInWorldSpace;
}

void UVRIKSkeletalMeshTranslator::ResetChairTransform()
{
	bHasChair = false;
}

void UVRIKSkeletalMeshTranslator::UpdatePoseableMesh(UPoseableMeshComponent* TargetMeshComp, const FPoseSnapshot& PoseSnapshot)
{
	if (!TargetMeshComp || !__uev_GetSkeletalMesh(TargetMeshComp))
	{
		return;
	}

	TargetMeshComp->BoneSpaceTransforms = PoseSnapshot.LocalTransforms;
	TargetMeshComp->MarkRefreshTransformDirty();
}

void UVRIKSkeletalMeshTranslator::CalcTorsoIK()
{
	const float ArmTwistRatio = 0.5f;
	const float MaxTwistDegrees = 90.f;
	const float MaxPitchBackwardDegrees = 10.f;
	const float MaxPitchForwardDegrees = 60.f;
	const float TorsoRotationSlerpSpeed = 13.f;
	const bool bDebugDraw = false;

	FTransform Waist = CapturedBody[Pelvis];

	TArray<FTransform> PostIKTransformsLeft;
	TArray<FTransform> PostIKTransformsRight;

	FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
		CapturedSkeletonL,
		CapturedBodyRaw.HandLeft.GetTranslation(),
		PostIKTransformsLeft,
		100.f /*MaxShoulderDragDistance*/,
		1.f /*ShoulderDragStiffness*/,
		0.001f /*Precision*/,
		10 /*MaxIterations*/
	);

	FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
		CapturedSkeletonR,
		CapturedBodyRaw.HandRight.GetTranslation(),
		PostIKTransformsRight,
		100.f /*MaxShoulderDragDistance*/,
		1.f /*ShoulderDragStiffness*/,
		0.001f /*Precision*/,
		10 /*MaxIterations*/
	);

	FTransform WaistPostIK = Waist;

	// Use first pass results to twist the torso around the spine direction
	// Note --calculations here are relative to the waist bone, not root!
	// 'Neck' is the midpoint beween the shoulders
	FVector ShoulderLeftPreIK = CapturedSkeletonL[0].GetLocation() - Waist.GetLocation();
	FVector ShoulderRightPreIK = CapturedSkeletonR[0].GetLocation() - Waist.GetLocation();
	FVector NeckPreIK = (ShoulderLeftPreIK + ShoulderRightPreIK) / 2.f;
	FVector SpineDirection = NeckPreIK.GetUnsafeNormal();

	FVector ShoulderLeftPostIK = PostIKTransformsLeft[0].GetLocation() - WaistPostIK.GetLocation();
	FVector ShoulderRightPostIK = PostIKTransformsRight[0].GetLocation() - WaistPostIK.GetLocation();
	FVector NeckPostIK = (ShoulderLeftPostIK + ShoulderRightPostIK) / 2.f;
	FVector SpineDirectionPost = NeckPostIK.GetUnsafeNormal();

	if (bDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PostIKTransformsRight[0].GetLocation(), 10.f, 8, FColor::Red, false, 0.05f, 0, 1.f);
		DrawDebugSphere(GetWorld(), PostIKTransformsLeft[0].GetLocation(), 10.f, 8, FColor::Blue, false, 0.05f, 0, 1.f);
	}

	FVector ShoulderLeftPostIKDir = (ShoulderLeftPostIK - NeckPostIK).GetUnsafeNormal();
	FVector ShoulderRightPostIKDir = (ShoulderRightPostIK - NeckPostIK).GetUnsafeNormal();

	FVector ShoulderLeftPreIKDir = (ShoulderLeftPreIK - NeckPreIK).GetUnsafeNormal();
	FVector ShoulderRightPreIKDir = (ShoulderRightPreIK - NeckPreIK).GetUnsafeNormal();

	// Find the twist angle by blending the small rotation and the large one as specified
	FVector LeftTwistAxis = FVector::CrossProduct(ShoulderLeftPreIKDir, ShoulderLeftPostIKDir);
	float LeftTwistRad = 0.0f;
	if (LeftTwistAxis.Normalize())
	{
		LeftTwistRad = (FVector::DotProduct(LeftTwistAxis, SpineDirection) > 0.0f) ?
			FMath::Acos(FVector::DotProduct(ShoulderLeftPreIKDir, ShoulderLeftPostIKDir)) :
			-1 * FMath::Acos(FVector::DotProduct(ShoulderLeftPreIKDir, ShoulderLeftPostIKDir));
	}

	FVector RightTwistAxis = FVector::CrossProduct(ShoulderRightPreIKDir, ShoulderRightPostIKDir);
	float RightTwistRad = 0.0f;
	if (RightTwistAxis.Normalize())
	{
		RightTwistRad = (FVector::DotProduct(RightTwistAxis, SpineDirection) > 0.0f) ?
			FMath::Acos(FVector::DotProduct(ShoulderRightPreIKDir, ShoulderRightPostIKDir)) :
			-1 * FMath::Acos(FVector::DotProduct(ShoulderRightPreIKDir, ShoulderRightPostIKDir));
	}

	float TwistRad;
	float SmallRad;
	float LargeRad;
	if (FMath::Abs(LeftTwistRad) > FMath::Abs(RightTwistRad))
	{
		SmallRad = RightTwistRad;
		LargeRad = LeftTwistRad;
	}
	else
	{
		SmallRad = LeftTwistRad;
		LargeRad = RightTwistRad;
	}

	// If IKing both arms, blend the twists
	TwistRad = FMath::Lerp(SmallRad, LargeRad, ArmTwistRatio);

	float TwistDeg = FMath::RadiansToDegrees(TwistRad);
	TwistDeg = FMath::Clamp(TwistDeg, -MaxTwistDegrees, MaxTwistDegrees);
	FQuat TwistRotation(SpineDirection, FMath::DegreesToRadians(TwistDeg));

	const FVector RightAxis = PelvisRightVector(Waist.Rotator());
	const FVector LeftAxis = RightAxis * -1.f;
	
	// Prepare pitch (bend forward / backward) rotation
	FVector SpinePitchPreIK = FVector::VectorPlaneProject(SpineDirection, LeftAxis);
	FVector SpinePitchPostIK = FVector::VectorPlaneProject(SpineDirectionPost, LeftAxis);

	FVector PitchPositiveDirection = FVector::CrossProduct(SpinePitchPreIK, SpinePitchPostIK);
	float PitchRad = 0.0f;
	if (PitchPositiveDirection.Normalize())
	{
		PitchRad = (FVector::DotProduct(PitchPositiveDirection, RightAxis) >= 0.0f) ?
			FMath::Acos(FVector::DotProduct(SpinePitchPreIK, SpinePitchPostIK)) :
			-1 * FMath::Acos(FVector::DotProduct(SpinePitchPreIK, SpinePitchPostIK));
	}

	PitchRad = FMath::DegreesToRadians(FMath::Clamp(FMath::RadiansToDegrees(PitchRad), -MaxPitchBackwardDegrees, MaxPitchForwardDegrees));
	FQuat PitchRotation(RightAxis, PitchRad);

	// Twist needs to be applied first; pitch will modify twist axes and cause a bad rotation
	FQuat TargetOffset = (PitchRotation * TwistRotation);
	// Interpolate rotation
	LastRotationOffset = FQuat::Slerp(LastRotationOffset, TargetOffset, TorsoRotationSlerpSpeed * GetWorld()->DeltaTimeSeconds);

	FQuat RibcageRot = (LastRotationOffset * Waist.GetRotation()).GetNormalized();
	// Apply new transforms
	Waist.SetRotation(RibcageRot);
	Waist.SetTranslation((PostIKTransformsLeft[0].GetLocation() + PostIKTransformsRight[0].GetLocation()) * 0.5f);

	if (bDebugDraw)
	{
		DrawDebugSphere(GetWorld(), Waist.GetTranslation(), 30.f, 5, FColor::Red, false, 0.05f, 0, 0.5f);
	}

	// Calculate bezier spine
	TArray<FTransform> FixedSpineBones;
	const int32 Num = BodySetup.SpineBones.Num();
	FixedSpineBones.SetNum(Num);
	CalculateSpine(CapturedBody[Pelvis], Waist, BodySetup.SpineBoneNames, BodySetup.SpineBones, FixedSpineBones, bDrawSpineDebug, true);

	// update CapturedBody map
	const FTransform SequenceStart = CapturedBody[Pelvis];
	for (int i = 0; i < Num; i++)
	{
		CapturedBody[BodySetup.SpineBoneNames[i]] = FixedSpineBones[i];
	}

	// convert bone rotation from pelvis to ribcage
	const FVector RibcageForward = PelvisForwardVector(Waist.Rotator());
	const FVector RibcageUp = PelvisUpVector(Waist.Rotator());
	
	Waist.SetRotation
	(
		MakeRotByTwoAxes(BodySetup.RibcageRotation.VerticalAxis, RibcageForward * BodySetup.RibcageRotation.UpDirection, BodySetup.RibcageRotation.ForwardAxis, RibcageUp * BodySetup.RibcageRotation.ForwardDirection).Quaternion()
	);

	// apply to ribcage
	CapturedBody[Ribcage] = Waist;
}

// Calculate spine bones between pelvis and ribcage by cubic bezier curve
// The coefficient of length variable (SpineBendingMultiplier = .2f) is empirical.
void UVRIKSkeletalMeshTranslator::CalculateSpine(const FTransform& Start, FTransform& End, const TArray<FName>& RelativeBoneNames, const TMap<FName, FTransform>& RelativeTransforms, TArray<FTransform>& ResultTransforms, bool bDrawDebugGeometry, bool SnapToEnd)
{
	FVector Points[4];
	TArray<FVector> ResultPoints;
	
	// start and end
	Points[0] = Start.GetTranslation();
	Points[3] = End.GetTranslation();

	// direction points
	const float length = FVector::Dist(Points[0], Points[3]) * SpineBendingMultiplier;

	const FVector dir = (Points[3] - Points[0]).GetSafeNormal();
	const FVector UpDir = FMath::Lerp<FVector>(PelvisUpVector(Start.Rotator()), dir, 0.6f).GetSafeNormal();
	const FVector DownDir = FMath::Lerp<FVector>(-PelvisUpVector(End.Rotator()), -dir, 0.6f).GetSafeNormal();


	Points[1] = Start.GetTranslation() + UpDir * length;
	Points[2] = End.GetTranslation() + DownDir * length;

	// number of points to calculate (including start and end)
	int BonesNum = ResultTransforms.Num() + 2;
	FVector::EvaluateBezier(Points, BonesNum, ResultPoints);

	if (bDrawDebugGeometry)
	{
		for (const auto& Point : ResultPoints)
		{
			//void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
			DrawDebugSphere(GetWorld(), Point, 4.f, 4, FColor::Yellow, false, 0.1f, 0, 1.f);
		}
		DrawDebugLine(GetWorld(), Start.GetTranslation(), Start.GetTranslation() + PelvisForwardVector(Start.Rotator()) * 100.f, FColor::Red, false, 0.1f, 0, 1.f);
		DrawDebugLine(GetWorld(), End.GetTranslation(), End.GetTranslation() + PelvisForwardVector(End.Rotator()) * 100.f, FColor::Red, false, 0.1f, 0, 1.f);
	}

	for (int i = 0; i < BonesNum - 2; i++)
	{
		FVector DirectionY;
		const FTransform NewRelativeOffset = RelativeTransforms[RelativeBoneNames[i]];

		if (i == 0)
		{
			ResultTransforms[i] = NewRelativeOffset * Start;
			DirectionY = PelvisRightVector(Start.Rotator());
		}
		else
		{
			ResultTransforms[i] = NewRelativeOffset * ResultTransforms[i - 1];
			DirectionY = PelvisRightVector(ResultTransforms[i].Rotator());
		}

		const FVector DirectionX = (ResultPoints[i + 2] - ResultPoints[i + 1]).GetSafeNormal();

		FRotator r = MakeRotByTwoAxes(BodySetup.PelvisRotation.ForwardAxis, DirectionX * BodySetup.PelvisRotation.ForwardDirection,
											BodySetup.PelvisRotation.HorizontalAxis, DirectionY * BodySetup.PelvisRotation.RightDirection);

		if (ManualSpineCorrection.IsValidIndex(i))
		{
			r = UKismetMathLibrary::ComposeRotators(r, ManualSpineCorrection[i]);
		}

		if (bDrawSpineDebug)
		{
			DrawDebugLine(GetWorld(), ResultTransforms[i].GetTranslation(), ResultTransforms[i].GetTranslation() + PelvisUpVector(r) * 5.f, FColor::Yellow, false, 0.1f, 0, 1.f);
		}

		ResultTransforms[i].SetRotation(r.Quaternion());
	}

	if (SnapToEnd)
	{
		FVector Delta = End.GetTranslation() - ResultTransforms.Last().GetTranslation();
		for (auto& Tr : ResultTransforms)
		{
			Tr.AddToTranslation(Delta);
		}
	}

	End.SetTranslation((BodySetup.RelativeRibcageOffset * ResultTransforms.Last()).GetTranslation());
}

// Function returns (updates) FPoseSnapshot struct used to animate skeletal mesh component in Anim Blueprint
// Use ReturnValue as an input parameter in PoseSnapshot Animation Node.
// Twist bones must be updated manually.
bool UVRIKSkeletalMeshTranslator::GetLastPose(FPoseSnapshot& ReturnPoseValue)
{
	static bool bWarning = false;

	_CHECKTRIAL;

	FVector RuntimeBodyScale = BodySetup.MeshScale;
	RuntimeBodyScale.X *= ScaleMultiplierHorizontal;
	RuntimeBodyScale.Y *= ScaleMultiplierHorizontal;
	RuntimeBodyScale.Z *= ScaleMultiplierVertical;

	// can't return PoseSnapshot if skeletal mesh isn't initialized
	if (!bIsInitialized || !bIsCalibrated)
	{
		ReturnPoseValue.bIsValid = false;
		if (!bIsCalibrated && !bWarning)
		{
			UE_LOG(LogVRIKSkeletalMeshTranslator, Log, TEXT("[GetLastPose]: Component isn't calibrated."));
			bWarning = true;
		}

		return false;
	}

	if (!VRIKSolver->IsInitialized())
	{
		ReturnPoseValue.bIsValid = false;
		return false;
	}

	if (bRootMotion && bComponentWasMovedInTick)
	{
		BodyMesh->SetRelativeLocationAndRotation(ComponentActorTransform.GetTranslation(), ComponentActorTransform.GetRotation());
	}

	// References to skeleton data
	const TArray<FTransform>& ComponentSpaceTMs = BodyMesh->GetComponentSpaceTransforms();
	const FReferenceSkeleton& RefSkeleton = AccessRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();
	const int32 NumSpaceBases = ComponentSpaceTMs.Num();

	// Return false if don't have a valid mesh
	if (NumSpaceBases == 0)
	{
		ReturnPoseValue.bIsValid = false;
		return false;
	}

	// initialize FPoseSnapshot struct
	ReturnPoseValue.SkeletalMeshName = BodySetup.SkeletalMeshName;
	ReturnPoseValue.bIsValid = true;
	// update arrays size
	ReturnPoseValue.LocalTransforms.Reset(NumSpaceBases);
	ReturnPoseValue.LocalTransforms.AddUninitialized(NumSpaceBases);
	ReturnPoseValue.BoneNames.Reset(NumSpaceBases);
	ReturnPoseValue.BoneNames.AddUninitialized(NumSpaceBases);

	// get component transform
	FTransform tr_component = BodyMesh->GetComponentTransform();
	FTransform tr_bone, tr_parent;

	const FVector RootBoneScale = RefPoseSpaceBaseTMs[0].GetScale3D();
	tr_component.SetScale3D(FVector::OneVector);
	// set transform of root bone
	if (bHasRootBone)
	{
		ReturnPoseValue.BoneNames[0] = RootBone;
		ReturnPoseValue.LocalTransforms[0] = FTransform(BodySetup.RootBoneRotation) * CapturedBody[RootBone].GetRelativeTransform(tr_component);
		ReturnPoseValue.LocalTransforms[0].SetScale3D(RootBoneScale);
	}
	else
	{
		ReturnPoseValue.BoneNames[0] = Pelvis;
		ReturnPoseValue.LocalTransforms[0] = CapturedBody[Pelvis].GetRelativeTransform(tr_component);

		const FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z)
			* RefPoseSpaceBaseTMs[0].GetScale3D();
		ReturnPoseValue.LocalTransforms[0].SetScale3D(vPelvis);
	}

	int32 PelvisBoneIndex = INDEX_NONE, RibcageBoneIndex = INDEX_NONE;

	/*********************************************************************
	*** FIRST PASS: calculate major bones for torso IK                 ***
	**********************************************************************/
	for (int32 ComponentSpaceIdx = 1; ComponentSpaceIdx < NumSpaceBases; ++ComponentSpaceIdx)
	{
		// bone name
		const FName BoneName = RefSkeleton.GetBoneName(ComponentSpaceIdx);
		ReturnPoseValue.BoneNames[ComponentSpaceIdx] = BoneName;

		if (BoneName == Pelvis)
		{
			PelvisBoneIndex = ComponentSpaceIdx;
		}
		else if (BoneName == Ribcage)
		{
			RibcageBoneIndex = ComponentSpaceIdx;
		}

		// only calculate first pass bones (for optimisation)
		if (!BodySetup.FirstPassBones[ComponentSpaceIdx])
		{
			ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
			continue;
		}

		// parent bone index
		const int32 ParentIndex = RefSkeleton.GetParentIndex(ComponentSpaceIdx);

		// get bone transform if bone is evaluated in a UCaptureDevice Component		
		bool bBoneHasEvaluated = CapturedBody.Contains(BoneName);
		if (bBoneHasEvaluated) tr_bone = CapturedBody[BoneName];

		if (bHandsIKLateUpdate)
		{
			// restore hand IK after scaling
			if (BoneName == UpperarmRight)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
				tr_bone = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, ComponentSpaceIdx);

				CalculateTwoBoneIK(
					tr_bone.GetTranslation(),
					CapturedBodyRaw.HandRight.GetTranslation(),
					CapturedBodyRaw.ElbowJointTargetRight,
					BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
					CapturedBody[UpperarmRight], CapturedBody[LowerarmRight],
					BodySetup.RightHandRotation, bInvertElbows, true);

				tr_bone = CapturedBody[BoneName];

				CapturedSkeletonR[0] = CapturedBody[UpperarmRight];
				CapturedSkeletonR[1] = CapturedBody[LowerarmRight];
				CapturedSkeletonR[2] = CapturedBodyRaw.HandRight;
			}
			else if (BoneName == UpperarmLeft)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
				tr_bone = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, ComponentSpaceIdx);

				CalculateTwoBoneIK(
					tr_bone.GetTranslation(),
					CapturedBodyRaw.HandLeft.GetTranslation(),
					CapturedBodyRaw.ElbowJointTargetLeft,
					BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
					CapturedBody[UpperarmLeft], CapturedBody[LowerarmLeft],
					BodySetup.LeftHandRotation, bInvertElbows, true);

				tr_bone = CapturedBody[BoneName];

				CapturedSkeletonL[0] = CapturedBody[UpperarmLeft];
				CapturedSkeletonL[1] = CapturedBody[LowerarmLeft];
				CapturedSkeletonL[2] = CapturedBodyRaw.HandLeft;
			}
		}

		if (bBoneHasEvaluated && tr_bone.Rotator() != FRotator::ZeroRotator)
		{
			// get parent bone transform
			if (RefSkeleton.IsValidIndex(ParentIndex))
			{
				const FName ParentBoneName = RefSkeleton.GetBoneName(ParentIndex);
				const FTransform* ParentTransform = CapturedBody.Find(ParentBoneName);
				if (ParentTransform)
				{
					tr_parent = *ParentTransform;
				}
				else
				{
					// if parent bone isn't evaluated 
					// look for grandpa bone
					const int32 GrandpaIndex = RefSkeleton.GetParentIndex(ParentIndex);
					if (RefSkeleton.IsValidIndex(GrandpaIndex))
					{
						const FName GrandpaBoneName = RefSkeleton.GetBoneName(GrandpaIndex);
						const FTransform* GrandpaTransform = CapturedBody.Find(GrandpaBoneName);
						if (GrandpaTransform)
							tr_parent = RefPoseSpaceBaseTMs[ParentIndex] * (*GrandpaTransform);
						else
							tr_parent = tr_component;
					}
					else tr_parent = tr_component;
				}
			}
			else
			{
				tr_parent = tr_component;
			}

			// calc bone transform relative to parent bone
			ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = tr_bone.GetRelativeTransform(tr_parent);
			if (BoneName != Pelvis)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetTranslation(RefPoseSpaceBaseTMs[ComponentSpaceIdx].GetTranslation());
			}
			else if (BoneName == Pelvis)
			{
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				if (!bHasRootBone)
				{
					ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].GetRelativeTransform(tr_component);
				}
				else
				{
					tr_parent = ReturnPoseValue.LocalTransforms[0] * tr_component;
					tr_parent.SetScale3D(RootBoneScale);
					ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = tr_bone.GetRelativeTransform(tr_parent);
				}
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vPelvis);
			}
			else if (!bHandScaleFromUpperbone && (BoneName == ShoulderRight || BoneName == ShoulderLeft))
			{
				FVector vHand = FVector(RuntimeBodyScale.X, RuntimeBodyScale.X, RuntimeBodyScale.X);
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				vHand.X /= vPelvis.X; vHand.Y /= vPelvis.Y; vHand.Z /= vPelvis.Z;
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vHand);
			}
			else if (bHandScaleFromUpperbone && (BoneName == UpperarmRight || BoneName == UpperarmLeft))
			{
				FVector vHand = FVector(RuntimeBodyScale.X, RuntimeBodyScale.X, RuntimeBodyScale.X);
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				vHand.X /= vPelvis.X; vHand.Y /= vPelvis.Y; vHand.Z /= vPelvis.Z;
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vHand);
			}
			else
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(RefPoseSpaceBaseTMs[ComponentSpaceIdx].GetScale3D());
			}
		}
		else if (RefPoseSpaceBaseTMs.IsValidIndex(ComponentSpaceIdx))
		{
			ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
		}
	}

	// Calculate spine
	if (!bDisableFlexibleTorso)
	{
		CalcTorsoIK();
	}

	/*********************************************************************
	*** SECOND PASS: calculate all bones                               ***
	**********************************************************************/

	// fore each bone
	for (int32 ComponentSpaceIdx = 1; ComponentSpaceIdx < NumSpaceBases; ++ComponentSpaceIdx)
	{
		// bone name
		const FName BoneName = RefSkeleton.GetBoneName(ComponentSpaceIdx);
		ReturnPoseValue.BoneNames[ComponentSpaceIdx] = BoneName;

		if (BoneName == Pelvis)
		{
			continue;
		}

		// parent bone index
		const int32 ParentIndex = RefSkeleton.GetParentIndex(ComponentSpaceIdx);

		// get bone transform if bone is evaluated in a UCaptureDevice Component	
		FTransform* tr_bone_ptr = CapturedBody.Find(BoneName);
		bool bBoneHasEvaluated;
		if (tr_bone_ptr)
		{
			bBoneHasEvaluated = true; tr_bone = *tr_bone_ptr;
		}
		else bBoneHasEvaluated = false;

		if (bHandsIKLateUpdate)
		{
			// restore hand IK after scaling
			if (BoneName == UpperarmRight)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
				tr_bone = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, ComponentSpaceIdx);

				CalculateTwoBoneIK(
					tr_bone.GetTranslation(),
					CapturedBodyRaw.HandRight.GetTranslation(),
					CapturedBodyRaw.ElbowJointTargetRight,
					BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
					CapturedBody[UpperarmRight], CapturedBody[LowerarmRight],
					BodySetup.RightHandRotation, bInvertElbows);

				tr_bone = CapturedBody[BoneName];
			}
			else if (BoneName == UpperarmLeft)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
				tr_bone = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, ComponentSpaceIdx);

				CalculateTwoBoneIK(
					tr_bone.GetTranslation(),
					CapturedBodyRaw.HandLeft.GetTranslation(),
					CapturedBodyRaw.ElbowJointTargetLeft,
					BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
					CapturedBody[UpperarmLeft], CapturedBody[LowerarmLeft],
					BodySetup.LeftHandRotation, bInvertElbows);

				tr_bone = CapturedBody[BoneName];
			}
		}

		// use reference positions for leg bones if legs IK is disabled
		if (!VRIKSolver->ComputeLegsIK && (BoneName == ThighRight ||
										   BoneName == ThighLeft ||
										   BoneName == CalfRight ||
										   BoneName == CalfLeft ||
										   BoneName == FootRight ||
										   BoneName == FootLeft))
		{
			bBoneHasEvaluated = false;
		}
		
		if (bBoneHasEvaluated && tr_bone.Rotator() != FRotator::ZeroRotator)
		{
			// get parent bone transform
			if (RefSkeleton.IsValidIndex(ParentIndex))
			{
				tr_parent = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, ParentIndex);
			}
			else
			{
				tr_parent = tr_component;
			}

			// calc bone transform relative to parent bone
			ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = tr_bone.GetRelativeTransform(tr_parent);
			// Reset translation: forward hierarchy of twist bones cause wrong calculation.
			// It isn't necessary for the most common skeletons including default UE4 Mannequin.
			// The next line can be commented for this skeletons.
			if (BoneName != Pelvis)
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetTranslation(RefPoseSpaceBaseTMs[ComponentSpaceIdx].GetTranslation());
			}

			if (BoneName == Pelvis)
			{
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				if (!bHasRootBone)
				{
					ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].GetRelativeTransform(tr_component);
				}

				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vPelvis);
			}
			else if (!bHandScaleFromUpperbone && (BoneName == ShoulderRight || BoneName == ShoulderLeft))
			{
				FVector vHand = FVector(RuntimeBodyScale.X, RuntimeBodyScale.X, RuntimeBodyScale.X);
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				vHand.X /= vPelvis.X; vHand.Y /= vPelvis.Y; vHand.Z /= vPelvis.Z;
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vHand);
			}
			else if (bHandScaleFromUpperbone && (BoneName == UpperarmRight || BoneName == UpperarmLeft))
			{
				FVector vHand = FVector(RuntimeBodyScale.X, RuntimeBodyScale.X, RuntimeBodyScale.X);
				FVector vPelvis = FVector(RuntimeBodyScale.Z, RuntimeBodyScale.Z, RuntimeBodyScale.Z);
				vHand.X /= vPelvis.X; vHand.Y /= vPelvis.Y; vHand.Z /= vPelvis.Z;
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(vHand);
			}
			else
			{
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetScale3D(RefPoseSpaceBaseTMs[ComponentSpaceIdx].GetScale3D());
			}
		}
		else if (RefPoseSpaceBaseTMs.IsValidIndex(ComponentSpaceIdx))
		{
			// check and update twist
			float* TwistMultiplier = LowerarmTwistsRight.Find(BoneName);
			float TwistValue = 0.f;
			bool bIsRightTwist = true;
			if (TwistMultiplier)
			{
				TwistValue = LowerarmTwistRight;
			}
			else
			{
				TwistMultiplier = UpperarmTwistsRight.Find(BoneName);
				if (TwistMultiplier)
				{
					TwistValue = UpperarmTwistRight;
				}
				else
				{
					bIsRightTwist = false;

					TwistMultiplier = LowerarmTwistsLeft.Find(BoneName);
					if (TwistMultiplier)
					{
						TwistValue = LowerarmTwistLeft;
					}
					else
					{
						TwistMultiplier = UpperarmTwistsLeft.Find(BoneName);
						if (TwistMultiplier)
						{
							TwistValue = UpperarmTwistLeft;
						}
					}
				}
			}

			// is twist bone
			if (TwistMultiplier != nullptr)
			{
				EAxis::Type RotAxis = bIsRightTwist ? BodySetup.RightHandRotation.ForwardAxis : BodySetup.LeftHandRotation.ForwardAxis;

				FRotator NewRot = RefPoseSpaceBaseTMs[ComponentSpaceIdx].Rotator();
				const float TwistRoll = FMath::Clamp(TwistValue * *TwistMultiplier, -HandTwistLimit, HandTwistLimit);
				switch (RotAxis)
				{
					case EAxis::Type::X: NewRot.Roll += TwistRoll; break;
					case EAxis::Type::Y: NewRot.Pitch += TwistRoll; break;
					case EAxis::Type::Z: NewRot.Yaw += TwistRoll; break;
				}

				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx].SetRotation(NewRot.Quaternion());
			}
			else
			{
				// use default transform
				ReturnPoseValue.LocalTransforms[ComponentSpaceIdx] = RefPoseSpaceBaseTMs[ComponentSpaceIdx];
			}
		}
	}

	LastPelvisBonePosition = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, PelvisBoneIndex);
	LastRibcageBonePosition = RestoreBoneTransformInWS(ReturnPoseValue, RefSkeleton, RibcageBoneIndex);

	// convert to component space
	LastPelvisBonePosition = LastPelvisBonePosition.GetRelativeTransform(BodyMesh->GetComponentTransform());
	LastRibcageBonePosition = LastRibcageBonePosition.GetRelativeTransform(BodyMesh->GetComponentTransform());

	return bIsInitialized;
}

FORCEINLINE FTransform UVRIKSkeletalMeshTranslator::RestoreBoneTransformInWS(const FPoseSnapshot& CurrentPose, const FReferenceSkeleton& RefSkeleton, const int32 CurrentBoneIndex)
{
	// calculate world transform transform
	int32 TransformIndex = CurrentBoneIndex;
	int32 ParentIndex = RefSkeleton.GetParentIndex(TransformIndex);
	FTransform tr_bone = CurrentPose.LocalTransforms[CurrentBoneIndex];

	if (BodyMesh->GetAttachParent())
	{
		ComponentWorldTransform = ComponentActorTransform * BodyMesh->GetAttachParent()->GetComponentTransform();
	}

	// stop on pelvis
	while (ParentIndex != INDEX_NONE)
	{
		TransformIndex = ParentIndex;

		tr_bone = tr_bone * CurrentPose.LocalTransforms[TransformIndex];
		tr_bone.NormalizeRotation();

		ParentIndex = RefSkeleton.GetParentIndex(TransformIndex);
	}
	// add component space
	tr_bone.NormalizeRotation();
	if (bRootMotion)
	{
		tr_bone = tr_bone * ComponentWorldTransform;
	}
	else
	{

	}

	return tr_bone;
}

// Helper function to convert logical bone orientation from VRIKBody to custom skeleton bone orientation
FORCEINLINE FRotator UVRIKSkeletalMeshTranslator::BuildTransformFromAxes(const FRotator& SourceRotator, EAxis::Type XAxis, float ForwardDirection, EAxis::Type YAxis, float RightDirection, EAxis::Type ZAxis, float UpDirection)
{
	FVector x, y, z;
	FQuat q = SourceRotator.Quaternion();

	if (XAxis == EAxis::X) x = q.GetForwardVector() * ForwardDirection;
	else if (XAxis == EAxis::Y) y = q.GetForwardVector() * ForwardDirection;
	else if (XAxis == EAxis::Z) z = q.GetForwardVector() * ForwardDirection;

	if (YAxis == EAxis::X) x = q.GetRightVector() * RightDirection;
	else if (YAxis == EAxis::Y) y = q.GetRightVector() * RightDirection;
	else if (YAxis == EAxis::Z) z = q.GetRightVector() * RightDirection;

	if (ZAxis == EAxis::X) x = q.GetUpVector() * UpDirection;
	else if (ZAxis == EAxis::Y) y = q.GetUpVector() * UpDirection;
	else if (ZAxis == EAxis::Z) z = q.GetUpVector() * UpDirection;

	return UKismetMathLibrary::MakeRotFromXY(x, y);
}

// Debug function to print specified bone orientation info
FString UVRIKSkeletalMeshTranslator::PrintBoneInfo(const FBoneRotatorSetup& BoneSetup)
{
	FString s;
	TMap<EAxis::Type, FString> Axis;
	Axis.Add(EAxis::X, "X"); Axis.Add(EAxis::Y, "Y"); Axis.Add(EAxis::Z, "Z");

	FString koef1;

	if (BoneSetup.ForwardAxis == EAxis::None) return TEXT("");

	koef1 = (BoneSetup.ForwardDirection > 0.f ? "+" : "-");
	if (BoneSetup.ForwardDirection == 0) koef1 = "-error";
	s = "Forward Axis: " + Axis[BoneSetup.ForwardAxis] + koef1 + "\n";

	koef1 = (BoneSetup.RightDirection > 0.f ? "+" : "-");
	if (BoneSetup.RightDirection == 0) koef1 = "-error";
	s += "Right Axis: " + Axis[BoneSetup.HorizontalAxis] + koef1 + "\n";

	koef1 = (BoneSetup.UpDirection > 0.f ? "+" : "-");
	if (BoneSetup.UpDirection == 0) koef1 = "-error";
	s += "Up Axis: " + Axis[BoneSetup.VerticalAxis] + koef1;

	return s;
}

FORCEINLINE float UVRIKSkeletalMeshTranslator::GetLateUpperarmTwist(const FTransform& Collarbone, const FTransform& Upperarm, float CurrentValue, bool bSide)
{
	FBoneRotatorSetup ShoulderSetup, UpperarmSetup;
	if (bSide)
	{
		ShoulderSetup = BodySetup.RightShoulderRotation;
		UpperarmSetup = BodySetup.RightHandRotation;
	}
	else
	{
		ShoulderSetup = BodySetup.LeftShoulderRotation;
		UpperarmSetup = BodySetup.LeftHandRotation;
	}

	FVector XVec, YVec, YVecShoulder;
	FRotator UpperarmRot, ShoulderRot;
	FRotator TwistDelta;
	XVec = FRotationMatrix(Upperarm.Rotator()).GetScaledAxis(UpperarmSetup.ForwardAxis) * UpperarmSetup.ForwardDirection;
	YVec = FRotationMatrix(Upperarm.Rotator()).GetScaledAxis(UpperarmSetup.VerticalAxis) * UpperarmSetup.UpDirection;
	YVecShoulder = FRotationMatrix(Collarbone.Rotator()).GetScaledAxis(ShoulderSetup.VerticalAxis) * ShoulderSetup.UpDirection;

	float TwistDP = FMath::Abs(FVector::DotProduct(YVecShoulder, YVec));
	if (TwistDP > 0.2f && TwistDP < 0.8f)
	{
		UpperarmRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVec);
		ShoulderRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVecShoulder);
		TwistDelta = UKismetMathLibrary::NormalizedDeltaRotator(UpperarmRot, ShoulderRot);

		if (FMath::Abs(CurrentValue) < HandTwistLimit || FMath::Abs(TwistDelta.Roll) < HandTwistLimit || TwistDelta.Roll * CurrentValue > 0.f)
		{
			return TwistDelta.Roll;
		}
	}

	return CurrentValue;
}

/*
Function to
1) Load bones from VRIKBody
2) Apply Reorientation
3) Calculate intermediate bones (spine, neck)
*/
FORCEINLINE bool UVRIKSkeletalMeshTranslator::UpdateBonesState()
{
	FVector v0;
	FRotator r0, r1;
	FTransform t0;

	// The first goal is to restore all bones rotation in skeleton rotation axis
	// The second goal - get root bone location in world space corresponding both to real head location and to skeletal mesh head bone
	// Head bone location has a priority

	if (!VRIKSolver || !VRIKSolver->IsInitialized())
	{
		return false;
	}

	FVector RuntimeBodyScale = BodySetup.MeshScale;
	RuntimeBodyScale.X *= ScaleMultiplierHorizontal;
	RuntimeBodyScale.Y *= ScaleMultiplierHorizontal;
	RuntimeBodyScale.Z *= ScaleMultiplierVertical;

	VRIKSolver->GetRefLastFrameData(CapturedBodyRaw);

	const FTransform OriginalPelvisTr = CapturedBodyRaw.Pelvis;
	if (bHasChair)
	{
		CapturedBodyRaw.Pelvis = ChairTransform;

		FVector Delta = CapturedBodyRaw.Ribcage.GetTranslation() - CapturedBodyRaw.Pelvis.GetTranslation();
		if (Delta.Size() > BodySetup.SpineLength * RuntimeBodyScale.Z)
		{
			FVector PelvisDelta = Delta.GetSafeNormal();
			PelvisDelta *= (Delta.Size() - BodySetup.SpineLength * RuntimeBodyScale.Z);
			CapturedBodyRaw.Pelvis.AddToTranslation(PelvisDelta);
		}
	}

	PelvisRotator_WS = CapturedBodyRaw.Pelvis.Rotator();

	// twists (2018/06/28)
	LowerarmTwistRight = CapturedBodyRaw.LowerarmTwistRight;
	LowerarmTwistLeft = CapturedBodyRaw.LowerarmTwistLeft;

	const FTransform RefComponent = VRIKSolver->GetRefComponent()->GetComponentTransform();

	const FVector Velocity = VRIKSolver->GetCharacterVelocity();
	if (Velocity.SizeSquared() > 20.f)
	{
		const FVector VelocityProj = RefComponent.GetRotation().UnrotateVector(Velocity);
		WalkingSpeed = VelocityProj.Size2D();
		FRotator WalkingDirRot = UKismetMathLibrary::MakeRotFromXZ(Velocity, RefComponent.GetRotation().GetUpVector());
		WalkingYaw = UKismetMathLibrary::NormalizedDeltaRotator(WalkingDirRot, PelvisRotator_WS).Yaw;
	}
	else
	{
		WalkingSpeed = WalkingYaw = 0.f;
	}

	FVector XVec, YVec, YVecShoulder;
	FRotator UpperarmRot, ShoulderRot;
	FRotator TwistDelta;
	XVec = (CapturedBodyRaw.ForearmRight.GetTranslation() - CapturedBodyRaw.UpperarmRight.GetTranslation()).GetSafeNormal();
	YVec = CapturedBodyRaw.UpperarmRight.GetRotation().GetRightVector() * -1.f;
	YVecShoulder = CapturedBodyRaw.Ribcage.GetRotation().GetForwardVector();

	float TwistDP = FVector::DotProduct(YVecShoulder, YVec);
	if (FMath::Abs(TwistDP) > 0.1f)
	{
		UpperarmRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVec);
		ShoulderRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVecShoulder);
		TwistDelta = UKismetMathLibrary::NormalizedDeltaRotator(UpperarmRot, ShoulderRot);

		if (FMath::Abs(UpperarmTwistRight) < HandTwistLimit || FMath::Abs(TwistDelta.Roll) < HandTwistLimit || TwistDelta.Roll * UpperarmTwistRight > 0.f)
		{
			UpperarmTwistRight = TwistDelta.Roll;
		}
	}

	XVec = (CapturedBodyRaw.ForearmLeft.GetTranslation() - CapturedBodyRaw.UpperarmLeft.GetTranslation()).GetSafeNormal();
	YVec = CapturedBodyRaw.UpperarmLeft.GetRotation().GetRightVector();

	TwistDP = FVector::DotProduct(YVecShoulder, YVec);
	if (FMath::Abs(TwistDP) > 0.1f)
	{
		UpperarmRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVec);
		ShoulderRot = UKismetMathLibrary::MakeRotFromXY(XVec, YVecShoulder);
		TwistDelta = UKismetMathLibrary::NormalizedDeltaRotator(UpperarmRot, ShoulderRot);

		if (FMath::Abs(UpperarmTwistLeft) < HandTwistLimit || FMath::Abs(TwistDelta.Roll) < HandTwistLimit || TwistDelta.Roll * UpperarmTwistLeft > 0.f)
		{
			UpperarmTwistLeft = TwistDelta.Roll;
		}
	}

	// Head
	r0 = CapturedBodyRaw.Head.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.HeadRotation.VerticalAxis, BodySetup.HeadRotation.UpDirection,
		BodySetup.HeadRotation.HorizontalAxis, BodySetup.HeadRotation.RightDirection,
		BodySetup.HeadRotation.ForwardAxis, BodySetup.HeadRotation.ForwardDirection);
	CapturedBody[Head].SetTranslation(CapturedBodyRaw.Head.GetTranslation());
	CapturedBody[Head].SetRotation(r1.Quaternion());

	// Ribcage
	r0 = CapturedBodyRaw.Ribcage.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.RibcageRotation.VerticalAxis, BodySetup.RibcageRotation.UpDirection,
		BodySetup.RibcageRotation.HorizontalAxis, BodySetup.RibcageRotation.RightDirection,
		BodySetup.RibcageRotation.ForwardAxis, BodySetup.RibcageRotation.ForwardDirection);
	CapturedBody[Ribcage].SetTranslation(CapturedBodyRaw.Ribcage.GetTranslation());
	CapturedBody[Ribcage].SetRotation(r1.Quaternion());

	// Pelvis
	const FVector PelvisUpDirection = (CapturedBody[Ribcage].GetTranslation() - CapturedBody[Pelvis].GetTranslation()).GetSafeNormal();
	const float SpineOffset = (VRIKSolver->SpineLength - BodySetup.SpineLength * RuntimeBodyScale.Z) - 17.f + SpineVerticalOffset;

	r0 = CapturedBodyRaw.Pelvis.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.PelvisRotation.VerticalAxis, BodySetup.PelvisRotation.UpDirection,
		BodySetup.PelvisRotation.HorizontalAxis, BodySetup.PelvisRotation.RightDirection,
		BodySetup.PelvisRotation.ForwardAxis, BodySetup.PelvisRotation.ForwardDirection);
	CapturedBody[Pelvis].SetTranslation(CapturedBodyRaw.Pelvis.GetTranslation() + PelvisUpDirection * SpineOffset);
	CapturedBody[Pelvis].SetRotation(r1.Quaternion());

	// SequenceEnd is head transform with rotation adjusted to ribcage to calculate intermediate bones
	FTransform SequenceStart = CapturedBody[Ribcage];
	FTransform SequenceEnd;
	r0 = CapturedBodyRaw.Head.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.RibcageRotation.VerticalAxis, BodySetup.RibcageRotation.UpDirection,
		BodySetup.RibcageRotation.HorizontalAxis, BodySetup.RibcageRotation.RightDirection,
		BodySetup.RibcageRotation.ForwardAxis, BodySetup.RibcageRotation.ForwardDirection);
	SequenceEnd.SetTranslation(CapturedBodyRaw.Head.GetTranslation());
	SequenceEnd.SetRotation(r1.Quaternion());

	// Neck bones rotation + transform restore
	int Num = BodySetup.NeckBoneNames.Num();
	TArray<FTransform> NeckBoneTrs;
	NeckBoneTrs.SetNum(BodySetup.NeckBoneNames.Num());
	CalculateSpine(SequenceStart, SequenceEnd, BodySetup.NeckBoneNames, BodySetup.NeckBones, NeckBoneTrs, bDrawSpineDebug);

	for (int i = 0; i < Num; i++)
	{
		const FName BoneName = BodySetup.NeckBoneNames[i];
		CapturedBody[BoneName].SetRotation(NeckBoneTrs[i].GetRotation());
	}

	if (bDrawSpineDebug)
	{
		DrawDebugSphere(GetWorld(), SequenceStart.GetTranslation(), 3.f, 3, FColor::White, false, 0.05f, 0, 0.5f);
		DrawDebugSphere(GetWorld(), SequenceEnd.GetTranslation(), 3.f, 3, FColor::White, false, 0.05f, 0, 0.5f);
	}

	// And now the same sequence for spine bones
	// SequenceStart is a pelvis, SequenceEnd is a ribcage rotated in pelvis axes.
	SequenceStart = CapturedBody[Pelvis];
	r0 = CapturedBodyRaw.Ribcage.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.PelvisRotation.VerticalAxis, BodySetup.PelvisRotation.UpDirection,
		BodySetup.PelvisRotation.HorizontalAxis, BodySetup.PelvisRotation.RightDirection,
		BodySetup.PelvisRotation.ForwardAxis, BodySetup.PelvisRotation.ForwardDirection);
	SequenceEnd.SetTranslation(CapturedBodyRaw.Ribcage.GetTranslation());
	SequenceEnd.SetRotation(r1.Quaternion());

	// Spine bones rotation + transform restore
	Num = BodySetup.SpineBoneNames.Num();
	for (int i = 0; i < Num; i++) {
		FQuat q = FMath::Lerp(SequenceEnd.GetRotation(), SequenceStart.GetRotation(), (float)(i + 1) / (float)(Num + 1));
		CapturedBody[BodySetup.SpineBoneNames[i]].SetRotation(q);
	}

	// get resulted skeletal mesh head location
	FTransform MeshHead = CapturedBody[Pelvis];
	Num = BodySetup.SpineBoneNames.Num();
	for (int i = Num - 1; i >= 0; i--) {
		MeshHead = BodySetup.SpineBones[BodySetup.SpineBoneNames[i]] * MeshHead;
	}
	MeshHead = BodySetup.RibcageToSpine * MeshHead;

	CapturedBody[Ribcage].SetTranslation(MeshHead.GetTranslation());

	// set component position
	v0 = CapturedBody[Pelvis].GetTranslation();
	if (bHasRootBone) v0.Z = CapturedBodyRaw.GroundLevel;
	ComponentWorldTransform.SetTranslation(v0);

	r0.Roll = r0.Pitch = 0.f;
	if (bLockRootBoneRotation)
	{
		r0.Yaw = 0.f;
	}
	else {
		r0.Yaw = GetYawRotation(
			CapturedBodyRaw.Pelvis.GetRotation().GetForwardVector(),
			CapturedBodyRaw.Pelvis.GetRotation().GetUpVector());
	}
	ComponentWorldTransform.SetRotation(r0.Quaternion());

	if (BodyMesh->GetAttachParent())
	{
		ComponentActorTransform = ComponentWorldTransform.GetRelativeTransform(BodyMesh->GetAttachParent()->GetComponentTransform());
	}

	BodyDirection = UKismetMathLibrary::MakeRotFromXZ(CapturedBodyRaw.Pelvis.GetRotation().GetForwardVector(), GetOwner()->GetActorUpVector());

	// root bone position
	if (bHasRootBone)
	{
		if (bRootMotion)
		{
			CapturedBody[RootBone].SetTranslation(ComponentWorldTransform.GetTranslation());
			CapturedBody[RootBone].SetRotation(UKismetMathLibrary::ComposeRotators(BodySetup.RootBoneRotation, ComponentWorldTransform.Rotator()).Quaternion());
		}
		else
		{
			CapturedBody[RootBone].SetTranslation(RefComponent.GetTranslation());
			CapturedBody[RootBone].SetRotation(RefComponent.GetRotation());
		}
	}

	// and limbs

	// shoulder right
	CapturedBody[ShoulderRight] = BodySetup.ShoulderToRibcageRight * CapturedBody[Ribcage];
	r0 = CapturedBody[ShoulderRight].Rotator();
	r1 = FRotator::ZeroRotator;
	SetAxisRotation(r1, BodySetup.RightShoulderRotation.VerticalAxis, BodySetup.RightShoulderRotation.UpDirection * CapturedBodyRaw.CollarboneRight.Yaw);

	bool bInvertShoulder = (BodySetup.RightShoulderRotation.HorizontalAxis == EAxis::Y) ^ bInvertElbows;

	if (bInvertShoulder)
		SetAxisRotation(r1, BodySetup.RightShoulderRotation.HorizontalAxis, BodySetup.RightShoulderRotation.RightDirection * CapturedBodyRaw.CollarboneRight.Pitch);
	else
		SetAxisRotation(r1, BodySetup.RightShoulderRotation.HorizontalAxis, -BodySetup.RightShoulderRotation.RightDirection * CapturedBodyRaw.CollarboneRight.Pitch);
	VRIKBodyMath::IKB_AddRelativeRotation(r0, r1);
	CapturedBody[ShoulderRight].SetRotation(r0.Quaternion());

	// Recalculate TwoBoneIK for right hand to correspond real mesh position
	// Skeletal Mesh Shoulders have offset relative to VRIKBody data shoulders, so upperarm and lowerarm must be recalculated
	t0 = BodySetup.UpperarmToShoulderRight * CapturedBody[ShoulderRight];
	CalculateTwoBoneIK(
		t0.GetTranslation(),
		CapturedBodyRaw.HandRight.GetTranslation(),
		CapturedBodyRaw.ElbowJointTargetRight,
		BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
		CapturedBody[UpperarmRight], CapturedBody[LowerarmRight],	// <-- return values
		BodySetup.RightHandRotation, bInvertElbows);

	// palm right
	r0 = CapturedBodyRaw.HandRight.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.PalmRightRotation.ForwardAxis, BodySetup.PalmRightRotation.ForwardDirection,
		BodySetup.PalmRightRotation.HorizontalAxis, BodySetup.PalmRightRotation.RightDirection,
		BodySetup.PalmRightRotation.VerticalAxis, BodySetup.PalmRightRotation.UpDirection);
	CapturedBody[PalmRight].SetTranslation(CapturedBodyRaw.HandRight.GetTranslation());
	CapturedBody[PalmRight].SetRotation(r1.Quaternion());

	// shoulder left
	CapturedBody[ShoulderLeft] = BodySetup.ShoulderToRibcageLeft * CapturedBody[Ribcage];
	r0 = CapturedBody[ShoulderLeft].Rotator();
	r1 = FRotator::ZeroRotator;
	SetAxisRotation(r1, BodySetup.LeftShoulderRotation.VerticalAxis, BodySetup.LeftShoulderRotation.UpDirection * CapturedBodyRaw.CollarboneLeft.Yaw);

	bInvertShoulder = (BodySetup.LeftShoulderRotation.HorizontalAxis == EAxis::Y) ^ bInvertElbows;

	if (bInvertShoulder)
		SetAxisRotation(r1, BodySetup.LeftShoulderRotation.HorizontalAxis, BodySetup.LeftShoulderRotation.RightDirection * CapturedBodyRaw.CollarboneLeft.Pitch);
	else
		SetAxisRotation(r1, BodySetup.LeftShoulderRotation.HorizontalAxis, -BodySetup.LeftShoulderRotation.RightDirection * CapturedBodyRaw.CollarboneLeft.Pitch);
	VRIKBodyMath::IKB_AddRelativeRotation(r0, r1);
	CapturedBody[ShoulderLeft].SetRotation(r0.Quaternion());

	// Recalculate TwoBoneIK for left hand
	t0 = BodySetup.UpperarmToShoulderLeft * CapturedBody[ShoulderLeft];
	CalculateTwoBoneIK(
		t0.GetTranslation(),
		CapturedBodyRaw.HandLeft.GetTranslation(),
		CapturedBodyRaw.ElbowJointTargetLeft,
		BodySetup.fUpperarmLength * ScaleMultiplierHorizontal, BodySetup.fForearmLength * ScaleMultiplierHorizontal,
		CapturedBody[UpperarmLeft], CapturedBody[LowerarmLeft],	// <-- return values
		BodySetup.LeftHandRotation, bInvertElbows);

	// palm left
	r0 = CapturedBodyRaw.HandLeft.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.PalmLeftRotation.ForwardAxis, BodySetup.PalmLeftRotation.ForwardDirection,
		BodySetup.PalmLeftRotation.HorizontalAxis, BodySetup.PalmLeftRotation.RightDirection,
		BodySetup.PalmLeftRotation.VerticalAxis, BodySetup.PalmLeftRotation.UpDirection);
	CapturedBody[PalmLeft].SetTranslation(CapturedBodyRaw.HandLeft.GetTranslation());
	CapturedBody[PalmLeft].SetRotation(r1.Quaternion());

	// thigh right
	r0 = CapturedBodyRaw.ThighRight.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.RightLegRotation.ForwardAxis, BodySetup.RightLegRotation.ForwardDirection,
		BodySetup.RightLegRotation.HorizontalAxis, BodySetup.RightLegRotation.RightDirection,
		BodySetup.RightLegRotation.VerticalAxis, BodySetup.RightLegRotation.UpDirection);
	CapturedBody[ThighRight].SetTranslation(CapturedBodyRaw.ThighRight.GetTranslation());
	CapturedBody[ThighRight].SetRotation(r1.Quaternion());
	 
	// calf right
	r0 = CapturedBodyRaw.CalfRight.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.RightLegRotation.ForwardAxis, BodySetup.RightLegRotation.ForwardDirection,
		BodySetup.RightLegRotation.HorizontalAxis, BodySetup.RightLegRotation.RightDirection,
		BodySetup.RightLegRotation.VerticalAxis, BodySetup.RightLegRotation.UpDirection);
	CapturedBody[CalfRight].SetTranslation(CapturedBodyRaw.CalfRight.GetTranslation());
	CapturedBody[CalfRight].SetRotation(r1.Quaternion());

	// foot right
	CapturedBody[FootRight] = BodySetup.FootToGroundRight * CapturedBodyRaw.FootRightCurrent;

	// thigh left
	r0 = CapturedBodyRaw.ThighLeft.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.LeftLegRotation.ForwardAxis, BodySetup.LeftLegRotation.ForwardDirection,
		BodySetup.LeftLegRotation.HorizontalAxis, BodySetup.LeftLegRotation.RightDirection,
		BodySetup.LeftLegRotation.VerticalAxis, BodySetup.LeftLegRotation.UpDirection);
	CapturedBody[ThighLeft].SetTranslation(CapturedBodyRaw.ThighLeft.GetTranslation());
	CapturedBody[ThighLeft].SetRotation(r1.Quaternion());

	// calf left
	r0 = CapturedBodyRaw.CalfLeft.Rotator();
	r1 = BuildTransformFromAxes(r0,
		BodySetup.LeftLegRotation.ForwardAxis, BodySetup.LeftLegRotation.ForwardDirection,
		BodySetup.LeftLegRotation.HorizontalAxis, BodySetup.LeftLegRotation.RightDirection,
		BodySetup.LeftLegRotation.VerticalAxis, BodySetup.LeftLegRotation.UpDirection);
	CapturedBody[CalfLeft].SetTranslation(CapturedBodyRaw.CalfLeft.GetTranslation());
	CapturedBody[CalfLeft].SetRotation(r1.Quaternion());

	// foot left
	CapturedBody[FootLeft] = BodySetup.FootToGroundLeft * CapturedBodyRaw.FootLeftCurrent;

	if (bCalculateAnimatedLegs)
	{
		FTransform tFootRight = BodyMesh->GetSocketTransform(FootRight);
		FTransform tFootLeft = BodyMesh->GetSocketTransform(FootLeft);

		const FVector UpVec = RefComponent.GetRotation().GetUpVector();
		const FVector UpDir = 120.f * UpVec;

		VRIKSolver->TraceFloorInPoint(tFootRight.GetTranslation() + UpDir, FootRight_Location);
		VRIKSolver->TraceFloorInPoint(tFootLeft.GetTranslation() + UpDir, FootLeft_Location);
		FootRight_Location += VRIKSolver->FootOffsetToGround * UpVec;
		FootLeft_Location += VRIKSolver->FootOffsetToGround * UpVec;
	}

	return true;
}

void UVRIKSkeletalMeshTranslator::ApplyBlendSpace()
{
	const FVector Velocity = VRIKSolver->GetCharacterVelocity();
	const FRotator VelocityRot = UKismetMathLibrary::MakeRotFromXZ(Velocity, GetOwner()->GetActorUpVector());
	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(VelocityRot, BodyDirection);

	//LocomotionBlendSpace->GetSamplesFromBlendInput(FVector(DeltaRot.Yaw, Velocity.Size(), 0.f), BlendSampleDataCache);

	const float DeltaTime = GetWorld()->DeltaTimeSeconds;
	// Find max weight anim
	float RefDuration = 0.f;
	float MaxWeight = 0.f;
	for (auto& Anim : BlendSampleDataCache)
	{
#if ENGINE_MAJOR_VERSION >= 5
		float CurrWeight = Anim.GetClampedWeight();
#else
		float CurrWeight = Anim.GetWeight();
#endif
		if (CurrWeight > MaxWeight)
		{
#if ENGINE_MAJOR_VERSION >= 5
			RefDuration = Anim.Animation->GetPlayLength();
#else
			RefDuration = Anim.Animation->SequenceLength;
#endif
			MaxWeight = CurrWeight;
		}
	}

	// New blend space time (0..1) based no max weight anim duration
	CurrentBlendAnimationAlpha += DeltaTime / RefDuration;

	for (auto& Anim : BlendSampleDataCache)
	{
#if ENGINE_MAJOR_VERSION >= 5
		Anim.Time = CurrentBlendAnimationAlpha * Anim.Animation->GetPlayLength();
#else
		Anim.Time = CurrentBlendAnimationAlpha * Anim.Animation->SequenceLength;
#endif
	}
}

// Math helper. Set value at Rotator by Axis.
void UVRIKSkeletalMeshTranslator::SetAxisRotation(FRotator& Rotator, EAxis::Type Axis, float Value)
{
	/**/ if (Axis == EAxis::X) Rotator.Roll = Value;
	else if (Axis == EAxis::Y) Rotator.Pitch = Value;
	else if (Axis == EAxis::Z) Rotator.Yaw = Value;
}

// Math helper. Get axis value of the specified Rotator.
FORCEINLINE float UVRIKSkeletalMeshTranslator::GetAxisRotation(const FRotator& Rotator, EAxis::Type Axis)
{
	/**/ if (Axis == EAxis::X) return Rotator.Roll;
	else if (Axis == EAxis::Y) return Rotator.Pitch;
	else if (Axis == EAxis::Z) return Rotator.Yaw;
	else return 0.f;
}

// Helper. Find axis of rotator the closest to being parallel to the specified vectors. Returns +1.f in Multiplier if co-directed and -1.f otherwise
FORCEINLINE EAxis::Type UVRIKSkeletalMeshTranslator::FindCoDirection(const FRotator& BoneRotator, const FVector& Direction, float& ResultMultiplier) const
{
	EAxis::Type RetAxis;
	FVector dir = Direction;
	dir.Normalize();

	const FVector v1 = RotatorDirection(BoneRotator, EAxis::X);
	const FVector v2 = RotatorDirection(BoneRotator, EAxis::Y);
	const FVector v3 = RotatorDirection(BoneRotator, EAxis::Z);

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

// Math. Calculate IK transforms for two-bone chain.
// Need to be refactored to consider skeleton bones orientation!
void UVRIKSkeletalMeshTranslator::CalculateTwoBoneIK(
												const FVector& ChainStart, const FVector& ChainEnd,
												const FVector& JointTarget,
												const float UpperBoneSize, const float LowerBoneSize,
												FTransform& UpperBone, FTransform& LowerBone,
												FBoneRotatorSetup& LimbSpace,
												bool bInvertBending, bool bSnapToChainEnd)
{
	const float DirectSize = FVector::Dist(ChainStart, ChainEnd);
	const float DirectSizeSquared = DirectSize * DirectSize;
	const float BendingKoef = bInvertBending ? -1.f : 1.f;
	const float a = UpperBoneSize * UpperBoneSize;
	const float b = LowerBoneSize * LowerBoneSize;

	// 1) upperbone and lowerbone plane angles
	float Angle1 = FMath::RadiansToDegrees(FMath::Acos((DirectSizeSquared + a - b) / (2.f * UpperBoneSize * DirectSize)));
	float Angle2 = FMath::RadiansToDegrees(FMath::Acos((a + b - DirectSizeSquared) / (2.f * UpperBoneSize * LowerBoneSize)));

	// 2) Find rotation plane
	FTransform ChainPlane = FTransform(ChainStart);
	// upperarm-hand as FORWARD direction
	FVector FrontVec = (ChainEnd - ChainStart).GetSafeNormal();
	// RIGHT direction
	FVector JointVec = (JointTarget - ChainStart).GetSafeNormal(); JointVec *= (LimbSpace.RightDirection * LimbSpace.ExternalDirection);
	// (temp: UP direction)
	FVector PlaneNormal = JointVec ^ FrontVec; PlaneNormal.Normalize();
	FVector RightVec = FrontVec ^ PlaneNormal; RightVec.Normalize();
	ChainPlane.SetRotation(MakeRotByTwoAxes(LimbSpace.ForwardAxis, FrontVec * LimbSpace.ForwardDirection, LimbSpace.HorizontalAxis, RightVec * LimbSpace.RightDirection).Quaternion());

	// direction to joint target at the rotation plane
	float RotationDirection = LimbSpace.ExternalDirection * LimbSpace.ForwardDirection * BendingKoef;

	// 3) upper bone
	FRotator RotUpperbone = FRotator::ZeroRotator;
	SetAxisRotation(RotUpperbone, LimbSpace.VerticalAxis, Angle1 * RotationDirection);
	UpperBone = FTransform(RotUpperbone, FVector::ZeroVector) * ChainPlane;
	UpperBone.SetTranslation(ChainStart);

	// 4) lower bone
	ChainPlane = FTransform(UpperBone.GetRotation(), UpperBone.GetTranslation() + RotatorDirection(UpperBone.Rotator(), LimbSpace.ForwardAxis) * LimbSpace.ForwardDirection * UpperBoneSize);
	SetAxisRotation(RotUpperbone, LimbSpace.VerticalAxis, (Angle2 - 180.f) * RotationDirection);
	LowerBone = FTransform(RotUpperbone, FVector::ZeroVector) * ChainPlane;
	LowerBone.SetTranslation(ChainStart + UpperBone.GetRotation().GetForwardVector() * LimbSpace.ForwardDirection  * UpperBoneSize);

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
}

// Helper. Build rotator by two axis.
FRotator UVRIKSkeletalMeshTranslator::MakeRotByTwoAxes(EAxis::Type MainAxis, FVector MainAxisDirection, EAxis::Type SecondaryAxis, FVector SecondaryAxisDirection)
{
	FRotator r;

	if (MainAxis == EAxis::X) {
		if (SecondaryAxis == EAxis::Y)
			r = FRotationMatrix::MakeFromXY(MainAxisDirection, SecondaryAxisDirection).Rotator();
		else if (SecondaryAxis == EAxis::Z)
			r = FRotationMatrix::MakeFromXZ(MainAxisDirection, SecondaryAxisDirection).Rotator();
	}
	else if (MainAxis == EAxis::Y) {
		if (SecondaryAxis == EAxis::X)
			r = FRotationMatrix::MakeFromYX(MainAxisDirection, SecondaryAxisDirection).Rotator();
		else if (SecondaryAxis == EAxis::Z)
			r = FRotationMatrix::MakeFromYZ(MainAxisDirection, SecondaryAxisDirection).Rotator();
	}
	else if (MainAxis == EAxis::Z) {
		if (SecondaryAxis == EAxis::X)
			r = FRotationMatrix::MakeFromZX(MainAxisDirection, SecondaryAxisDirection).Rotator();
		else if (SecondaryAxis == EAxis::Y)
			r = FRotationMatrix::MakeFromZY(MainAxisDirection, SecondaryAxisDirection).Rotator();
	}

	return r;
}

// Calculate Forward Rotation (Yaw) for pelvis
FORCEINLINE float UVRIKSkeletalMeshTranslator::GetYawRotation(const FVector& ForwardVector, const FVector& UpVector)
{
	FVector FxyN = FVector(ForwardVector.X, ForwardVector.Y, 0.f).GetSafeNormal();
	const FVector Uxy = FVector(UpVector.X, UpVector.Y, 0.f);
	const FVector UxyN = Uxy.GetSafeNormal();

	// squared limit value
	const float TurningTreshold = 0.7f * 0.7f;

	// projection of the forward vector on horizontal plane is too small
	const float ProjectionSize = Uxy.SizeSquared();
	if (ProjectionSize > TurningTreshold) {
		// interpolation alpha
		const float alpha = FMath::Clamp((ProjectionSize - TurningTreshold) / (1.f - TurningTreshold), 0.f, 1.f);

		// new forward vector
		FVector NewF;
		// choose down or up side and use interpolated vector between forward and up to find current horizontal forward direction
		if (ForwardVector.Z < 0.f) {
			NewF = FMath::Lerp((UpVector.Z > 0.f) ? ForwardVector : -ForwardVector, UpVector, alpha);
		}
		else {
			NewF = FMath::Lerp((UpVector.Z > 0.f) ? ForwardVector : -ForwardVector, -UpVector, alpha);
		}

		FxyN = FVector(NewF.X, NewF.Y, 0.f).GetSafeNormal();

		return FMath::RadiansToDegrees(FMath::Atan2(FxyN.Y, FxyN.X));
	}
	// projection of the forward vector on horizontal plane is large enough to use it
	else {
		const float YawRet = FMath::RadiansToDegrees(FMath::Atan2(FxyN.Y, FxyN.X));

		return (UpVector.Z > 0.f) ? YawRet : -YawRet;
	}
}

void UVRIKSkeletalMeshTranslator::DebugShowBonesOrientation()
{
	if (!bIsInitialized) return;
	UWorld* w = GetWorld();
	if (!w) return;

	const FTransform tPelvis = BodyMesh->GetSocketTransform(Pelvis);
	const FTransform tRibcage = BodyMesh->GetSocketTransform(Ribcage);
	const FTransform tHead = BodyMesh->GetSocketTransform(Head);
	const FTransform tShoulderRight = BodyMesh->GetSocketTransform(ShoulderRight);
	const FTransform tUpperarmRight = BodyMesh->GetSocketTransform(UpperarmRight);
	const FTransform tForearmRight = BodyMesh->GetSocketTransform(LowerarmRight);
	const FTransform tPalmRight = BodyMesh->GetSocketTransform(PalmRight);
	const FTransform tShoulderLeft = BodyMesh->GetSocketTransform(ShoulderLeft);
	const FTransform tUpperarmLeft = BodyMesh->GetSocketTransform(UpperarmLeft);
	const FTransform tForearmLeft = BodyMesh->GetSocketTransform(LowerarmLeft);
	const FTransform tPalmLeft = BodyMesh->GetSocketTransform(PalmLeft);
	const FTransform tThighRight = BodyMesh->GetSocketTransform(ThighRight);
	const FTransform tCalfRight = BodyMesh->GetSocketTransform(CalfRight);
	const FTransform tFootRight = BodyMesh->GetSocketTransform(FootRight);
	const FTransform tThighLeft = BodyMesh->GetSocketTransform(ThighLeft);
	const FTransform tCalfLeft = BodyMesh->GetSocketTransform(CalfLeft);
	const FTransform tFootLeft = BodyMesh->GetSocketTransform(FootLeft);

	DrawAxes(w, tPelvis.GetTranslation(), BodySetup.PelvisRotation, tPelvis.Rotator());
	DrawAxes(w, tRibcage.GetTranslation(), BodySetup.RibcageRotation, tRibcage.Rotator());
	DrawAxes(w, tHead.GetTranslation(), BodySetup.HeadRotation, tHead.Rotator());
	DrawAxes(w, tShoulderRight.GetTranslation(), BodySetup.RightHandRotation, tShoulderRight.Rotator());
	DrawAxes(w, tUpperarmRight.GetTranslation(), BodySetup.RightHandRotation, tUpperarmRight.Rotator());
	DrawAxes(w, tForearmRight.GetTranslation(), BodySetup.RightHandRotation, tForearmRight.Rotator());
	DrawAxes(w, tShoulderLeft.GetTranslation(), BodySetup.LeftHandRotation, tShoulderLeft.Rotator());
	DrawAxes(w, tUpperarmLeft.GetTranslation(), BodySetup.LeftHandRotation, tUpperarmLeft.Rotator());
	DrawAxes(w, tForearmLeft.GetTranslation(), BodySetup.LeftHandRotation, tForearmLeft.Rotator());
	DrawAxes(w, tPalmRight.GetTranslation(), BodySetup.PalmRightRotation, tPalmRight.Rotator());
	DrawAxes(w, tPalmLeft.GetTranslation(), BodySetup.PalmLeftRotation, tPalmLeft.Rotator());
	DrawAxes(w, tThighRight.GetTranslation(), BodySetup.RightLegRotation, tThighRight.Rotator());
	DrawAxes(w, tCalfRight.GetTranslation(), BodySetup.RightLegRotation, tCalfRight.Rotator());
	DrawAxes(w, tFootRight.GetTranslation(), BodySetup.FootRightRotation, tFootRight.Rotator());
	DrawAxes(w, tThighLeft.GetTranslation(), BodySetup.LeftLegRotation, tThighLeft.Rotator());
	DrawAxes(w, tCalfLeft.GetTranslation(), BodySetup.LeftLegRotation, tCalfLeft.Rotator());
	DrawAxes(w, tFootLeft.GetTranslation(), BodySetup.FootLeftRotation, tFootLeft.Rotator());
}

void UVRIKSkeletalMeshTranslator::DrawAxes(UWorld* World, const FVector& Location, FBoneRotatorSetup RotSetup, FRotator Rot)
{
	FVector vF, vR, vU;
	FRotationMatrix r = FRotationMatrix(Rot);
	
	vF = r.GetScaledAxis(RotSetup.ForwardAxis) * RotSetup.ForwardDirection;
	vR = r.GetScaledAxis(RotSetup.HorizontalAxis) * RotSetup.RightDirection;
	vU = r.GetScaledAxis(RotSetup.VerticalAxis) * RotSetup.UpDirection;

	DrawDebugLine(World, Location, Location + vF * 20.f, FColor::Red, false, 0.1f, 0, 0.3f);
	DrawDebugLine(World, Location, Location + vR * 20.f, FColor::Green, false, 0.1f, 0, 0.3f);
	DrawDebugLine(World, Location, Location + vU * 20.f, FColor::Blue, false, 0.1f, 0, 0.3f);
}

float UVRIKSkeletalMeshTranslator::DebugGetYawRotation(const FVector ForwardVector, const FVector UpVector)
{
	return GetYawRotation(ForwardVector, UpVector);
}