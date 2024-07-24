// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#include "YnnkVrAvatar.h"
// ENGINE
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Animation/PoseSnapshot.h"
#include "Engine/NetSerialization.h"
#include "Runtime/Launch/Resources/Version.h"
#include "HAL/UnrealMemory.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

// PROJECT
#include "VRIKBodyPrivatePCH.h"
#include "RangeLimitedFABRIK.h"
#include "VRIKBodyMath.h"
#include "SimpleTorchController.h"
#include "YnnkVRIKTypes.h"
#include "KerasElbow.h"
#include "VRIKFingersFKIKSolver.h"

DEFINE_LOG_CATEGORY(LogYnnk);

#define PushTransformToArr(Arr, Ind, Loc, Q, Vel) \
Arr[Ind + 0] = Loc.X; \
Arr[Ind + 1] = Loc.Y; \
Arr[Ind + 2] = Loc.Z; \
Arr[Ind + 3] = Q.X * 30.f; \
Arr[Ind + 4] = Q.Y * 30.f; \
Arr[Ind + 5] = Q.Z * 30.f; \
Arr[Ind + 6] = Q.W * 30.f; \
Arr[Ind + 7] = Vel.X; \
Arr[Ind + 8] = Vel.Y; \
Arr[Ind + 9] = Vel.Z
#define ReadTransformFromArr(Arr, Ind, Loc, Q) Loc.X = Arr[Ind+0]; Loc.Y = Arr[Ind+1]; Loc.Z = Arr[Ind+2]; Q.X = Arr[Ind+3]; Q.Y = Arr[Ind+4]; Q.Z = Arr[Ind+5]; Q.W = Arr[Ind+6]; Q.Normalize()

#define NetworkInterpTransform(a, b) a = (NetworkInterpolationSpeed == 0.f) ? b : UKismetMathLibrary::TInterpTo(a, b, DeltaTime, NetworkInterpolationSpeed)

#define TRUE 1
#define FALSE 0

UYnnkVrAvatarComponent::UYnnkVrAvatarComponent()
	: HandRightOffset(FTransform(FRotator(-60.f, 65.f, 25.f), FVector(-10.f, -4.5f, 9.f)))
	, HandLeftOffset(FTransform(FRotator(-60.f, -65.f, -25.f), FVector(-10.f, 4.5f, 9.f)))
	, AvatarSkeletalMeshName(TEXT("CharacterMesh0"))
	, PlayerHeight(170.f)
	, PlayerArmSpan(152.f)
	, StepSize(60.f)
	, TorsoVerticalOffset(0.f)
	, HeadBoneName(TEXT("head"))
	, HandRightBoneName(TEXT("hand_r"))
	, HandLeftBoneName(TEXT("hand_l"))
	, FootRightBoneName(TEXT("foot_r"))
	, FootLeftBoneName(TEXT("foot_l"))
	, CalibrationOffset_Head(2.f)
	, CalibrationOffset_Hand(3.f)
	, SpineBendingMultiplier(0.3f)
	, ElbowModel(EYnnkElbowModelSource::YEM_Procedural)
	, ElbowSinkFactor(0.2f)
	, ElbowFarSinkFactor(0.7f)
	, HeadJointOffsetFromHMD(FVector(-8.f, 0.f, -12.f))
	, RibageOffsetFromHeadJoint(FVector(0.f, 0.f, -15.f))
	, bFlatGround(true)
	, bTraceGroundByObjectType(false)
	, GroundCollisionChannel(ECollisionChannel::ECC_Visibility)
	, FeetVerticalOffset(0.f)
	, ComponentForwardYaw(-90.f)
	, bBlendLegsAnimation(false)
	, BodyModel(EYnnkBodyModelSource::YBM_YnnkNeuralNet)
	, NeuralNet_SmoothFrames(12)
	, NeuralNet_FilterPower(1.f)
	, bNeuralNet_IgnoreOwnerMovement(true)
	, ProceduralPelvisReturnToHeadAngle(55.f)
	, ProceduralPelvisReturnToHeadAngleSoft(35.f)
	, ProceduralTorsoRotationSmoothFrames(50)
	, NetworkInterpolationSpeed(16.f)
	, ServerUpdateInterval(0.f)
	, bAutoReplicateBodyCalibration(false)
{
	// setup defaults
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_StepHeight(TEXT("CurveFloat'/VRIKBody/Curves/FC_StepHeight.FC_StepHeight'"));
	if (CurveFinder_StepHeight.Succeeded())
	{
		CurveStepHeight = CurveFinder_StepHeight.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_StepShape(TEXT("CurveFloat'/VRIKBody/Curves/FC_StepPitch.FC_StepPitch'"));
	if (CurveFinder_StepShape.Succeeded())
	{
		CurveStepPitch = CurveFinder_StepShape.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_StepOffset(TEXT("CurveFloat'/VRIKBody/Curves/FC_StepOffset.FC_StepOffset'"));
	if (CurveFinder_StepOffset.Succeeded())
	{
		CurveStepOffset = CurveFinder_StepOffset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_RibcagePitch(TEXT("CurveFloat'/VRIKBody/Curves/FC_RibcagePItch.FC_RibcagePItch'"));
	if (CurveFinder_RibcagePitch.Succeeded())
	{
		CurveRibcagePitch = CurveFinder_RibcagePitch.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_RibcageRoll(TEXT("CurveFloat'/VRIKBody/Curves/FC_RibcageRoll.FC_RibcageRoll'"));
	if (CurveFinder_RibcageRoll.Succeeded())
	{
		CurveRibcageRoll = CurveFinder_RibcageRoll.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_SpinePitch(TEXT("CurveFloat'/VRIKBody/Curves/FC_SpinePitch.FC_SpinePitch'"));
	if (CurveFinder_SpinePitch.Succeeded())
	{
		CurveSpinePitch = CurveFinder_SpinePitch.Object;
	}
	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveFinder_PelvisPitch(TEXT("CurveFloat'/VRIKBody/Curves/FC_PelvisPitch.FC_PelvisPitch'"));
	if (CurveFinder_PelvisPitch.Succeeded())
	{
		CurvePelvisPitch = CurveFinder_PelvisPitch.Object;
	}

	// used for filtering/smoothing NN outputs
	RuntimeIndex = 0;
	WalkingSpeedHistoryIndex = 0;
	bPyTorchAvailable = true;
	NnInputScale = FVector::OneVector;
	KerasElbowModel = nullptr;
	InternalBodyPose = EYnnkBodyPose::YBP_Default;
}

/* Networking Properties Replication */
void UYnnkVrAvatarComponent::GetLifetimeReplicatedProps(TArray <FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYnnkVrAvatarComponent, IKTargetsRep);
	DOREPLIFETIME(UYnnkVrAvatarComponent, IKInputsRep);
	DOREPLIFETIME(UYnnkVrAvatarComponent, BodyCalibrationRep);
	DOREPLIFETIME(UYnnkVrAvatarComponent, FingersRepRight);
	DOREPLIFETIME(UYnnkVrAvatarComponent, FingersRepLeft);
}

void UYnnkVrAvatarComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UYnnkVrAvatarComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialized)
	{
		return;
	}

	// Get IK coordinates
	UpdateIKTargets(DeltaTime);

	const FTransform RefTr = GroundRefComponent->GetComponentTransform();

	// Build body

	// A. Make spine spline from head to pelvis
	// B. Torso IK to adjust ribcage

	FVector RibcageDirection = FMath::Lerp(IKTargets.Head.GetRotation().GetUpVector(),
		(IKTargets.Head.GetTranslation() - IKTargets.Pelvis.GetTranslation()).GetSafeNormal(),
		0.5f);

	if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
	{
		const FVector RibcageTargetL = IKTargets.Head.GetTranslation() - RibcageDirection * NeckLength;
		const FQuat RibcageTargetQ = FQuat::Slerp(IKTargets.Pelvis.GetRotation(), IKTargets.Head.GetRotation(), 0.05f);
		IKTargets.Ribcage = FTransform(RibcageTargetQ, RibcageTargetL);
	}

	FTransform RibcageTarget = IKTargets.Ribcage;
	FTransform RibcageBoneOffset = RibcageBoneOffsetFromJoint;
	RibcageBoneOffset.ScaleTranslation(MeshScale.Z);
	const FTransform RibcageTargetAsBone = RibcageBoneOffset * IKTargets.Ribcage;
	RibcageTarget.SetTranslation(
		RibcageTargetAsBone.GetTranslation()
	);

	FTransform RibcageTrTarget;
	// 1st pass: calculate neck to ribcage
	if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
	{
		// save initial ribcage position (calculated from head)
		RibcageTrTarget = CalculateBezierSpine(RibcageTarget, IKTargets.Head, SpineBones_RibcageIndex, 0, NeckLength);
	}
	else
	{
		// save initial ribcage position (calculated from head)
		RibcageTrTarget = CalculateNeck(RibcageTarget, IKTargets.Head, SpineBones_RibcageIndex, 0);// , NeckLength);
	}

	// 2nd pass: torso update
	CalculateBezierSpine(IKTargets.Pelvis, RibcageTrTarget, SpineBones.Num() - 1, SpineBones_RibcageIndex, SpineLength);

	// collarbones
	CalcShouldersWithOffset(EControllerHand::Right, DeltaTime);
	CalcShouldersWithOffset(EControllerHand::Left, DeltaTime);
		
	// hands
	// current upperarm locations
	BonesActorSpace[UpperarmRightIndex] = UpperarmRightToClavicleGen * BonesActorSpace[ClavicleRightIndex];
	BonesActorSpace[UpperarmLeftIndex] = UpperarmLeftToClavicleGen * BonesActorSpace[ClavicleLeftIndex];
		
	// elbows
	if (ElbowModel == EYnnkElbowModelSource::YEM_NeuralNet && IsValid(KerasElbowModel))
	{
		KerasElbowModel->Evaluate(IKTargets.Ribcage, IKTargets.HandRight, IKTargets.HandLeft, ElbowTargetRight_AS, ElbowTargetLeft_AS);
	}
	else
	{
		ElbowTargetRight_AS = CalculateElbow(BonesActorSpace[RibcageIndex], BonesActorSpace[UpperarmRightIndex], IKTargets.HandRight, true);
		ElbowTargetLeft_AS = CalculateElbow(BonesActorSpace[RibcageIndex], BonesActorSpace[UpperarmLeftIndex], IKTargets.HandLeft, false);
	}

	// Right hand
	FVector HandPalmRight = YnnkHelpers::CalculateTwoBoneIK(
		BonesActorSpace[UpperarmRightIndex].GetTranslation(),
		IKTargets.HandRight.GetTranslation(), ElbowTargetRight_AS,
		BoneSizeScaled.UpperarmLength, BoneSizeScaled.LowerarmLength,
		BonesActorSpace[UpperarmRightIndex], BonesActorSpace[LowerarmRightIndex], true, false); // right side
	BonesActorSpace[HandRightIndex] = IKTargets.HandRight;

	// Left hand
	FVector HandPalmLeft = YnnkHelpers::CalculateTwoBoneIK(
		BonesActorSpace[UpperarmLeftIndex].GetTranslation(),
		IKTargets.HandLeft.GetTranslation(), ElbowTargetLeft_AS,
		BoneSizeScaled.UpperarmLength, BoneSizeScaled.LowerarmLength,
		BonesActorSpace[UpperarmLeftIndex], BonesActorSpace[LowerarmLeftIndex], false, false); // left side
	BonesActorSpace[HandLeftIndex] = IKTargets.HandLeft;

	if (bUpperBodyPriorityHands)
	{
		CapturedSkeletonR[0] = BonesActorSpace[UpperarmRightIndex];
		CapturedSkeletonR[1] = BonesActorSpace[LowerarmRightIndex];
		CapturedSkeletonR[2] = FTransform(IKTargets.HandRight.GetRotation(), HandPalmRight);

		CapturedSkeletonL[0] = BonesActorSpace[UpperarmLeftIndex];
		CapturedSkeletonL[1] = BonesActorSpace[LowerarmLeftIndex];
		CapturedSkeletonL[2] = FTransform(IKTargets.HandLeft.GetRotation(), HandPalmLeft);

		// full torso IK
		CalcTorsoIK();
	}		

	// collarbones
	CalcShouldersWithOffset(EControllerHand::Right, DeltaTime);
	CalcShouldersWithOffset(EControllerHand::Left, DeltaTime);

	// hands
	// current upperarm locations
	BonesActorSpace[UpperarmRightIndex] = UpperarmRightToClavicleGen * BonesActorSpace[ClavicleRightIndex];
	BonesActorSpace[UpperarmLeftIndex] = UpperarmLeftToClavicleGen * BonesActorSpace[ClavicleLeftIndex];

	// update neck and head bones
	FVector RibcageOffset = BonesActorSpace[RibcageIndex].GetTranslation() - RibcageTrTarget.GetTranslation();
	for (int32 i = 0; i < SpineBones_RibcageIndex; i++)
	{
		const int32 BoneIndex = SpineBoneIndices[i];
		BonesActorSpace[BoneIndex].AddToTranslation(RibcageOffset);
	}

	BonesActorSpace[HandRightIndex].SetRotation(IKTargets.HandRight.GetRotation());
	BonesActorSpace[HandLeftIndex].SetRotation(IKTargets.HandLeft.GetRotation());

	// single-pass legs

	// a. thigh locations
	BonesActorSpace[ThighRightIndex] = ThighRightToPelvisGen * BonesActorSpace[PelvisIndex];
	BonesActorSpace[ThighLeftIndex] = ThighLeftToPelvisGen * BonesActorSpace[PelvisIndex];

	// b. knee targets
	FRotator PelvisAsRootRot = YnnkHelpers::CalculateRootRotation(IKTargets.Pelvis.GetRotation().GetForwardVector(), IKTargets.Pelvis.GetRotation().GetUpVector());

	FVector LegRightCenter = (BonesActorSpace[PelvisIndex].GetTranslation() + IKTargets.FootRight.GetTranslation()) * 0.5f;

	KneeTargetRight_AS = FMath::Lerp(PelvisAsRootRot.Vector(), IKTargets.FootRight.GetRotation().GetForwardVector(), 0.4f).GetSafeNormal();
	KneeTargetRight_AS = LegRightCenter + KneeTargetRight_AS * 40.f;

	//DrawDebugLine(GetWorld(), LegRightCenter, LegRightCenter + KneeTargetRight_AS * 40.f, FColor::Yellow, false, 0.05f, 0, 0.5f);

	KneeTargetLeft_AS = FMath::Lerp(PelvisAsRootRot.Vector(), IKTargets.FootLeft.GetRotation().GetForwardVector(), 0.4f).GetSafeNormal();
	KneeTargetLeft_AS = (BonesActorSpace[PelvisIndex].GetTranslation() + IKTargets.FootLeft.GetTranslation()) * 0.5f + KneeTargetLeft_AS * 40.f;

	// c. two bone IK
	FVector FootLoc = YnnkHelpers::CalculateTwoBoneIK(
		BonesActorSpace[ThighRightIndex].GetTranslation(),
		IKTargets.FootRight.GetTranslation(), KneeTargetRight_AS,
		BoneSizeScaled.ThighLength, BoneSizeScaled.CalfLength,
		BonesActorSpace[ThighRightIndex], BonesActorSpace[CalfRightIndex], true, false); // right side

	BonesActorSpace[FootRightIndex] = FTransform(IKTargets.FootRight.GetRotation(), FootLoc);

	/*****/ FootLoc = YnnkHelpers::CalculateTwoBoneIK(
		BonesActorSpace[ThighLeftIndex].GetTranslation(),
		IKTargets.FootLeft.GetTranslation(), KneeTargetLeft_AS,
		BoneSizeScaled.ThighLength, BoneSizeScaled.CalfLength,
		BonesActorSpace[ThighLeftIndex], BonesActorSpace[CalfLeftIndex], true, false); // left side

	BonesActorSpace[FootLeftIndex] = FTransform(IKTargets.FootLeft.GetRotation(), FootLoc);

	// cleanup offsets and build final pose
	UpdatePoseSnapshot();
	
	// Ensure calibration is replicated (once in 3 seconds)
	if (bAutoReplicateBodyCalibration)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime > NextCalibrationCheckTime)
		{
			NextCalibrationCheckTime = CurrentTime + 3.f;
			UpdateBodyCalibration();
		}
	}

	// Should replicate fingers info?
	if (bReplicateFingers)
	{
		APawn* Player = Cast<APawn>(GetOwner());
		bool bLocallyControlled = !IsValid(Player) || Player->IsLocallyControlled();
		if (bLocallyControlled)
		{
			// send to server
			float CurrentTime = GetWorld()->GetTimeSeconds();
			if (CurrentTime > NextFingersUpdateTime || FingersUpdateInterval == 0.f)
			{
				NextFingersUpdateTime = CurrentTime + FingersUpdateInterval;

				TArray<uint8> FingersR, FingersL;
				RightHandSolver->GetOpenXRInputAsByteArray(FingersR);
				RightHandSolver->GetOpenXRInputAsByteArray(FingersL);

				ServerUpdateFingersData(FingersR, FingersL);
			}
		}
		else
		{
			// get from server and smoothen
			RightHandSolver->ApplyOpenXRInputFromByteArray(FingersRepRight);
			LeftHandSolver->ApplyOpenXRInputFromByteArray(FingersRepLeft);
		}
	}
}

void UYnnkVrAvatarComponent::UpdateBodyCalibration()
{
	if (!GetIsReplicated())
	{
		return;
	}

	FDateTime CurrentTime = FDateTime::Now();
	APawn* Player = Cast<APawn>(GetOwner());
	bool bLocallyControlled = !IsValid(Player) || Player->IsLocallyControlled();

	if (bLocallyControlled)
	{
		if (BodyCalibrationRep.Time != CurrentTime
			&& BodyCalibrationRep.PlayerHeight != PlayerHeight
			&& BodyCalibrationRep.PlayerArmSpan != PlayerArmSpan)
		{
			if (__uev_IsServer)
			{
				BodyCalibrationRep.Time = CurrentTime;
				BodyCalibrationRep.PlayerHeight = PlayerHeight;
				BodyCalibrationRep.PlayerArmSpan = PlayerArmSpan;
			}
			else
			{
				ServerAutoUpdateCalibration(CurrentTime, PlayerHeight, PlayerArmSpan);
			}
		}
	}
	else // remote instance
	{
		if (BodyCalibrationRep.Time != LastCalibrationUpdateTime)
		{
			LastCalibrationUpdateTime = BodyCalibrationRep.Time;
			SetBodySizeManually(BodyCalibrationRep.PlayerHeight, BodyCalibrationRep.PlayerArmSpan);
		}
	}
}

bool UYnnkVrAvatarComponent::InitializeWithComponents(USceneComponent* HeadSource, USceneComponent* HandRightSource, USceneComponent* HandLeftSource, USceneComponent* GroundSource)
{
	HeadComponent = HeadSource;
	HandRightComponent = HandRightSource;
	HandLeftComponent = HandLeftSource;
	GroundRefComponent = GroundSource;
	return Initialize();
}

bool UYnnkVrAvatarComponent::Initialize()
{
	AActor* Owner = GetOwner();

	WalkingSpeedHistory.Init(0.f, 10);
	WalkingSpeedHistoryIndex = 0;
	WalkingSpeed = WalkingSpeedMean = WalkingYaw = 0.f;

	HeadVelocityHistory.Init(FVector::ZeroVector, 30);
	HeadVelocityIndex = 0;
	HeadVelocity2D = FVector::ZeroVector;

	TorsoYawByHands.Init(FVector::RightVector, ProceduralTorsoRotationSmoothFrames);
	TorsoYawByHandsIndex = 0;
	TorsoYawByHandsSum = FVector::ZeroVector;
	TorsoYawByHandsMean = FVector::ForwardVector;
	SavedPelvisYaw = 0.f;
	LastPelvisYaw = FRotator::ZeroRotator;
	LastPelvisYawBlendStart = FRotator::ZeroRotator;
	SavedBodyLoc_UpdateTime = 0.f;

	TSet<UActorComponent*> OtherComps = Owner->GetComponents();

	for (auto& Comp : OtherComps)
	{
		if (!Comp->IsA(USceneComponent::StaticClass()))
		{
			continue;
		}

		FName CompName = Comp->GetFName();
		if (CompName == CameraComponentName && !IsValid(HeadComponent))
		{
			HeadComponent = Cast<USceneComponent>(Comp);
		}
		else if (CompName == HandRightComponentName && !IsValid(HandRightComponent))
		{
			HandRightComponent = Cast<USceneComponent>(Comp);
		}
		else if (CompName == HandLeftComponentName && !IsValid(HandLeftComponent))
		{
			HandLeftComponent = Cast<USceneComponent>(Comp);
		}
		else if (CompName == GroundComponentName && !IsValid(GroundRefComponent))
		{
			GroundRefComponent = Cast<USceneComponent>(Comp);
		}
		else if (CompName == AvatarSkeletalMeshName && Comp->IsA(USkeletalMeshComponent::StaticClass()))
		{
			BodyMesh = Cast<USkeletalMeshComponent>(Comp);
		}
	}

	if (!IsValid(GroundRefComponent) && IsValid(HeadComponent))
	{
		GroundRefComponent = HeadComponent->GetAttachParent();
	}

	if (!IsValid(HeadComponent) || !IsValid(HandRightComponent) || !IsValid(HandLeftComponent) || !IsValid(GroundRefComponent) || !IsValid(BodyMesh))
	{
		UE_LOG(LogYnnk, Warning, TEXT("Can't initialize refenrences to input components (HMD and controllers), skeletal mesh or ground component"));
		return false;
	}

	if (!IsValid(CurveStepHeight)
		|| !IsValid(CurveStepPitch)
		|| !IsValid(CurveStepOffset)
		|| !IsValid(CurveRibcagePitch)
		|| !IsValid(CurveRibcageRoll)
		|| !IsValid(CurveSpinePitch)
		|| !IsValid(CurvePelvisPitch))
	{
		UE_LOG(LogYnnk, Warning, TEXT("Can't initialize. IK curves are invalid."));
		return false;
	}

	if (ElbowModel == EYnnkElbowModelSource::YEM_NeuralNet)
	{
		FString ModelFile = GetResourcesPath() / TEXT("elbowsmodel.keras");
		if (!FPaths::FileExists(ModelFile))
		{
			ModelFile = GetResourcesPath() / TEXT("elbowsmodel_editor.keras");
		}
		KerasElbowModel = UKerasElbow::CreateKerasElbowModel(ModelFile, false);
	}

	bInitialized = BuildSkeletalModel();
	
	if (bInitialized)
	{
		BodyMesh->AddTickPrerequisiteComponent(this);
		LastRefTransform = GroundRefComponent->GetComponentTransform();
	}
	else
	{
		UE_LOG(LogYnnk, Warning, TEXT("Can't initialize skeleton data"));
	}

	return bInitialized;
}

void UYnnkVrAvatarComponent::SetDefaultHandOffsets(EMotionControllerModel ControllerModel)
{
#if ENGINE_MINOR_VERSION < 3
	// Relative to RightAim/LeftAim

	switch (ControllerModel)
	{
		case EMotionControllerModel::MC_ViveController:
			HandRightOffset = FTransform(FRotator(-25.f, 33.1f, 53.1f), FVector(-15.2f, -1.f, 4.6f));
			break;
		case EMotionControllerModel::MC_ValveIndex:
			HandRightOffset = FTransform(FRotator(1.7f, 28.4f, 65.9f), FVector(-16.5f, 0.2f, -6.8f));
			break;
		case EMotionControllerModel::MC_OculusTouch:
			HandRightOffset = FTransform(FRotator(-22.8f, 17.3f, 66.7f), FVector(-17.2f, 1.3f, 2.7f));
			break;
		case EMotionControllerModel::MC_ViveCosmos:
			HandRightOffset = FTransform(FRotator(-25.f, 33.1f, 53.1f), FVector(-15.2f, -1.f, 4.6f));
			break;
		case EMotionControllerModel::MC_Pico4:
			HandRightOffset = FTransform(FRotator(1.7f, 15.7f, 80.1f), FVector(-9.2f, 2.2f, -3.2f));
			break;
	}
#else
	switch (ControllerModel)
	{
		case EMotionControllerModel::MC_ViveController:
			HandRightOffset = FTransform(FRotator(-25.f, 33.1f, 53.1f), FVector(-15.2f, -1.f, 4.6f));
			break;
		case EMotionControllerModel::MC_ValveIndex:
			HandRightOffset = FTransform(FRotator(-52.651124f, 28.85f, 60.136882f), FVector(-6.309f, 1.433f, 6.736f));
			break;
		case EMotionControllerModel::MC_OculusTouch:
			HandRightOffset = FTransform(FRotator(-22.8f, 17.3f, 66.7f), FVector(-17.2f, 1.3f, 2.7f));
			break;
		case EMotionControllerModel::MC_ViveCosmos:
			HandRightOffset = FTransform(FRotator(-25.f, 33.1f, 53.1f), FVector(-15.2f, -1.f, 4.6f));
			break;
		case EMotionControllerModel::MC_Pico4:
			HandRightOffset = FTransform(FRotator(1.7f, 15.7f, 80.1f), FVector(-9.2f, 2.2f, -3.2f));
			break;
	}
#endif

	HandLeftOffset.SetTranslation(FVector(HandRightOffset.GetTranslation().X, -HandRightOffset.GetTranslation().Y, HandRightOffset.GetTranslation().Z));
	HandLeftOffset.SetRotation(FRotator(HandRightOffset.Rotator().Pitch, -HandRightOffset.Rotator().Yaw, -HandRightOffset.Rotator().Roll).Quaternion());

	/*
	HandRightOffset.AddToTranslation(FVector(-12.f, 0.f, 0.f));
	HandLeftOffset.AddToTranslation(FVector(-12.f, 0.f, 0.f));
	*/
}

void UYnnkVrAvatarComponent::ClientSetComponent_Implementation(uint8 Component, USceneComponent* NewTarget)
{
	switch (Component)
	{
		case 0: HeadComponent = NewTarget; break;
		case 1: HandRightComponent = NewTarget; break;
		case 2: HandLeftComponent = NewTarget; break;
	}
}

void UYnnkVrAvatarComponent::SetHeadComponent(USceneComponent* NewHeadTarget)
{
	HeadComponent = NewHeadTarget;

	if (GetIsReplicated())
	{
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		if (PawnOwner && !PawnOwner->IsLocallyControlled())
		{
			ClientSetComponent(0, NewHeadTarget);
		}
	}
}

void UYnnkVrAvatarComponent::SetHandRightComponent(USceneComponent* NewHandTarget)
{
	HandRightComponent = NewHandTarget;

	if (GetIsReplicated())
	{
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		if (PawnOwner && !PawnOwner->IsLocallyControlled())
		{
			ClientSetComponent(1, NewHandTarget);
		}
	}
}

void UYnnkVrAvatarComponent::SetHandLeftComponent(USceneComponent* NewHandTarget)
{
	HandLeftComponent = NewHandTarget;

	if (GetIsReplicated())
	{
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		if (PawnOwner && !PawnOwner->IsLocallyControlled())
		{
			ClientSetComponent(2, NewHandTarget);
		}
	}
}

void UYnnkVrAvatarComponent::SetBodySizeManually(float Height, float ArmSpan)
{
	if (MeshSize.IsZero())
	{
		MeshSize = FVector::OneVector;
	}

	// save current player size
	PlayerArmSpan = ArmSpan;
	PlayerHeight = Height;

	// update skeletal mesh scale
	MeshScale.X = PlayerArmSpan / MeshSize.X;
	MeshScale.Z = PlayerHeight / MeshSize.Z;

	// update sizes of bones
	BoneSizeScaled.UpperarmLength = BoneSize.UpperarmLength * MeshScale.X;
	BoneSizeScaled.LowerarmLength = BoneSize.LowerarmLength * MeshScale.X;
	BoneSizeScaled.ThighLength = BoneSize.ThighLength * MeshScale.Z;
	BoneSizeScaled.CalfLength = BoneSize.CalfLength * MeshScale.Z;
}

void UYnnkVrAvatarComponent::DrawGenericSkeletonCoordinates()
{
	if (!bInitialized)
	{
		return;
	}
	const FReferenceSkeleton& RefSkeleton = __uev_GetRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	UWorld* World = GetWorld();
	for (int32 BoneIndex = 0; BoneIndex < RefPoseSpaceBaseTMs.Num(); BoneIndex++)
	{
		if (!SkeletonTransforms.Contains(BoneIndex))
		{
			continue;
		}
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		const FTransform SkelTr = BodyMesh->GetSocketTransform(BoneName);
		const FTransform RevTr = FTransform().GetRelativeTransform(SkeletonTransforms[BoneIndex]);
		const FTransform GenericTr = RevTr * SkelTr;

		FVector Loc = GenericTr.GetTranslation();
		DrawDebugLine(World, Loc, Loc + GenericTr.GetRotation().GetForwardVector() * 15.f, FColor::Red, false, 0.05f, 0, 0.5f);
		DrawDebugLine(World, Loc, Loc + GenericTr.GetRotation().GetRightVector() * 15.f, FColor::Green, false, 0.05f, 0, 0.5f);
		DrawDebugLine(World, Loc, Loc + GenericTr.GetRotation().GetUpVector() * 15.f, FColor::Blue, false, 0.05f, 0, 0.5f);
	}
}

void UYnnkVrAvatarComponent::SetSpineCurvesBlendAlpha(float NewAlpha)
{
	if (IsValid(CurveRibcagePitch2) && IsValid(CurveSpinePitch2) && IsValid(CurvePelvisPitch2))
	{
		SpineCurvesAlpha = FMath::Clamp(NewAlpha, 0.f, 1.f);
	}
	else
	{
		SpineCurvesAlpha = 0.f;
	}
}

void UYnnkVrAvatarComponent::UpdateIKTargets(float DeltaTime)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	// Non-pawn or pawn with local controller
	bool bLocallyControlled = !OwnerPawn || OwnerPawn->IsLocallyControlled();
	bool bBodyIsReady = false;

	// Load from server?
	const FTransform SubBase = GroundRefComponent->GetComponentTransform();

	if (GetIsReplicated() && !bLocallyControlled)
	{
		if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
		{
			NetworkInterpTransform(IKTargets.Head, IKTargetsRep.Head.AsTransform());
			NetworkInterpTransform(IKTargets.HandRight, IKTargetsRep.HandRight.AsTransform());
			NetworkInterpTransform(IKTargets.HandLeft, IKTargetsRep.HandLeft.AsTransform());
			NetworkInterpTransform(IKTargets.Pelvis, IKTargetsRep.Pelvis.AsTransform());
			NetworkInterpTransform(IKTargets.FootRight, IKTargetsRep.FootRight.AsTransform());
			NetworkInterpTransform(IKTargets.FootLeft, IKTargetsRep.FootLeft.AsTransform());
			bBodyIsReady = true;
		}
		else
		{
			// first, convert HMD to head
			FTransform HeadTarget = IKInputsRep.HMD.AsTransform();
			HeadTarget.AddToTranslation(HeadTarget.GetRotation().RotateVector(HeadJointOffsetFromHMD));
			NetworkInterpTransform(IKTargets.Head, HeadTarget);
			NetworkInterpTransform(IKTargets.HandRight, IKInputsRep.HandRight.AsTransform());
			NetworkInterpTransform(IKTargets.HandLeft, IKInputsRep.HandLeft.AsTransform());
		}
	}
	else // get HMD, ControllerRight, ControllerLeft from input components
	{		
		FTransform RawHead = HeadComponent->GetComponentTransform().GetRelativeTransform(SubBase);
		FTransform HandTargetRight = GetHandTargetTransform(true, false);
		FTransform HandTargetLeft = GetHandTargetTransform(false, false);

		// set default positions for non-tracked HMD and controllers
		if (HeadComponent->GetRelativeLocation().IsNearlyZero())
		{
			RawHead = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, PlayerHeight));
		}
		if (HandRightComponent->GetRelativeLocation().IsNearlyZero())
		{
			FTransform HeadVertical = FTransform(FRotator(0.f, RawHead.Rotator().Yaw, 0.f), RawHead.GetTranslation());
			FTransform RawControllerRight = FTransform(FRotator(15.f, -30.f, -60.f), FVector(25.f, 45.f, -40.f));
			RawControllerRight *= HeadVertical;
			HandTargetRight = RawControllerRight;
		}
		if (HandLeftComponent->GetRelativeLocation().IsNearlyZero())
		{
			FTransform HeadVertical = FTransform(FRotator(0.f, RawHead.Rotator().Yaw, 0.f), RawHead.GetTranslation());
			FTransform RawControllerLeft = FTransform(FRotator(15.f, 30.f, 60.f), FVector(25.f, -45.f, -40.f));
			RawControllerLeft *= HeadVertical;
			HandTargetLeft = RawControllerLeft;
		}

		// Save input transforms (head and hands)
		IKTargets.Head = RawHead;
		IKTargets.Head.AddToTranslation(IKTargets.Head.GetRotation().RotateVector(HeadJointOffsetFromHMD));
		IKTargets.HandRight = HandRightOffset * HandTargetRight;
		IKTargets.HandLeft = HandLeftOffset * HandTargetLeft;
	}

	// Here we have input IK targets ready. It's time to build remaining data

	// Velocities
	UpdateVelocitiesInternal(DeltaTime);

	// Torso and feet IK targets
	if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
	{
		if (!bBodyIsReady)
		{
			BuildBodyInternal_YNN(DeltaTime);
		}
	}
	else
	{
		BuildBodyInternal_Procedural(DeltaTime);
	}

	// Trace positions of animated feet bones to adjust vertical coordinate
	// FootRight_Location and FootLeft_Location should be applied in animation blueprint
	if (bBlendLegsAnimation)
	{
		FTransform tFootRight = BodyMesh->GetSocketTransform(FootRightVirtualBoneName);
		FTransform tFootLeft = BodyMesh->GetSocketTransform(FootLeftVirtualBoneName);

		FTransform NewFootRight = TestFootToGround(tFootRight, true, true);
		FTransform NewFootLeft = TestFootToGround(tFootLeft, false, true);

		FootRight_Location = NewFootRight.Equals(tFootRight)
			? FVector::ZeroVector
			: (NewFootRight * SubBase).GetTranslation();
		FootLeft_Location = NewFootLeft.Equals(tFootLeft)
			? FVector::ZeroVector
			: (NewFootLeft * SubBase).GetTranslation();
	}

	// Send to server?
	if (GetIsReplicated() && bLocallyControlled)
	{
		float CurrentTime = GetWorld()->GetRealTimeSeconds();
		if (ServerUpdateInterval == 0.f || CurrentTime >= NextServerUpdateTime)
		{
			NextServerUpdateTime = CurrentTime + ServerUpdateInterval;
			if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
			{
				IKTargetsRep.Head = FNetworkTransform(IKTargets.Head);
				IKTargetsRep.HandRight = FNetworkTransform(IKTargets.HandRight);
				IKTargetsRep.HandLeft = FNetworkTransform(IKTargets.HandLeft);
				IKTargetsRep.Pelvis = FNetworkTransform(IKTargets.Pelvis);
				IKTargetsRep.FootRight = FNetworkTransform(IKTargets.FootRight);
				IKTargetsRep.FootLeft = FNetworkTransform(IKTargets.FootLeft);
				ServerSendIK(IKTargetsRep);
			}
			else
			{
				IKInputsRep.HMD = GetHMDTransform(false);
				IKInputsRep.HandRight = IKTargets.HandRight;
				IKInputsRep.HandLeft = IKTargets.HandLeft;
				ServerSendIKInputs(IKInputsRep);
			}
		}
	}
}

/**
* Save indices of bones and local transforms relative to generic skeleton transforms equal to NN generic model
* Out: xxIndex variables, SkeletonTransforms
*/
bool UYnnkVrAvatarComponent::BuildSkeletalModel()
{
	if (!__uev_GetSkeletalMesh(BodyMesh))
	{
		return false;
	}

	// at first restore bone names

	const FReferenceSkeleton& RefSkeleton = __uev_GetRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	BonesActorSpace.SetNum(RefPoseSpaceBaseTMs.Num());
	CapturedSkeletonR.SetNumUninitialized(3);
	CapturedSkeletonL.SetNumUninitialized(3);

	int32 RightIndex, LeftIndex;
	int32 ChainLength = 0;

	HandRightIndex = RefSkeleton.FindBoneIndex(HandRightBoneName);
	HandLeftIndex = RefSkeleton.FindBoneIndex(HandLeftBoneName);
	if (HandRightIndex == INDEX_NONE || HandLeftIndex == INDEX_NONE)
	{
		return false;
	}

	RightIndex = HandRightIndex; LeftIndex = HandLeftIndex;
	while (RightIndex != INDEX_NONE && LeftIndex != INDEX_NONE)
	{
		ChainLength++;

		RightIndex = RefSkeleton.GetParentIndex(RightIndex);
		LeftIndex = RefSkeleton.GetParentIndex(LeftIndex);

		if (RightIndex == INDEX_NONE || LeftIndex == INDEX_NONE)
		{
			return false;
		}
		if (RightIndex == LeftIndex)
		{
			RibcageIndex = RightIndex;
			break;
		}
	}

	if (ChainLength < 5)
	{
		LowerarmRightIndex = RefSkeleton.GetParentIndex(HandRightIndex);
		UpperarmRightIndex = RefSkeleton.GetParentIndex(LowerarmRightIndex);
		ClavicleRightIndex = RefSkeleton.GetParentIndex(UpperarmRightIndex);

		LowerarmLeftIndex = RefSkeleton.GetParentIndex(HandLeftIndex);
		UpperarmLeftIndex = RefSkeleton.GetParentIndex(LowerarmLeftIndex);
		ClavicleLeftIndex = RefSkeleton.GetParentIndex(UpperarmLeftIndex);
	}
	else
	{
		LowerarmRightIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(HandRightIndex));
		UpperarmRightIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(LowerarmRightIndex));
		ClavicleRightIndex = RefSkeleton.GetParentIndex(UpperarmRightIndex);

		LowerarmLeftIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(HandLeftIndex));
		UpperarmLeftIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(LowerarmLeftIndex));
		ClavicleLeftIndex = RefSkeleton.GetParentIndex(UpperarmLeftIndex);
	}

	// legs
	ChainLength = 0;
	FootRightIndex = RefSkeleton.FindBoneIndex(FootRightBoneName);
	FootLeftIndex = RefSkeleton.FindBoneIndex(FootLeftBoneName);

	int32 Counter = 0;
	if (PelvisBoneName.IsNone() || RefSkeleton.FindBoneIndex(PelvisBoneName) == INDEX_NONE)
	{
		RightIndex = FootRightIndex; LeftIndex = FootLeftIndex;
		while (RightIndex != INDEX_NONE && LeftIndex != INDEX_NONE)
		{
			ChainLength++;
			RightIndex = RefSkeleton.GetParentIndex(RightIndex);
			LeftIndex = RefSkeleton.GetParentIndex(LeftIndex);

			if (RightIndex == INDEX_NONE || LeftIndex == INDEX_NONE) return false;
			if (RightIndex == LeftIndex)
			{
				PelvisIndex = RightIndex;
				break;
			}
			if (Counter++ > 0xff) return false;
		}
	}
	else
	{
		PelvisIndex = RefSkeleton.FindBoneIndex(PelvisBoneName);
	}

	if (ChainLength < 5)
	{
		CalfRightIndex = RefSkeleton.GetParentIndex(FootRightIndex);
		ThighRightIndex = RefSkeleton.GetParentIndex(CalfRightIndex);
		CalfLeftIndex = RefSkeleton.GetParentIndex(FootLeftIndex);
		ThighLeftIndex = RefSkeleton.GetParentIndex(CalfLeftIndex);
	}
	else
	{
		CalfRightIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(FootRightIndex));
		ThighRightIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(CalfRightIndex));
		CalfLeftIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(FootLeftIndex));
		ThighLeftIndex = RefSkeleton.GetParentIndex(RefSkeleton.GetParentIndex(CalfLeftIndex));
	}

	HeadIndex = RefSkeleton.FindBoneIndex(HeadBoneName);
	if (HeadIndex == INDEX_NONE)
	{
		return false;
	}

	// And now transforms
	// Y axis is forward
	FTransform CompSpace = FTransform::Identity;
	CompSpace.SetRotation(FRotator(0.f, 90.f, 0.f).Quaternion());
	const FVector ComponentForward = CompSpace.GetRotation().GetForwardVector();

	FTransform t, tRel, t2;

	// Pelvis
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, PelvisIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(CompSpace);
	SkeletonTransforms.Add(PelvisIndex, tRel);

	// Head
	const FTransform HeadCS = YnnkHelpers::RestoreRefBonePose(RefSkeleton, HeadIndex);
	t = HeadCS;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(CompSpace);
	SkeletonTransforms.Add(HeadIndex, tRel);

	// Ribcage

	// offset of real ribcage bone relative to generic ribcage (between clavicles with component-like orientation)
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleRightIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleLeftIndex);
	t = FTransform(CompSpace.GetRotation(), (t.GetTranslation() + t2.GetTranslation()) * 0.5f);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, RibcageIndex);
	RibcageBoneOffsetFromJoint = t2.GetRelativeTransform(t);

	RibageOffsetFromHeadJoint = t.GetTranslation() - HeadCS.GetTranslation();
	// x should be forward
	if (FMath::IsNearlyZero(RibageOffsetFromHeadJoint.X, 0.001f))
	{
		RibageOffsetFromHeadJoint.X = RibageOffsetFromHeadJoint.Y;
		RibageOffsetFromHeadJoint.Y = 0.f;
	}

	// (and head again)
	// head joint
	t.AddToTranslation(RibageOffsetFromHeadJoint * -1.f);
	// head bone relative to ribcage joint
	HeadBoneOffsetfromJoint = HeadCS.GetRelativeTransform(t);

	// Right Leg

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootRightIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighRightIndex);
	FTransform RightLegSpace = FTransform(UKismetMathLibrary::MakeRotFromXY(t.GetTranslation() - t2.GetTranslation(), ComponentForward).Quaternion());
	FTransform RightFootSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(ComponentForward, t2.GetTranslation() - t.GetTranslation()).Quaternion());

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootRightIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(RightFootSpace);
	SkeletonTransforms.Add(FootRightIndex, tRel);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, CalfRightIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(RightLegSpace);
	SkeletonTransforms.Add(CalfRightIndex, tRel);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighRightIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(RightLegSpace);
	SkeletonTransforms.Add(ThighRightIndex, tRel);

	// Left Leg

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootLeftIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighLeftIndex);
	FTransform LeftLegSpace = FTransform(UKismetMathLibrary::MakeRotFromXY(t.GetTranslation() - t2.GetTranslation(), ComponentForward).Quaternion());
	FTransform LeftFootSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(ComponentForward, t2.GetTranslation() - t.GetTranslation()).Quaternion());

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootLeftIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(LeftFootSpace);
	SkeletonTransforms.Add(FootLeftIndex, tRel);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, CalfLeftIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(LeftLegSpace);
	SkeletonTransforms.Add(CalfLeftIndex, tRel);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighLeftIndex);
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(LeftLegSpace);
	SkeletonTransforms.Add(ThighLeftIndex, tRel);

	// spine right
	// vRight = vUp ^ vForward
	// Right Arm

	FTransform hand_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, HandRightIndex);
	FTransform lowerarm_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, LowerarmRightIndex);
	FTransform upperarm_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, UpperarmRightIndex);
	FTransform clavicle_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleRightIndex);

	MeshSize.X = (FVector::Distance(hand_tr.GetTranslation(), lowerarm_tr.GetTranslation())
		+ FVector::Distance(lowerarm_tr.GetTranslation(), upperarm_tr.GetTranslation())
		+ FVector::Distance(upperarm_tr.GetTranslation(), clavicle_tr.GetTranslation())) * 2.f;

	FVector HandForward = (hand_tr.GetTranslation() - lowerarm_tr.GetTranslation()).GetSafeNormal();
	FVector HandUp = ComponentForward ^ HandForward;
	FTransform HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = hand_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(HandRightIndex, tRel);

	t = lowerarm_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(LowerarmRightIndex, tRel);

	HandForward = (lowerarm_tr.GetTranslation() - upperarm_tr.GetTranslation()).GetSafeNormal();
	HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = upperarm_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(UpperarmRightIndex, tRel);

	HandForward = (upperarm_tr.GetTranslation() - clavicle_tr.GetTranslation()).GetSafeNormal();
	HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = clavicle_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(ClavicleRightIndex, tRel);

	// Left Arm

	hand_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, HandLeftIndex);
	lowerarm_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, LowerarmLeftIndex);
	upperarm_tr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, UpperarmLeftIndex);
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleLeftIndex);
	MeshSize.X += FVector::Dist(t.GetTranslation(), clavicle_tr.GetTranslation());
	clavicle_tr = t;

	HandForward = (hand_tr.GetTranslation() - lowerarm_tr.GetTranslation()).GetSafeNormal();
	HandUp = HandForward ^ ComponentForward;
	HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = hand_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(HandLeftIndex, tRel);

	t = lowerarm_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(LowerarmLeftIndex, tRel);

	HandForward = (lowerarm_tr.GetTranslation() - upperarm_tr.GetTranslation()).GetSafeNormal();
	HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = upperarm_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(UpperarmLeftIndex, tRel);

	HandForward = (upperarm_tr.GetTranslation() - clavicle_tr.GetTranslation()).GetSafeNormal();
	HandSpace = FTransform(UKismetMathLibrary::MakeRotFromXZ(HandForward, HandUp).Quaternion());

	t = clavicle_tr;
	t.SetTranslation(FVector::ZeroVector);
	tRel = t.GetRelativeTransform(HandSpace);
	SkeletonTransforms.Add(ClavicleLeftIndex, tRel);

	// Spine

	int32 BoneIndex = RefSkeleton.GetParentIndex(HeadIndex);
	const FTransform HeadBoneTr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, HeadIndex);
	FVector PrevBoneLoc = HeadBoneTr.GetTranslation();

	const FVector HeadBoneLoc = PrevBoneLoc;
	const FVector PelvisBoneLoc = YnnkHelpers::RestoreRefBonePose(RefSkeleton, PelvisIndex).GetTranslation();
	const FVector RibcageBoneLoc = YnnkHelpers::RestoreRefBonePose(RefSkeleton, RibcageIndex).GetTranslation();

	const FVector SpineDirection = (RibcageBoneLoc - PelvisBoneLoc).GetSafeNormal();
	const FRotator SpineRotator = UKismetMathLibrary::MakeRotFromZX(SpineDirection, ComponentForward);
	const FVector NeckDirection = (HeadBoneLoc - RibcageBoneLoc).GetSafeNormal();
	const FRotator NeckRotator = UKismetMathLibrary::MakeRotFromZX(NeckDirection, ComponentForward);

	SpineLength = FVector::DistSquared(RibcageBoneLoc, PelvisBoneLoc);
	NeckLength = FVector::DistSquared(HeadBoneLoc, RibcageBoneLoc);

	float CoveredDistance = 0.f;
	Counter = 0;

	FSpineBonePreset FirstSpineBone;
	FirstSpineBone.VerticalDistance = 0.f;
	FirstSpineBone.BoneOffset = HeadBoneTr.GetRelativeTransform(FTransform(CompSpace.GetRotation(), HeadBoneTr.GetTranslation()));
	SpineBoneIndices.Add(HeadIndex);
	SpineBones.Add(HeadIndex, FirstSpineBone);

	FTransform Ribcage_GS;

	do
	{
		FVector VerticalVector, RootLoc, EffLoc;
		FRotator VertRot;
		if (BoneIndex >= RibcageIndex)
		{
			VerticalVector = NeckDirection; VertRot = NeckRotator;
			RootLoc = RibcageBoneLoc; EffLoc = HeadBoneLoc;
		}
		else
		{
			VerticalVector = SpineDirection; VertRot = SpineRotator;
			RootLoc = PelvisBoneLoc; EffLoc = RibcageBoneLoc;
		}

		const FTransform NewBoneTr = YnnkHelpers::RestoreRefBonePose(RefSkeleton, BoneIndex);
		FVector Projected = (NewBoneTr.GetTranslation() - RootLoc).ProjectOnTo(VerticalVector) + RootLoc;

		FSpineBonePreset NextBone;
		NextBone.VerticalDistance = FVector::Distance(EffLoc, Projected) - CoveredDistance;
		CoveredDistance += NextBone.VerticalDistance;

		NextBone.BoneOffset = NewBoneTr.GetRelativeTransform(FTransform(VertRot, Projected));

		if (BoneIndex == RibcageIndex)
		{
			Ribcage_GS = FTransform(VertRot, Projected);
			SpineBones_RibcageIndex = SpineBoneIndices.Num();
			NeckLength = CoveredDistance;
			CoveredDistance = 0.f;
		}
		
		SpineBoneIndices.Add(BoneIndex);
		SpineBones.Add(BoneIndex, NextBone);
		SkeletonTransforms.Add(BoneIndex, NextBone.BoneOffset);

		PrevBoneLoc = NewBoneTr.GetTranslation();
		BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (Counter++ > 0xff) return false;
	} while (BoneIndex >= PelvisIndex);
	SpineLength = CoveredDistance;

	for (const auto& sBone : SpineBones)
	{
		UE_LOG(LogYnnk, Log, TEXT("Spine bone: %d [%s] offset %f relative transform %s"), sBone.Key, *RefSkeleton.GetBoneName(sBone.Key).ToString(), sBone.Value.VerticalDistance, *sBone.Value.BoneOffset.ToString());
	}

	// offsets for clavicles
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleRightIndex);
	FTransform clavicle_r_gs = FTransform().GetRelativeTransform(SkeletonTransforms[ClavicleRightIndex]) * t; // convert to generic skeleton space
	ClavicleRightToRibcageGen = clavicle_r_gs.GetRelativeTransform(Ribcage_GS);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ClavicleLeftIndex);
	FTransform clavicle_l_gs = FTransform().GetRelativeTransform(SkeletonTransforms[ClavicleLeftIndex]) * t; // convert to generic skeleton space
	ClavicleLeftToRibcageGen = clavicle_l_gs.GetRelativeTransform(Ribcage_GS);

	// offsets for upperarms
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, UpperarmRightIndex);
	t = FTransform().GetRelativeTransform(SkeletonTransforms[UpperarmRightIndex]) * t; // convert to generic skeleton space
	UpperarmRightToClavicleGen = t.GetRelativeTransform(clavicle_r_gs);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, UpperarmLeftIndex);
	t = FTransform().GetRelativeTransform(SkeletonTransforms[UpperarmLeftIndex]) * t; // convert to generic skeleton space
	UpperarmLeftToClavicleGen = t.GetRelativeTransform(clavicle_l_gs);

	// offsets for legs
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, PelvisIndex);
	FTransform pelvis_gs = FTransform().GetRelativeTransform(SkeletonTransforms[PelvisIndex]) * t;

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighRightIndex);
	t = FTransform().GetRelativeTransform(SkeletonTransforms[ThighRightIndex]) * t; // convert to generic skeleton space
	ThighRightToPelvisGen = t.GetRelativeTransform(pelvis_gs);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighLeftIndex);
	t = FTransform().GetRelativeTransform(SkeletonTransforms[ThighLeftIndex]) * t; // convert to generic skeleton space
	ThighLeftToPelvisGen = t.GetRelativeTransform(pelvis_gs);

	// offset for feet

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootRightIndex);
	FootBoneHeight = (float)t.GetTranslation().Z;

	t = FTransform().GetRelativeTransform(SkeletonTransforms[FootRightIndex]) * t;
	FTransform t3 = t * BodyMesh->GetComponentTransform();
	FootRightToFloorGen = FTransform(CompSpace.GetRotation(), FVector(t.GetTranslation().X, t.GetTranslation().Y, 0.f));
	FootRightToFloorGen = FootRightToFloorGen.GetRelativeTransform(t);

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootLeftIndex);
	t = FTransform().GetRelativeTransform(SkeletonTransforms[FootLeftIndex]) * t;
	FootLeftToFloorGen = FTransform(CompSpace.GetRotation(), FVector(t.GetTranslation().X, t.GetTranslation().Y, 0.f));
	FootLeftToFloorGen = FootLeftToFloorGen.GetRelativeTransform(t);

	// Size of bones

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, UpperarmRightIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, LowerarmRightIndex);
	BoneSize.UpperarmLength = FVector::Dist(t.GetTranslation(), t2.GetTranslation());
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, HandRightIndex);
	BoneSize.LowerarmLength = FVector::Dist(t.GetTranslation(), t2.GetTranslation());

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighRightIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, CalfRightIndex);
	BoneSize.ThighLength = FVector::Dist(t.GetTranslation(), t2.GetTranslation());
	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootRightIndex);
	BoneSize.CalfLength = FVector::Dist(t.GetTranslation(), t2.GetTranslation());

	t = YnnkHelpers::RestoreRefBonePose(RefSkeleton, ThighRightIndex);
	t2 = YnnkHelpers::RestoreRefBonePose(RefSkeleton, FootRightIndex);
	MeshSize.Z = (HeadCS.GetTranslation().Z + FMath::Abs(HeadJointOffsetFromHMD.Z)) - t.GetTranslation().Z + BoneSize.ThighLength + BoneSize.CalfLength + t2.GetTranslation().Z;

	// Initialize skeleton scale settings for default/saved player sizes
	SetBodySizeManually(PlayerHeight, PlayerArmSpan);

	return true;
}

// Calculate spine bones between pelvis and ribcage by cubic bezier curve
// The coefficient of length variable (SpineBendingMultiplier = .2f) is empirical.
FTransform UYnnkVrAvatarComponent::CalculateBezierSpine(const FTransform& LowerTr, const FTransform& UpperTr, int32 LowerIndex, int32 UpperIndex, float CurveLength, bool bDrawDebugGeometry)
{
	TArray<FVector> ResultPoints;
	TArray<FVector> Points;
	// Pelvis, spine, head
	int32 LastInd = LowerIndex - UpperIndex;
	Points.SetNum(4);

	// start and end
	Points[0] = UpperTr.GetTranslation();
	Points[3] = LowerTr.GetTranslation();

	// direction points
	const float length = FVector::Dist(LowerTr.GetTranslation(), UpperTr.GetTranslation()) * SpineBendingMultiplier;

	const FVector dir = (Points[0] - Points[3]).GetSafeNormal();
	const FVector UpDir = LowerTr.GetRotation().GetUpVector();// FMath::Lerp<FVector>(LowerTr.GetRotation().GetUpVector(), dir, 0.1f).GetSafeNormal();
	const FVector DownDir = FMath::Lerp<FVector>(-UpperTr.GetRotation().GetUpVector(), -dir, 0.1f).GetSafeNormal();
	Points[1] = Points[0] + DownDir * length * 0.5f;
	Points[2] = Points[3] + UpDir * length;

	// number of points to calculate (including start and end)
	FVector::EvaluateBezier(Points.GetData(), LastInd + 1, ResultPoints);

	float BezierCurveLength = 0.f;
	for (int32 i = 1; i < ResultPoints.Num(); i++)
	{
		BezierCurveLength += FVector::Dist(ResultPoints[i - 1], ResultPoints[i]);
	}

	const FRotator SpineRot = UKismetMathLibrary::MakeRotFromZX(dir, LowerTr.GetRotation().GetForwardVector());
	// apply known transform
	BonesActorSpace[SpineBoneIndices[UpperIndex]] = UpperTr;

	if (bDrawDebugGeometry)
	{
		DrawDebugLine(GetWorld(), UpperTr.GetTranslation(), UpperTr.GetTranslation() + UpperTr.GetRotation().GetForwardVector() * 20.f, FColor::Red, false, 0.1f, 0, 1.f);
		DrawDebugLine(GetWorld(), UpperTr.GetTranslation(), UpperTr.GetTranslation() + UpperTr.GetRotation().GetRightVector() * 20.f, FColor::Green, false, 0.1f, 0, 1.f);
		DrawDebugLine(GetWorld(), UpperTr.GetTranslation(), UpperTr.GetTranslation() + UpperTr.GetRotation().GetUpVector() * 5.f, FColor::Blue, false, 0.1f, 0, 1.f);
	}

	const FVector RightVectorS = UpperTr.GetRotation().GetRightVector();
	const FVector RightVectorE = LowerTr.GetRotation().GetRightVector();

	// skip upper bone
	float LengthCovered = 0.f;
	int32 CurvePoint = 0;
	FVector PrevLocation = UpperTr.GetTranslation();
	FTransform OutTr;
	for (int32 i = UpperIndex + 1; i <= LowerIndex; i++)
	{
		CurvePoint++;

		int32 PrevIndex = CurvePoint == 0 ? 0 : CurvePoint - 1;
		int32 NextIndex = CurvePoint < ResultPoints.Num() - 1 ? CurvePoint + 1 : CurvePoint;

		const int32 BoneIndex = SpineBoneIndices[i];
		const auto& BoneData = SpineBones[BoneIndex];

		LengthCovered += BoneData.VerticalDistance * MeshScale.Z;
		const float GlobalAlpha = LengthCovered / CurveLength;

		const FVector RightVec = FMath::Lerp(RightVectorS, RightVectorE, GlobalAlpha);
		const FVector UpVec = (ResultPoints[PrevIndex] - ResultPoints[CurvePoint]).GetSafeNormal();
		
		FRotator r = UKismetMathLibrary::MakeRotFromZY(UpVec, RightVec);

		FTransform TargetTr = FTransform(r, PrevLocation - UpVec * BoneData.VerticalDistance * MeshScale.Z);
		PrevLocation = TargetTr.GetTranslation();

		BonesActorSpace[BoneIndex] = TargetTr;
		if (i == LowerIndex) OutTr = TargetTr;

		if (bDrawDebugGeometry)
		{
			DrawDebugLine(GetWorld(), PrevLocation, PrevLocation + r.Quaternion().GetForwardVector() * 20.f, FColor::Red, false, 0.1f, 0, 1.f);
			DrawDebugLine(GetWorld(), PrevLocation, PrevLocation + r.Quaternion().GetRightVector() * 20.f, FColor::Green, false, 0.1f, 0, 1.f);
			DrawDebugLine(GetWorld(), PrevLocation, PrevLocation + UpVec * 5.f, FColor::Blue, false, 0.1f, 0, 1.f);
		}
	}

	return OutTr;
}

// Calculate spine bones between pelvis and ribcage by cubic bezier curve
// The coefficient of length variable (SpineBendingMultiplier = .2f) is empirical.
FTransform UYnnkVrAvatarComponent::CalculateNeck(const FTransform& LowerTr, const FTransform& UpperTr, int32 LowerIndex, int32 UpperIndex, bool bDrawDebugGeometry)
{
	const FReferenceSkeleton& RefSkeleton = __uev_GetRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));

	// apply known transform
	BonesActorSpace[SpineBoneIndices[LowerIndex]] = LowerTr;

	FTransform PrevPos = LowerTr;
	float LengthCovered = 0.f;
	for (int32 i = LowerIndex - 1; i >= UpperIndex; i--)
	{
		const int32 BoneIndex = SpineBoneIndices[i];
		const auto& BoneData = SpineBones[BoneIndex];
		const auto& PrevBoneData = SpineBones[SpineBoneIndices[i + 1]];
		LengthCovered += PrevBoneData.VerticalDistance;

		float Alpha = (LengthCovered / NeckLength);
		FQuat NewRot = FQuat::Slerp(LowerTr.GetRotation(), UpperTr.GetRotation(), Alpha);
		FVector NewLoc = PrevPos.GetTranslation() + PrevPos.GetRotation().GetUpVector() * PrevBoneData.VerticalDistance;

		PrevPos = FTransform(NewRot, NewLoc);
		BonesActorSpace[BoneIndex] = PrevPos;

		if (bDrawDebugGeometry)
		{
			FTransform NewPosWS = FTransform(NewRot, NewLoc) * GroundRefComponent->GetComponentTransform();
			DrawDebugLine(GetWorld(), NewPosWS.GetTranslation(), NewPosWS.GetTranslation() + NewPosWS.GetRotation().GetForwardVector() * 20.f, FColor::Red, false, 0.1f, 0, 1.f);
			DrawDebugLine(GetWorld(), NewPosWS.GetTranslation(), NewPosWS.GetTranslation() + NewPosWS.GetRotation().GetRightVector() * 20.f, FColor::Green, false, 0.1f, 0, 1.f);
			DrawDebugLine(GetWorld(), NewPosWS.GetTranslation(), NewPosWS.GetTranslation() + NewPosWS.GetRotation().GetUpVector() * 5.f, FColor::Blue, false, 0.1f, 0, 1.f);
		}
	}

	return LowerTr;
}

FTransform UYnnkVrAvatarComponent::CalculateNeckNew(const FTransform& Ribcage, const FTransform& Head, bool bRibacgeInBoneSpace, bool bApplyOffset)
{
	// head is always in generic space
	const float NeckRotationFraction = 0.2f;

	const FReferenceSkeleton& RefSkeleton = __uev_GetRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

	const int32 NeckIndex = SpineBoneIndices[1]; // 0 is head bone, 1 is first neck bone
	const FTransform RevSkeletonTransformRibcage = FTransform().GetRelativeTransform(SkeletonTransforms[RibcageIndex]);
	const FTransform RevSkeletonTransformNeck = FTransform().GetRelativeTransform(SkeletonTransforms[NeckIndex]);
	const FTransform RevSkeletonTransformHead = FTransform().GetRelativeTransform(SkeletonTransforms[HeadIndex]);

	// in bone space
	FTransform RibcageInBoneSpace;
	if (bRibacgeInBoneSpace)
	{
		RibcageInBoneSpace = Ribcage;
		// make generic space
		BonesActorSpace[RibcageIndex] = RevSkeletonTransformRibcage * Ribcage;
	}
	else
	{
		// convert to bone space
		RibcageInBoneSpace = SkeletonTransforms[RibcageIndex] * Ribcage;
		// save original gen-space value
		BonesActorSpace[RibcageIndex] = Ribcage;
	}

	// neck in bone space
	FTransform NewNeckTr = RefPoseSpaceBaseTMs[NeckIndex] * RibcageInBoneSpace;
	// head in generic space
	FTransform GenericHeadTr = RevSkeletonTransformHead * RefPoseSpaceBaseTMs[HeadIndex] * NewNeckTr;
	// neck in gen space
	NewNeckTr = RevSkeletonTransformNeck * NewNeckTr;
	// static head in gen space
	FRotator HeadDelta = UKismetMathLibrary::NormalizedDeltaRotator(Head.Rotator(), GenericHeadTr.Rotator()) * NeckRotationFraction;
	// add offset to neck in generic space
	NewNeckTr.SetRotation(UKismetMathLibrary::ComposeRotators(NewNeckTr.Rotator(), HeadDelta).Quaternion());

	// in generic space
	BonesActorSpace[NeckIndex] = NewNeckTr;

	// head in bone space
	BonesActorSpace[HeadIndex] = RefPoseSpaceBaseTMs[HeadIndex] * (SkeletonTransforms[NeckIndex] * BonesActorSpace[NeckIndex]);
	// apply gen-space rotation
	BonesActorSpace[HeadIndex].SetRotation(Head.GetRotation());

	// Make sure head location matches IK target
	// We don't do that after torso-to-hands correction.
	if (bApplyOffset)
	{
		FVector PositionError = Head.GetTranslation() - BonesActorSpace[HeadIndex].GetTranslation();
		BonesActorSpace[HeadIndex].AddToTranslation(PositionError);
		BonesActorSpace[NeckIndex].AddToTranslation(PositionError);
		BonesActorSpace[RibcageIndex].AddToTranslation(PositionError);
	}

	return BonesActorSpace[RibcageIndex];
}

void UYnnkVrAvatarComponent::CalcTorsoIK(bool bDebugDraw)
{
	const float ArmTwistRatio = 0.5f;
	const float MaxTwistDegrees = 90.f;
	const float MaxPitchBackwardDegrees = 10.f;
	const float MaxPitchForwardDegrees = 60.f;
	const float TorsoRotationSlerpSpeed = 13.f;

	FTransform Waist = BonesActorSpace[RibcageIndex];

	TArray<FTransform> PostIKTransformsLeft;
	TArray<FTransform> PostIKTransformsRight;

	if (bDebugDraw)
	{
		UWorld* World = GetWorld();
		DrawDebugLine(World, CapturedSkeletonR[0].GetTranslation(), CapturedSkeletonR[1].GetTranslation(), FColor::Blue, false, 0.05f, 0, 1.f);
		DrawDebugLine(World, CapturedSkeletonR[1].GetTranslation(), CapturedSkeletonR[2].GetTranslation(), FColor::Blue, false, 0.05f, 0, 2.f);

		DrawDebugLine(World, CapturedSkeletonL[0].GetTranslation(), CapturedSkeletonL[1].GetTranslation(), FColor::Blue, false, 0.05f, 0, 1.f);
		DrawDebugLine(World, CapturedSkeletonL[1].GetTranslation(), CapturedSkeletonL[2].GetTranslation(), FColor::Blue, false, 0.05f, 0, 2.f);
	}

	FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
		CapturedSkeletonL,
		IKTargets.HandLeft.GetTranslation(),
		PostIKTransformsLeft,
		100.f /*MaxShoulderDragDistance*/,
		1.f /*ShoulderDragStiffness*/,
		0.001f /*Precision*/,
		10 /*MaxIterations*/
	);

	FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
		CapturedSkeletonR,
		IKTargets.HandRight.GetTranslation(),
		PostIKTransformsRight,
		100.f /*MaxShoulderDragDistance*/,
		1.f /*ShoulderDragStiffness*/,
		0.001f /*Precision*/,
		10 /*MaxIterations*/
	);

	if (bDebugDraw)
	{
		UWorld* World = GetWorld();
		DrawDebugLine(World, PostIKTransformsRight[0].GetTranslation(), PostIKTransformsRight[1].GetTranslation(), FColor::Orange, false, 0.05f, 0, 2.f);
		DrawDebugLine(World, PostIKTransformsRight[1].GetTranslation(), PostIKTransformsRight[2].GetTranslation(), FColor::Orange, false, 0.05f, 0, 1.f);

		DrawDebugLine(World, PostIKTransformsLeft[0].GetTranslation(), PostIKTransformsLeft[1].GetTranslation(), FColor::Orange, false, 0.05f, 0, 2.f);
		DrawDebugLine(World, PostIKTransformsLeft[1].GetTranslation(), PostIKTransformsLeft[2].GetTranslation(), FColor::Orange, false, 0.05f, 0, 1.f);
	}

	// Use first pass results to twist the torso around the spine direction
	// Note --calculations here are relative to the waist bone, not root!
	// 'Neck' is the midpoint beween the shoulders
	FVector ShoulderLeftPreIK = CapturedSkeletonL[0].GetLocation() - Waist.GetLocation();
	FVector ShoulderRightPreIK = CapturedSkeletonR[0].GetLocation() - Waist.GetLocation();
	FVector NeckPreIK = (ShoulderLeftPreIK + ShoulderRightPreIK) / 2.f;
	FVector SpineDirection = NeckPreIK.GetUnsafeNormal();

	FVector ShoulderLeftPostIK = PostIKTransformsLeft[0].GetLocation() - Waist.GetLocation();
	FVector ShoulderRightPostIK = PostIKTransformsRight[0].GetLocation() - Waist.GetLocation();
	FVector NeckPostIK = (ShoulderLeftPostIK + ShoulderRightPostIK) / 2.f;
	FVector SpineDirectionPost = NeckPostIK.GetUnsafeNormal();

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

	const FVector RightAxis = Waist.GetRotation().GetRightVector();
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

	Waist = BonesActorSpace[RibcageIndex];
	Waist.SetRotation(RibcageRot);

	FVector PostIK_Loc = (PostIKTransformsLeft[0].GetLocation() + PostIKTransformsRight[0].GetLocation()) * 0.5f;
	FVector PreIK_Loc = (CapturedSkeletonL[0].GetLocation() + CapturedSkeletonR[0].GetLocation()) * 0.5f;
	FVector PelvisOffsetDelta = PostIK_Loc - PreIK_Loc;

	float WaistOffsetMul = UKismetMathLibrary::MapRangeClamped(StandingPercent, 50.f, 100.f, 0.5f, 0.9f);
	PelvisOffsetDelta *= WaistOffsetMul;
	if (PelvisOffsetDelta.SizeSquared() > 4.f * 4.f)
	{
		PelvisOffsetDelta = PelvisOffsetDelta.GetSafeNormal() * 4.f;
	}

	Waist.AddToTranslation(PelvisOffsetDelta);

	if (bDebugDraw)
	{
		DrawDebugSphere(GetWorld(), PostIK_Loc, 10.f, 5, FColor::Yellow, false, 0.05f, 0, 0.5f);
		DrawDebugSphere(GetWorld(), PreIK_Loc, 10.f, 5, FColor::Cyan, false, 0.05f, 0, 0.5f);
	}

	// Calculate bezier spine
	TArray<FTransform> FixedSpineBones;
	const int32 Num = SpineBones.Num();
	FixedSpineBones.SetNum(Num);
	CalculateBezierSpine(IKTargets.Pelvis, Waist, SpineBones.Num() - 1, SpineBones_RibcageIndex, SpineLength * MeshScale.Z);
}

void UYnnkVrAvatarComponent::SetEnabled(bool bNewIsEnabled)
{
	bNewIsEnabled = bNewIsEnabled && bInitialized;
	SetComponentTickEnabled(bNewIsEnabled);

	if (BodyModel == EYnnkBodyModelSource::YBM_YnnkNeuralNet)
	{
	// Initialize PyTorch model and used variables
		if (bPyTorchAvailable && bNewIsEnabled && !IsValid(TorchModule))
		{
#if PLATFORM_WINDOWS
			TorchModule = USimpleTorchController::CreateSimpleTorchController(this);

			FString ModelFile = GetResourcesPath() / TEXT("vrfullbodymodel.tmod");
			if (!FPaths::FileExists(ModelFile))
			{
				bPyTorchAvailable = false;
				UE_LOG(LogYnnk, Warning, TEXT("Can't find full-body neural net model file: %s"), *ModelFile);
				return;
			}

			if (!TorchModule->LoadTorchScriptModel(ModelFile))
			{
				bPyTorchAvailable = false;
				UE_LOG(LogYnnk, Warning, TEXT("Unable to load neural net model"));
				return;
			}

			VelocitiesHistory.SetNum(3);
			for (auto& BoneVel : VelocitiesHistory)
			{
				BoneVel.SetNum(10);
			}

			FMemory::Memzero(RuntimeValues, RUNVALUES_NUM * SD_NUM * sizeof(float));
			FMemory::Memzero(RuntimeSdSum, RUNVALUES_NUM * sizeof(float));
			FMemory::Memzero(RuntimeSoftSum, RUNVALUES_NUM * sizeof(float));

			// in-out tensors
			NnInputs.Create({ 29 });
			NnOutputs.Create({ RUNVALUES_NUM });

			RuntimeIndex = 0;
#endif //PLATFORM_WINDOWS
			}
		}
}

bool UYnnkVrAvatarComponent::CalibrateInTPose(FString& Errors)
{
	const float WorldToMetersScale = GEngine->XRSystem.IsValid()
		? GEngine->XRSystem->GetWorldToMetersScale() * 0.01f
		: 1.f;

	if (!bInitialized)
	{
		Errors = TEXT("Component isn't initialized");
		return false;
	}

	const FTransform Base = GroundRefComponent->GetComponentTransform();
	FVector HeadLoc = HeadComponent->GetComponentTransform().GetRelativeTransform(Base).GetTranslation();
	FVector HandRLoc = (HandRightOffset * HandRightComponent->GetComponentTransform()).GetRelativeTransform(Base).GetTranslation();
	FVector HandLLoc = (HandLeftOffset * HandLeftComponent->GetComponentTransform()).GetRelativeTransform(Base).GetTranslation();

	if (HeadLoc.IsZero() || HandRLoc.IsZero() || HandRLoc.IsZero())
	{
		Errors = TEXT("At least one component isn't tracked");
		return false;
	}

	if (HeadLoc.Z < 50.f)
	{
		Errors = TEXT("Head is too low");
		return false;
	}

	if (HandRLoc.Z > HeadLoc.Z || HandLLoc.Z > HeadLoc.Z)
	{
		Errors = TEXT("Hands should be lower then head");
		return false;
	}

	if (HandRLoc.Z < HeadLoc.Z + HeadJointOffsetFromHMD.Z + RibageOffsetFromHeadJoint.Z - 15.f || HandLLoc.Z < HeadLoc.Z + HeadJointOffsetFromHMD.Z + RibageOffsetFromHeadJoint.Z - 15.f)
	{
		Errors = TEXT("Hands are too low");
		return false;
	}

	if (FMath::Abs(HandRLoc.Z - HandLLoc.Z) > 20.f)
	{
		Errors = TEXT("Hands in T pose should have approximately the same vertical coordinate");
		return false;
	}

	/*
	HeadLoc *= WorldToMetersScale;
	HandRLoc *= WorldToMetersScale;
	HandLLoc *= WorldToMetersScale;
	*/

	const float RawControllersDist = FVector::Dist(HandRLoc, HandLLoc);
	float ArmSpan = RawControllersDist + CalibrationOffset_Hand * 2.f;
	float Height = HeadLoc.Z + CalibrationOffset_Head;

	UE_LOG(LogYnnk, Log, TEXT("Calibration complete. HMD_Z = %f Controllers_Distance = %f"), HeadLoc.Z, RawControllersDist);
	NnInputScale.X = NnInputScale.Y = RawControllersDist / 140.f;
	NnInputScale.Z = HeadLoc.Z / 170.f;

	SetBodySizeManually(Height, ArmSpan);

	if (GetIsReplicated())
	{
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			if (__uev_IsServer)
			{
				ClientsUpdateCalibration(Height, ArmSpan);
			}
			else
			{
				ServerUpdateCalibration(Height, ArmSpan);
			}
		}
	}

	return true;
}

float UYnnkVrAvatarComponent::GetLowerBodyRotation(bool bWorldSpace) const
{
	float f = LastPelvisYaw.Yaw;
	if (bWorldSpace && bInitialized)
	{
		f = FRotator::NormalizeAxis(f + GroundRefComponent->GetComponentRotation().Yaw);
	}
	return f;
}

bool UYnnkVrAvatarComponent::AttachHandToComponent(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform)
{
	APawn* p = Cast<APawn>(GetOwner());
	if (GetIsReplicated() && p)
	{
		bool bIsServer = __uev_IsServer;

		if (p->IsLocallyControlled())
		{
			return AttachHandToComponentLocal(bHandRight, Component, SocketName, RelativeTransform);
		}
		else if (bIsServer)
		{
			ClientAttachHandToComponent(bHandRight, Component, SocketName, RelativeTransform);
			return true;
		}
		else
		{
			UE_LOG(LogYnnk, Warning, TEXT("AttachHandToComponent should be called on local client or server"));
			return false;
		}
	}
	else
	{
		return AttachHandToComponentLocal(bHandRight, Component, SocketName, RelativeTransform);
	}
}

bool UYnnkVrAvatarComponent::AttachHandToComponentLocal(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform)
{
	if (!IsValid(Component))
	{
		return false;
	}

	if (bHandRight)
	{
		bHandAttachedRight = true;
		HandParentRight = Component;
		HandAttachSocketRight = SocketName;
		HandAttachTransformRight = RelativeTransform;
	}
	else /* left hand */
	{
		bHandAttachedLeft = true;
		HandParentLeft = Component;
		HandAttachSocketLeft = SocketName;
		HandAttachTransformLeft = RelativeTransform;
	}

	return true;
}

void UYnnkVrAvatarComponent::ClientAttachHandToComponent_Implementation(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform)
{
	if (APawn* p = Cast<APawn>(GetOwner()))
	{
		if (p->IsLocallyControlled())
		{
			AttachHandToComponentLocal(bHandRight, Component, SocketName, RelativeTransform);
		}
	}
}

void UYnnkVrAvatarComponent::DetachHandFromComponent(bool bHandRight)
{
	APawn* p = Cast<APawn>(GetOwner());
	if (GetIsReplicated() && p)
	{
		bool bIsServer = __uev_IsServer;

		if (p->IsLocallyControlled())
		{
			DetachHandFromComponentLocal(bHandRight);
		}
		else if (bIsServer)
		{
			ClientDetachHandFromComponent(bHandRight);
		}
		else
		{
			UE_LOG(LogYnnk, Warning, TEXT("DetachHandFromComponent should be called on local client or server"));
		}
	}
	else
	{
		return DetachHandFromComponentLocal(bHandRight);
	}
}

void UYnnkVrAvatarComponent::DetachHandFromComponentLocal(bool bHandRight)
{
	if (bHandRight)
	{
		bHandAttachedRight = false;
		HandParentRight = nullptr;
		HandAttachTransformRight = FTransform::Identity;
		HandAttachSocketRight = TEXT("");
	}
	else /* left hand */
	{
		bHandAttachedLeft = false;
		HandParentLeft = nullptr;
		HandAttachTransformLeft = FTransform::Identity;
		HandAttachSocketLeft = TEXT("");
	}
}

void UYnnkVrAvatarComponent::ClientDetachHandFromComponent_Implementation(bool bHandRight)
{
	if (APawn* p = Cast<APawn>(GetOwner()))
	{
		if (p->IsLocallyControlled())
		{
			DetachHandFromComponentLocal(bHandRight);
		}
	}
}

void UYnnkVrAvatarComponent::ClientsUpdateCalibration_Implementation(float Height, float ArmSpan)
{
	SetBodySizeManually(Height, ArmSpan);
}

// calc shoulder pitch and yaw offsets
void UYnnkVrAvatarComponent::CalcShouldersWithOffset(EControllerHand Hand, float DeltaTime)
{
	const float ShoulderOffsetYawP = 32.f;
	const float ShoulderOffsetYawN = -25.f;
	const float ShoulderOffsetPitchP = 30.f;
	const float ShoulderOffsetPitchN = -8.f;

	FTransform RibcageTr = BonesActorSpace[RibcageIndex];
	FTransform ClavicleTr, UpperarmTr;
	FVector HandLoc;

	const float HandLength = (BoneSizeScaled.UpperarmLength + BoneSizeScaled.LowerarmLength);

	// rotation target and default clavicle state
	if (Hand == EControllerHand::Right)
	{
		ClavicleTr = ClavicleRightToRibcageGen * RibcageTr;
		UpperarmTr = UpperarmRightToClavicleGen * ClavicleTr;
		HandLoc = IKTargets.HandRight.GetTranslation();
	}
	else
	{
		ClavicleTr = ClavicleLeftToRibcageGen * RibcageTr;
		UpperarmTr = UpperarmLeftToClavicleGen * ClavicleTr;
		HandLoc = IKTargets.HandLeft.GetTranslation();
	}
	FTransform RelHand = FTransform(HandLoc).GetRelativeTransform(FTransform(IKTargets.Ribcage.GetRotation(), ClavicleTr.GetTranslation()));

	//float ShoulderShiftKoefX, ShoulderShiftKoefZ;
	FRotator& ApplyRot = (Hand == EControllerHand::Right) ? ShoulderRightLocal : ShoulderLeftLocal;

	if (FVector::Dist(UpperarmTr.GetTranslation(), HandLoc) < HandLength)
	{
		ApplyRot = FMath::RInterpTo(ApplyRot, FRotator::ZeroRotator, DeltaTime, 12.f);
	}
	else
	{
		FVector Target = HandLoc + (UpperarmTr.GetTranslation() - HandLoc).GetSafeNormal() * HandLength;

		FRotator BaseRot = UKismetMathLibrary::MakeRotFromXZ(UpperarmTr.GetTranslation() - ClavicleTr.GetTranslation(), ClavicleTr.GetRotation().GetUpVector());
		FRotator NewRot = UKismetMathLibrary::MakeRotFromXZ(Target - ClavicleTr.GetTranslation(), ClavicleTr.GetRotation().GetUpVector());
		ApplyRot = UKismetMathLibrary::NormalizedDeltaRotator(NewRot, BaseRot);

		ApplyRot.Pitch = FMath::Clamp(ApplyRot.Pitch, ShoulderOffsetPitchN, ShoulderOffsetPitchP);
		if (Hand == EControllerHand::Right)
		{
			ApplyRot.Yaw = FMath::Clamp(ApplyRot.Yaw * -1.f, ShoulderOffsetYawN, ShoulderOffsetYawP) * -1.f;
		}
		else
		{
			ApplyRot.Yaw = FMath::Clamp(ApplyRot.Yaw, ShoulderOffsetYawN, ShoulderOffsetYawP);
		}
	}

	FRotator ClavicleRot = ClavicleTr.Rotator();
	YnnkHelpers::AddRelativeRotation(ClavicleRot, ApplyRot);
	ClavicleTr.SetRotation(ClavicleRot.Quaternion());

	// 5) update data struct
	if (Hand == EControllerHand::Right)
	{
		BonesActorSpace[ClavicleRightIndex] = ClavicleTr;
	}
	else
	{
		BonesActorSpace[ClavicleLeftIndex] = ClavicleTr;
	}
}

void UYnnkVrAvatarComponent::UpdatePoseSnapshot()
{
	// References to skeleton data
	const TArray<FTransform>& ComponentSpaceTMs = BodyMesh->GetComponentSpaceTransforms();
	const FReferenceSkeleton& RefSkeleton = __uev_GetRefSkeleton(__uev_GetSkeletalMesh(BodyMesh));
	const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();
	const int32 NumSpaceBases = ComponentSpaceTMs.Num();

	// Need to initialize pose snapshot object?
	if (!CurrentPoseSnapshot.bIsValid)
	{
		CurrentPoseSnapshot.bIsValid = true;
		CurrentPoseSnapshot.SkeletalMeshName = __uev_GetSkeletalMesh(BodyMesh)->GetFName();
		CurrentPoseSnapshot.SnapshotName = TEXT("YnnkAvatar");
		CurrentPoseSnapshot.BoneNames.SetNumUninitialized(NumSpaceBases);
		CurrentPoseSnapshot.LocalTransforms.SetNumUninitialized(NumSpaceBases);

		for (int32 BoneIndex = 0; BoneIndex < NumSpaceBases; BoneIndex++)
		{
			CurrentPoseSnapshot.BoneNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);
			CurrentPoseSnapshot.LocalTransforms[BoneIndex] = RefPoseSpaceBaseTMs[BoneIndex];
		}

		// init cache
		SnapshotTransformsCache.SetNumUninitialized(NumSpaceBases);
		SnapshotTransformsCache_State.SetNumUninitialized(NumSpaceBases);
	}

	// Clear cache state
	FMemory::Memzero(SnapshotTransformsCache_State.GetData(), SnapshotTransformsCache_State.Num() /* * sizeof(uint8) */);

	// Update mesh position
	const FTransform SubBase = GroundRefComponent->GetComponentTransform();
	FTransform PelvisWorld = BonesActorSpace[PelvisIndex] * SubBase;

	FTransform BodyMeshTr = FTransform(BodyMesh->GetComponentQuat(), PelvisWorld.GetTranslation());
	BodyMeshTr.SetTranslation(FVector(BodyMeshTr.GetTranslation().X, BodyMeshTr.GetTranslation().Y, SubBase.GetTranslation().Z));
	BodyMesh->SetWorldLocation(BodyMeshTr.GetTranslation());
	// Offset from BodyMesh to ground reference component
	const FTransform MeshToGround = BodyMeshTr.GetRelativeTransform(SubBase);

	// Convert transforms
	for (int32 BoneIndex = 0; BoneIndex < NumSpaceBases; BoneIndex++)
	{
		if (BonesActorSpace[BoneIndex].GetTranslation().IsZero())
		{
			// use ref-pose transform
			continue;
		}

		int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);

		// get transform of previout bone
		const FTransform ParentBone = (BoneIndex == 0)
			? MeshToGround
			: RestoreSnapshotTransformInAS(RefSkeleton, MeshToGround, ParentIndex);
		// convert orentation to skeleton space
		FTransform t = SkeletonTransforms[BoneIndex] * BonesActorSpace[BoneIndex];
		// set to relative
		t = t.GetRelativeTransform(ParentBone);

		if (BoneIndex == PelvisIndex)
		{
			FVector OriginalScale = RefPoseSpaceBaseTMs[BoneIndex].GetScale3D();
			// add vertical scaling
			t.SetScale3D(OriginalScale * FVector(MeshScale.Z, MeshScale.Z, MeshScale.Z));

			CurrentPoseSnapshot.LocalTransforms[BoneIndex] = t;
		}
		else if (BoneIndex == ClavicleRightIndex || BoneIndex == ClavicleLeftIndex)
		{
			FVector BoneScale = RefPoseSpaceBaseTMs[BoneIndex].GetScale3D();
			// remove vertical scaling and add arm scaling
			BoneScale *= FVector(MeshScale.X / MeshScale.Z, MeshScale.X / MeshScale.Z, MeshScale.X / MeshScale.Z);

			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetScale3D(BoneScale);
			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetRotation(t.GetRotation());
		}
		else if (BoneIndex == UpperarmRightIndex)
		{
			FVector Start = (RefPoseSpaceBaseTMs[BoneIndex] * ParentBone).GetTranslation();
			FVector End = YnnkHelpers::CalculateTwoBoneIK(
				Start, IKTargets.HandRight.GetTranslation(), ElbowTargetRight_AS,
				BoneSizeScaled.UpperarmLength, BoneSizeScaled.LowerarmLength,
				BonesActorSpace[UpperarmRightIndex], BonesActorSpace[LowerarmRightIndex], true, false);
			BonesActorSpace[HandRightIndex].SetTranslation(End);
			BonesActorSpace[HandRightIndex].SetRotation(IKTargets.HandRight.GetRotation());

			// use updated transform
			t = SkeletonTransforms[BoneIndex] * BonesActorSpace[BoneIndex];
			// set to relative
			t = t.GetRelativeTransform(ParentBone);
			// apply
			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetRotation(t.GetRotation());
		}
		else if (BoneIndex == UpperarmLeftIndex)
		{			
			FVector Start = (RefPoseSpaceBaseTMs[BoneIndex] * ParentBone).GetTranslation();
			FVector End = YnnkHelpers::CalculateTwoBoneIK(
				Start, IKTargets.HandLeft.GetTranslation(), ElbowTargetLeft_AS,
				BoneSizeScaled.UpperarmLength, BoneSizeScaled.LowerarmLength,
				BonesActorSpace[UpperarmLeftIndex], BonesActorSpace[LowerarmLeftIndex], false, false);
			BonesActorSpace[HandLeftIndex].SetTranslation(End);

			// use updated transform
			t = SkeletonTransforms[BoneIndex] * BonesActorSpace[BoneIndex];
			// set to relative
			t = t.GetRelativeTransform(ParentBone);
			// apply
			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetRotation(t.GetRotation());
		}
		else if (BoneIndex == HeadIndex)
		{
			const float MaxHeadScale = 1.1f;
			FVector BoneScale = RefPoseSpaceBaseTMs[BoneIndex].GetScale3D();
			if (MeshScale.Z > MaxHeadScale)
			{
				BoneScale *= (MaxHeadScale / MeshScale.Z);
			}

			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetRotation(t.GetRotation());
			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetScale3D(BoneScale);
		}
		else
		{
			CurrentPoseSnapshot.LocalTransforms[BoneIndex].SetRotation(t.GetRotation());
		}
	}
}

FTransform UYnnkVrAvatarComponent::RestoreSnapshotTransformInAS(const FReferenceSkeleton& RefSkeleton, const FTransform& MeshToGround, const int32 CurrentBoneIndex)
{
	FTransform tr_bone = CurrentPoseSnapshot.LocalTransforms[CurrentBoneIndex];

	int32 ParentIndex = RefSkeleton.GetParentIndex(CurrentBoneIndex);
	while (ParentIndex != INDEX_NONE)
	{
		if (SnapshotTransformsCache_State[ParentIndex])
		{
			tr_bone *= SnapshotTransformsCache[ParentIndex];

			// save to cache
			SnapshotTransformsCache[CurrentBoneIndex] = tr_bone;
			SnapshotTransformsCache_State[CurrentBoneIndex] = TRUE;
			return tr_bone;
		}

		tr_bone *= CurrentPoseSnapshot.LocalTransforms[ParentIndex];
		ParentIndex = RefSkeleton.GetParentIndex(ParentIndex);
	}

	// add component space
	tr_bone *= MeshToGround;

	// save to cache
	SnapshotTransformsCache[CurrentBoneIndex] = tr_bone;
	SnapshotTransformsCache_State[CurrentBoneIndex] = TRUE;

	return tr_bone;
}

/**
* Calculate joint target from ribcage, shoulder and hand transforms
*/
FVector UYnnkVrAvatarComponent::CalculateElbow(const FTransform& Ribcage, const FTransform& Shoulder, const FTransform& Wrist, bool bRight)
{
	const float HandLength = BoneSizeScaled.UpperarmLength + BoneSizeScaled.LowerarmLength;

	// 1. Get Alpha coefficient for interpolation.
	float AlphaDivider = (HandLength == 0.f) ? 1.f : HandLength;
	const FVector WristToShoulder = Shoulder.GetTranslation() - Wrist.GetTranslation();
	float Alpha = WristToShoulder.Size() / AlphaDivider;

	// borders adjustment
	float SinkAlpha = FMath::Clamp(FMath::Lerp(ElbowSinkFactor, ElbowFarSinkFactor, Alpha), 0.f, 1.f);

	// 2. Initial joint target centre.
	const FVector CentralPoint = FMath::Lerp<FVector>(Shoulder.GetTranslation(), Wrist.GetTranslation(), Alpha);

	// 3. Addend.

	// Choose between thumb-finger and forward-fingers directions
	FVector WristUp = bRight
		? Wrist.GetRotation().GetRightVector() * -1.f
		: Wrist.GetRotation().GetRightVector();
		//Wrist.GetRotation().GetUpVector();

	// reduce up-turned elbows
	const float WristShoulderDP = FMath::Clamp(FVector::DotProduct(WristToShoulder.GetSafeNormal(), WristUp), 0.f, 1.f);
	WristUp = (FMath::Lerp<FVector>(WristUp, Wrist.GetRotation().GetForwardVector(), WristShoulderDP)).GetSafeNormal();

	// Choose between ribcage-up and wrist-up directions
	FVector AddendUp = FMath::Lerp<FVector>(Ribcage.GetRotation().GetUpVector(), WristUp, SinkAlpha);

	if (AddendUp.IsNearlyZero())
	{
		AddendUp = Alpha > 0.f ? WristUp : Ribcage.GetRotation().GetUpVector();
	}
	else
	{
		AddendUp.Normalize();
	}

	// 4. Final calculation.
	return CentralPoint - (AddendUp * 20.f);
}

void UYnnkVrAvatarComponent::BuildBodyInternal_YNN(float DeltaTime)
{
	const FTransform InputBaseTr = bNeuralNet_IgnoreOwnerMovement
		? FTransform::Identity
		: GroundRefComponent->GetComponentTransform();

	FTransform RawHead, RawControllerRight, RawControllerLeft;
	if (bNeuralNet_IgnoreOwnerMovement)
	{
		const FTransform SubBase = GroundRefComponent->GetComponentTransform();
		RawHead = HeadComponent->GetComponentTransform().GetRelativeTransform(SubBase);
		RawControllerRight = HandRightComponent->GetComponentTransform().GetRelativeTransform(SubBase);
		RawControllerLeft = HandLeftComponent->GetComponentTransform().GetRelativeTransform(SubBase);
	}
	else
	{
		RawHead = HeadComponent->GetComponentTransform();
		RawControllerRight = HandRightComponent->GetComponentTransform();
		RawControllerLeft = HandLeftComponent->GetComponentTransform();
	}

	FTransform TickActorOffset;

	// set default positions for non-tracked HMD and controllers
	if (HeadComponent->GetRelativeLocation().IsNearlyZero())
	{
		RawHead = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, PlayerHeight));
		RawHead *= InputBaseTr;
	}
	if (HandRightComponent->GetRelativeLocation().IsNearlyZero())
	{
		FTransform HeadVertical = FTransform(FRotator(0.f, RawHead.Rotator().Yaw, 0.f), RawHead.GetTranslation());
		RawControllerRight = FTransform(FRotator(15.f, -30.f, -60.f), FVector(25.f, 45.f, -40.f));
		RawControllerRight *= HeadVertical;
	}
	if (HandLeftComponent->GetRelativeLocation().IsNearlyZero())
	{
		FTransform HeadVertical = FTransform(FRotator(0.f, RawHead.Rotator().Yaw, 0.f), RawHead.GetTranslation());
		RawControllerLeft = FTransform(FRotator(15.f, 30.f, 60.f), FVector(25.f, -45.f, -40.f));
		RawControllerLeft *= HeadVertical;
	}

	if (!bPyTorchAvailable || !IsValid(TorchModule))
	{
		IKTargets.FootRight = FTransform(FRotator(), FVector(0.f, 15.f, 10.f));
		IKTargets.FootLeft = FTransform(FRotator(), FVector(0.f, -15.f, 10.f));
		IKTargets.Pelvis = FTransform(FRotator(), FVector(0.f, 0.f, 90.f));

		return;
	}

	// get base from world-space head
	FTransform ComponentBaseTr = RawHead;
	// project head rotation onto horizontal plane
	FRotator BaseRot = YnnkHelpers::CalculateRootRotation(ComponentBaseTr.GetRotation().GetForwardVector(), ComponentBaseTr.GetRotation().GetUpVector());
	FVector HeadLoc = ComponentBaseTr.GetTranslation();
	HeadLoc.Z = InputBaseTr.GetTranslation().Z + 150.f; // BaseCaptureHeight
	ComponentBaseTr = FTransform(BaseRot, HeadLoc);

	// head, hand right, hand left
	TArray<FTransform> BoneTr, BoneTrWorld;
	TArray<FVector> Velocities, OldVelocities;
	BoneTr.SetNumUninitialized(3);
	BoneTrWorld.SetNumUninitialized(3);
	Velocities.SetNumUninitialized(3);
	OldVelocities.SetNumUninitialized(3);

	/**
	* scale input translation coordinates from current player size to actor size in the training set	
	*/
	{
		BoneTrWorld[0] = RawHead.GetRelativeTransform(InputBaseTr);
		BoneTrWorld[1] = (HandRightOffset * RawControllerRight).GetRelativeTransform(InputBaseTr);
		BoneTrWorld[2] = (HandLeftOffset * RawControllerLeft).GetRelativeTransform(InputBaseTr);

		for (int32 i = 0; i < 3; i++)
		{
			BoneTrWorld[i].ScaleTranslation(NnInputScale);
			BoneTrWorld[i] *= InputBaseTr;
		}

		// fix ComponentBaseTr
		FVector v = BoneTrWorld[0].GetTranslation();
		v.Z = InputBaseTr.GetTranslation().Z + 150.f;
		ComponentBaseTr.SetTranslation(v);		
	}

	// get transforms in world space

	// First frame? Save current values as previous
	if (LastTargetsTransforms.Num() < 3)
	{
		LastActorTransform = InputBaseTr;

		LastTargetsTransforms.SetNumUninitialized(3);
		for (int32 i = 0; i < 3; i++)
			LastTargetsTransforms[i] = BoneTrWorld[i];
	}

	/* 03.03.2022 - roll flip */
	const float HeadPitchWS = BoneTrWorld[0].Rotator().Pitch;
	FRotator HeadBaseRot = ComponentBaseTr.Rotator();
	FRotator RelHeadRot = FTransform(BoneTrWorld[0]).GetRelativeTransform(FTransform(HeadBaseRot)).Rotator();
	if (FMath::Abs(BoneTrWorld[0].Rotator().Roll) > 90.f || FMath::Abs(HeadPitchWS) > 80.f)
	{
		FVector rig = BoneTrWorld[0].GetRotation().GetRightVector();
		FVector fwd = HeadBaseRot.Quaternion().GetForwardVector();

		if (FVector::DotProduct(BoneTrWorld[0].GetRotation().GetForwardVector(), InputBaseTr.GetRotation().GetUpVector()) < 0.f)
		{
			FRotator NewRot = UKismetMathLibrary::MakeRotFromYX(rig, fwd);
			FRotator addend = FRotator(-80.f, 0.f, 0.f);

			FRotator NewHeadRot = UKismetMathLibrary::ComposeRotators(NewRot.GetInverse(), addend.GetInverse()).GetInverse();
			BoneTrWorld[0].SetRotation(NewHeadRot.Quaternion());
		}
		else
		{
			FRotator NewHeadRot = (FVector::DotProduct(BoneTrWorld[0].GetRotation().GetUpVector(), HeadBaseRot.Quaternion().GetForwardVector()) > 0)
				? UKismetMathLibrary::MakeRotFromYX(rig, GroundRefComponent->GetUpVector() * -1.f)
				: UKismetMathLibrary::MakeRotFromYX(rig, GroundRefComponent->GetUpVector());
			BoneTrWorld[0].SetRotation(NewHeadRot.Quaternion());
		}
	}

	// we need to find velocities to feed them to neural net
	for (int32 i = 0; i < 3; i++)
	{
		BoneTr[i] = BoneTrWorld[i].GetRelativeTransform(ComponentBaseTr);

		Velocities[i] = (BoneTr[i].GetTranslation() - LastTargetsTransforms[i].GetRelativeTransform(ComponentBaseTr).GetTranslation()) / DeltaTime;

		for (int32 VelIndex = 0; VelIndex < (10 - 1); VelIndex++)
		{
			VelocitiesHistory[i][VelIndex] = VelocitiesHistory[i][VelIndex + 1];
		}
		VelocitiesHistory[i][9] = Velocities[i];
		OldVelocities[i] = VelocitiesHistory[i][0] * 0.2f + VelocitiesHistory[i][5] * 0.5f;
	}

	// fill input tensor
	FVector l; FQuat q; FRotator r;
	l = BoneTr[0].GetTranslation();
	q = BoneTr[0].GetRotation();
	r = BoneTr[0].Rotator();
	// scale values for neural net
	// we use euler coordinates instead of quaternion for orientation,
	// because relative rotation has no marginal +180/-180 values,
	// and this allows to decrease NN inputs count
	NnInputs[0] = (l.Z - 16.f) * 10.f;
	NnInputs[1] = r.Roll * 10.f;
	NnInputs[2] = r.Pitch * 10.f;
	NnInputs[3] = Velocities[0].X * 10.f;
	NnInputs[4] = Velocities[0].Y * 10.f;
	NnInputs[5] = Velocities[0].Z * 10.f;
	NnInputs[6] = OldVelocities[0].X;
	NnInputs[7] = OldVelocities[0].Y;
	NnInputs[8] = OldVelocities[0].Z;

	for (int32 i = 1; i < 3; i++)
	{
		int32 Ind = 9 + (i - 1) * 10;
		l = BoneTr[i].GetTranslation();
		q = BoneTr[i].GetRotation();
		PushTransformToArr(NnInputs, Ind, l, q, Velocities[i]);
	}

	// get and process results

	// Measure execution time?
	/*
	int32 Sec1, Sec2;
	float Msec1, Msec2;
	UGameplayStatics::GetAccurateRealTime(this, Sec1, Msec1);
	*/

	TorchModule->DoForwardCall(NnInputs, NnOutputs);
	check(NnOutputs.GetDataSize() == RUNVALUES_NUM);

	/*
	UGameplayStatics::GetAccurateRealTime(this, Sec2, Msec2);
	UE_LOG(LogYnnk, Log, TEXT("Execute time: %d sec %f msec"), Sec2 - Sec1, Msec2 - Msec1);
	*/

	// unscale
	NnOutputs[0].ScaleItem(0.2f, -20.f);
	NnOutputs[2].ScaleItem(0.1f, -70.f);
	NnOutputs[3] *= 0.2f;
	NnOutputs[4].ScaleItem(0.2f, -10.f);
	NnOutputs[6] -= 20.f;
	NnOutputs[8].ScaleItem(0.1f, -136.f);
	NnOutputs[11] -= 20.f;
	NnOutputs[13].ScaleItem(0.1f, -136.f);

	// filter and smooth results
	for (int32 i = 0; i < RUNVALUES_NUM; i++)
	{
		NnOutputs[i] = FilterNNValue(RuntimeIndex, NnOutputs[i].Item(), RuntimeValues[i], RuntimeSdSum[i], RuntimeSoftSum[i], i < 6);
	}

	// save resulted transforms
	r = FRotator(NnOutputs[4].Item(), NnOutputs[5].Item(), NnOutputs[3].Item()); r.Normalize();
	IKTargets.Pelvis = FTransform(r, FVector(NnOutputs[0].Item(), NnOutputs[1].Item(), NnOutputs[2].Item()));

	r = FRotator(NnOutputs[9].Item(), NnOutputs[10].Item(), 0.f); r.Normalize();
	IKTargets.FootRight = FTransform(r, FVector(NnOutputs[6].Item(), NnOutputs[7].Item(), NnOutputs[8].Item()));

	r = FRotator(NnOutputs[14].Item(), NnOutputs[15].Item(), 0.f); r.Normalize();
	IKTargets.FootLeft = FTransform(r, FVector(NnOutputs[11].Item(), NnOutputs[12].Item(), NnOutputs[13].Item()));

	// feet blending?
	const float SpeedTreshold = 45.f;  /* cm per second */
	if (WalkingSpeedMean > SpeedTreshold || HeadPitchWS > -45.f)
	{
		FootRightRaw = IKTargets.FootRight;
		FootLeftRaw = IKTargets.FootLeft;
	}
	else
	{
		const float BlendAlpha = FMath::Clamp((WalkingSpeedMean - 10.f) / (SpeedTreshold - 10.f), 0.f, 1.f);
		IKTargets.FootRight = UKismetMathLibrary::TLerp(FootRightRaw, IKTargets.FootRight, BlendAlpha);
		IKTargets.FootLeft = UKismetMathLibrary::TLerp(FootLeftRaw, IKTargets.FootLeft, BlendAlpha);
	}

	// to world space
	IKTargets.Pelvis *= BoneTrWorld[0];
	IKTargets.FootRight *= ComponentBaseTr;
	IKTargets.FootLeft *= ComponentBaseTr;

	// trace IK feet
	TestFootToGround(IKTargets.FootRight, true, false);
	TestFootToGround(IKTargets.FootLeft, false, false);

	// to actor's space
	IKTargets.Pelvis = IKTargets.Pelvis.GetRelativeTransform(InputBaseTr);
	IKTargets.FootRight = IKTargets.FootRight.GetRelativeTransform(InputBaseTr);
	IKTargets.FootLeft = IKTargets.FootLeft.GetRelativeTransform(InputBaseTr);

	/**
	* unscale for player size
	*/
	{
		FVector Mul = FVector(1.f / NnInputScale.X, 1.f / NnInputScale.Y, 1.f / NnInputScale.Z);
		IKTargets.Pelvis.ScaleTranslation(Mul);
		IKTargets.FootRight.ScaleTranslation(Mul);
		IKTargets.FootLeft.ScaleTranslation(Mul);
	}

	// updates for next frame

	for (int32 i = 0; i < 3; i++)
		LastTargetsTransforms[i] = BoneTrWorld[i];

	RuntimeIndex++;
	if (RuntimeIndex == SD_NUM) RuntimeIndex = 0;

	LastActorTransform = InputBaseTr;
}

void UYnnkVrAvatarComponent::BuildBodyInternal_Procedural(float DeltaTime)
{
	if (UseSpecialRibcageAlgorithm)
	{
		BuildTorsoTargetsSG(DeltaTime);
	}
	else
	{
		BuildTorsoTargets(DeltaTime);
	}
	BuildFeetTargets(DeltaTime);
}

void UYnnkVrAvatarComponent::UpdateVelocitiesInternal(float DeltaTime)
{
	const FTransform SubBase = GroundRefComponent->GetComponentTransform();

	// 1. Velocity from component, i. e. generated by controllers input

	const FVector Velocity = (SubBase.GetTranslation() - LastRefTransform.GetTranslation()) / DeltaTime;
	LastRefTransform = SubBase;

	const FVector VelocityProj = SubBase.GetRotation().UnrotateVector(Velocity);
	if (Velocity.SizeSquared() > 20.f)
	{
		WalkingSpeed = VelocityProj.Size2D();
		FRotator WalkingDirRot = UKismetMathLibrary::MakeRotFromXZ(Velocity, SubBase.GetRotation().GetUpVector());
		WalkingYaw = FRotator::NormalizeAxis(WalkingDirRot.Yaw - SubBase.Rotator().Yaw - IKTargets.Pelvis.Rotator().Yaw);
	}
	else
	{
		WalkingSpeed = FMath::FInterpTo(WalkingSpeed, 0.f, DeltaTime, 8.f);
		WalkingYaw = FMath::FInterpTo(WalkingYaw, 0.f, DeltaTime, 8.f);
	}
	
	// Save to history
	WalkingSpeedHistory[WalkingSpeedHistoryIndex++] = WalkingSpeed;
	WalkingSpeedHistoryIndex %= WalkingSpeedHistory.Num();

	// Get mean velocity
	WalkingSpeedMean = 0.f;
	for (const auto& val : WalkingSpeedHistory) WalkingSpeedMean += val;
	WalkingSpeedMean /= (float)WalkingSpeedHistory.Num();

	// 2. Get head movement (i. e. physical movement of player in a room)

	FVector PhysicalVelocity = (IKTargets.Head.GetTranslation() - LastHeadTargetTransform.GetTranslation()) / DeltaTime;
	PhysicalVelocity.Z = 0.;
	LastHeadTargetTransform = IKTargets.Head;

	// Save to history
	HeadVelocityHistory[HeadVelocityIndex++] = PhysicalVelocity;
	HeadVelocityIndex %= HeadVelocityHistory.Num();

	// In this case, we save in a class variable only mean velocity, because instant value is too unstable
	HeadVelocity2D = FVector::ZeroVector;
	for (const auto& val : HeadVelocityHistory) HeadVelocity2D += val;
	HeadVelocity2D /= (double)HeadVelocityHistory.Num();
}

void UYnnkVrAvatarComponent::BuildTorsoTargets(float DeltaTime)
{
	// Default rotation (X - player forward, Z - up)
	FRotator RootRotation = CalculateRootRotation(IKTargets.Head.GetRotation().GetForwardVector(), IKTargets.Head.GetRotation().GetUpVector());

	// Use mean hands rotation for [TorsoRotationSmoothFrames] frames
	FRotator HandsRotation;
	{
		// use quite high tolerance value, because close-to-zero hand direcions aren't representative anyway
		FVector HandsRight = IKTargets.HandRight.GetTranslation() - IKTargets.HandLeft.GetTranslation();
		FVector HandsRightVector2D = HandsRight.GetSafeNormal2D(0.01f);

		// if hands have a direction in a horizontal plane, and hands aren't swapped (which means this rotation also isn't representative)
		if (HandsRightVector2D.SizeSquared() > 0.f && FVector::DotProduct(HandsRightVector2D, RootRotation.Quaternion().GetRightVector()) > 0.f)
		{
			const double Mul = 0.1 / (double)ProceduralTorsoRotationSmoothFrames;
			// get average right direction for last [TorsoRotationSmoothFrames] frames
			TorsoYawByHandsSum -= (TorsoYawByHands[TorsoYawByHandsIndex] * Mul);
			TorsoYawByHandsSum += (HandsRight * Mul);
			TorsoYawByHands[TorsoYawByHandsIndex] = HandsRight;
			TorsoYawByHandsIndex = (TorsoYawByHandsIndex + 1) % ProceduralTorsoRotationSmoothFrames;
		}

		// get rotator by hands from right direction vector
		// later we'll blend this rotator with head rotator
		FVector TargetVector = (TorsoYawByHandsSum).GetSafeNormal();
		if (TargetVector.IsNearlyZero(0.01f))
		{
			TargetVector = HandsRightVector2D.IsNearlyZero() ? TorsoYawByHandsMean : HandsRightVector2D;
		}
		TorsoYawByHandsMean = FMath::VInterpTo(TorsoYawByHandsMean, TargetVector, GetWorld()->DeltaTimeSeconds, 15.f);
		HandsRotation = UKismetMathLibrary::MakeRotFromZY(RootRotation.Quaternion().GetUpVector(), TorsoYawByHandsMean);
	}

	// avoiding gimbal lock
	float RelativeHeadRotRoll, RelativeHeadRotPitch, RelativeHeadRotYaw;
	YnnkHelpers::GetRotationDeltaInEulerCoordinates(IKTargets.Head.GetRotation(), RootRotation.Quaternion(), RelativeHeadRotRoll, RelativeHeadRotPitch, RelativeHeadRotYaw);

	// Current height of head from 0 to 100% (in percents, not fraction!)
	// Current height of head from 0 to 100% (in percents, not fraction!)
	float HeadHeightCoefficientUnclamped = (IKTargets.Head.GetTranslation().Z + 2.f) / (PlayerHeight + HeadJointOffsetFromHMD.Z) * 100.f;
	float HeadHeightCoefficient = FMath::Clamp(HeadHeightCoefficientUnclamped, 0.f, 100.f);
	// Save camera vertical offset from player height
	StandingPercent = HeadHeightCoefficient;

	if (InternalBodyPose == EYnnkBodyPose::YBP_Sit)
	{
		HeadHeightCoefficientUnclamped = FMath::Max(100.f, HeadHeightCoefficientUnclamped);
		HeadHeightCoefficient = 100.f;
	}

	// Rotation by hands. Need to use average for last few seconds
	FQuat TargetRibcageYawQuat = FQuat::Slerp(RootRotation.Quaternion(), HandsRotation.Quaternion(), 0.7f);

	// Rotation of spine (to get pelvis location later)
	FRotator SpineRot = FRotator(GetCurveValue(CurveSpinePitch, CurveSpinePitch2, HeadHeightCoefficient), RootRotation.Yaw, 0.f).GetNormalized();

	// Desired ribcage rotation is almost ready,
	// but CurveRibcagePitch isn't correct for sitting/leaning, so it needs an adjustment
	float RibcagePitch = GetCurveValue(CurveRibcagePitch, CurveRibcagePitch2, RelativeHeadRotPitch);
	if (HeadHeightCoefficient < 94.f)
	{
		float RibcagePitchMultiplier = FMath::GetMappedRangeValueClamped(FVector2f(75.f, 94.f), FVector2f(0.2f, 1.f), HeadHeightCoefficient);
		RibcagePitch = FMath::Lerp<float>((float)SpineRot.Pitch, RibcagePitch, RibcagePitchMultiplier);
	}
	FRotator RibcageRot = FRotator(RibcagePitch, TargetRibcageYawQuat.Rotator().Yaw, CurveRibcageRoll->GetFloatValue(RelativeHeadRotRoll)).GetNormalized();

	// RibageOffsetFromHeadJoint is offet from IKTargets.Head to IKTargets.Ribcage
	FVector RibcageOffsetFromHead = RibcageRot.RotateVector(RibageOffsetFromHeadJoint);

	// Ribcage is ready
	IKTargets.Ribcage = FTransform(RibcageRot, IKTargets.Head.GetTranslation() + RibcageOffsetFromHead);

	// Pelvis yaw rotation is static until some angle. Then it's returned to the head (root) rotation)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float DeltaYawAbs = FMath::Abs(UKismetMathLibrary::NormalizedDeltaRotator(RootRotation, LastPelvisYaw).Yaw);

		if (!bRestorePelvisYawRotation && InternalBodyPose == EYnnkBodyPose::YBP_Default)
		{
			if (DeltaYawAbs > ProceduralPelvisReturnToHeadAngle || HeadHeightCoefficientUnclamped > 106.f || HeadHeightCoefficientUnclamped < 75.f)
			{
				bRestorePelvisYawRotation = true;
				LastPelvisYawBlendStart = LastPelvisYaw;
				RestorePelvisSoftAngleTime = -1.f;
			}
			else if (DeltaYawAbs > ProceduralPelvisReturnToHeadAngleSoft)
			{
				if (RestorePelvisSoftAngleTime < 0.f)
				{
					RestorePelvisSoftAngleTime = CurrentTime;
				}
				else if (CurrentTime - RestorePelvisSoftAngleTime > 0.7f)
				{
					bRestorePelvisYawRotation = true;
					LastPelvisYawBlendStart = LastPelvisYaw;
					RestorePelvisSoftAngleTime = -1.f;
				}
			}
			else
			{
				RestorePelvisSoftAngleTime = -1.f;
			}
		}
		else if (DeltaYawAbs < 2.f)
		{
			bRestorePelvisYawRotation = false;
		}

		if (bRestorePelvisYawRotation)
		{
			if (DeltaYawAbs < ProceduralPelvisReturnToHeadAngle)
			{
				LastPelvisYaw = FMath::RInterpConstantTo(LastPelvisYaw, RootRotation, DeltaTime, 130.f);
			}
			else
			{
				LastPelvisYaw = FMath::RInterpTo(LastPelvisYaw, RootRotation, DeltaTime, 5.f);
			}
		}
	}
	//GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::White, FString::FromInt((int)bRestorePelvisYaw));
	float ApplyYaw = LastPelvisYaw.Yaw;

	// Rotation of pelvis (applied to IK target)
	FRotator PelvsRot = FRotator(GetCurveValue(CurvePelvisPitch, CurvePelvisPitch2, HeadHeightCoefficient), ApplyYaw, 0.f).GetNormalized();

	// pelvis is ready
	float SpineRealLength = (SpineLength + RibcageBoneOffsetFromJoint.GetTranslation().Size()) * MeshScale.Z;
	IKTargets.Pelvis = FTransform(PelvsRot, IKTargets.Ribcage.GetTranslation() - SpineRot.Quaternion().GetUpVector() * SpineRealLength);

	if (bHasChair)
	{
		IKTargets.Pelvis = ChairTransform.GetRelativeTransform(GroundRefComponent->GetComponentTransform());

		FTransform t5 = IKTargets.Pelvis * GroundRefComponent->GetComponentTransform();
		DrawDebugCoordinateSystem(GetWorld(), t5.GetTranslation(), t5.Rotator(), 30.f, false, 0.05f, 0, 0.5f);
	}

	// apply manual offset
	if (TorsoVerticalOffset != 0.f)
	{
		FVector Offset = SpineRot.Quaternion().GetUpVector() * TorsoVerticalOffset;
		IKTargets.Pelvis.AddToTranslation(Offset);
		IKTargets.Ribcage.AddToTranslation(Offset);
		IKTargets.Head.AddToTranslation(Offset);
	}
}

void UYnnkVrAvatarComponent::BuildTorsoTargetsSG(float DeltaTime)
{
	// Default rotation (X - player forward, Z - up)
	FRotator RootRotation = CalculateRootRotation(IKTargets.Head.GetRotation().GetForwardVector(), IKTargets.Head.GetRotation().GetUpVector());

	bool bLocallyControlled = false;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		bLocallyControlled = OwnerPawn->IsLocallyControlled();
	}

	// Use mean hands rotation for [TorsoRotationSmoothFrames] frames
	FRotator HandsRotation;
	{
		// use quite high tolerance value, because close-to-zero hand direcions aren't representative anyway
		FVector HandsRight = IKTargets.HandRight.GetTranslation() - IKTargets.HandLeft.GetTranslation();
		FVector HandsRightVector2D = HandsRight.GetSafeNormal2D(0.01f);

		// this vector always should look right
		const float HandsToHeadDotProduct = FVector::DotProduct(HandsRight.GetSafeNormal(), RootRotation.Quaternion().GetRightVector());
		bool bHandsInverted = HandsToHeadDotProduct < 0.f;
		if (IKTargets.Head.Rotator().Pitch < -20.f)
		{
			bHandsInverted = (HandsToHeadDotProduct < 0.5f) || (HandsRight.SizeSquared() < 15.f * 15.f);
		}

		// if hands have a direction in a horizontal plane, and hands aren't swapped (which means this rotation also isn't representative)
		if (HandsRightVector2D.SizeSquared() > 0.f && !bHandsInverted)
		{
			int32 UseTorsoRotationSmoothFrames = bLocallyControlled ? ProceduralTorsoRotationSmoothFrames / 4 : ProceduralTorsoRotationSmoothFrames;

			const double Mul = 0.1 / (double)ProceduralTorsoRotationSmoothFrames;
			// get average right direction for last [TorsoRotationSmoothFrames] frames
			TorsoYawByHandsSum -= (TorsoYawByHands[TorsoYawByHandsIndex] * Mul);
			TorsoYawByHandsSum += (HandsRight * Mul);
			TorsoYawByHands[TorsoYawByHandsIndex] = HandsRight;
			TorsoYawByHandsIndex = (TorsoYawByHandsIndex + 1) % ProceduralTorsoRotationSmoothFrames;
		}

		// get rotator by hands from right direction vector
		// later we'll blend this rotator with head rotator
		FVector TargetVector = (TorsoYawByHandsSum).GetSafeNormal();
		if (TargetVector.IsNearlyZero(0.01f))
		{
			TargetVector = HandsRightVector2D.IsNearlyZero() ? TorsoYawByHandsMean : HandsRightVector2D;
		}

		// Rotation by hands. Need to use average for last few seconds
		if (bLocallyControlled)
		{
			if (HandsRight.SizeSquared2D() < 30.f * 30.f)
			{
				float HandsRightAlpha = FMath::Max(0.f, HandsRight.Size2D() - 10.f) / 20.f * 14.f;
				TorsoYawByHandsMean = FMath::VInterpTo(TorsoYawByHandsMean, TargetVector, DeltaTime, HandsRightAlpha);
			}
			else
			{
				//TorsoYawByHandsMean = TargetVector;
				TorsoYawByHandsMean = FMath::VInterpTo(TorsoYawByHandsMean, TargetVector, DeltaTime, 14.f);
			}
		}
		else
		{
			TorsoYawByHandsMean = FMath::VInterpTo(TorsoYawByHandsMean, TargetVector, DeltaTime, 9.f);
		}
		HandsRotation = UKismetMathLibrary::MakeRotFromZY(RootRotation.Quaternion().GetUpVector(), TorsoYawByHandsMean);

		float HeadHandsYawDelta = FRotator::NormalizeAxis(HandsRotation.Yaw - RootRotation.Yaw);
		float MaxRibcageToHeadYaw = 90.f;
		if (FMath::Abs(HeadHandsYawDelta) > MaxRibcageToHeadYaw)
		{
			if (CriticalHandsToHeadRotation == 0)
			{
				CriticalHandsToHeadRotation = (HeadHandsYawDelta > MaxRibcageToHeadYaw ? 1 : -1);
			}

			HandsRotation = FRotator(HandsRotation.Pitch, RootRotation.Yaw + MaxRibcageToHeadYaw * CriticalHandsToHeadRotation, HandsRotation.Roll).GetNormalized();
		}
		else
		{
			CriticalHandsToHeadRotation = 0;
		}
	}

	// avoiding gimbal lock
	float RelativeHeadRotRoll, RelativeHeadRotPitch, RelativeHeadRotYaw;
	YnnkHelpers::GetRotationDeltaInEulerCoordinates(IKTargets.Head.GetRotation(), RootRotation.Quaternion(), RelativeHeadRotRoll, RelativeHeadRotPitch, RelativeHeadRotYaw);

	// Current height of head from 0 to 100% (in percents, not fraction!)
	// Current height of head from 0 to 100% (in percents, not fraction!)
	float HeadHeightCoefficientUnclamped = (IKTargets.Head.GetTranslation().Z + 2.f) / (PlayerHeight + HeadJointOffsetFromHMD.Z) * 100.f;
	float HeadHeightCoefficient = FMath::Clamp(HeadHeightCoefficientUnclamped, 0.f, 100.f);
	// Save camera vertical offset from player height
	StandingPercent = HeadHeightCoefficient;

	if (InternalBodyPose == EYnnkBodyPose::YBP_Sit)
	{
		HeadHeightCoefficientUnclamped = FMath::Max(100.f, HeadHeightCoefficientUnclamped);
		HeadHeightCoefficient = 100.f;
	}

	// Rotation by hands. Need to use average for last few seconds
	FQuat TargetRibcageYawQuat = FQuat::Slerp(RootRotation.Quaternion(), HandsRotation.Quaternion(), 0.7f);

	// Rotation of spine (to get pelvis location later)
	FRotator SpineRot = FRotator(GetCurveValue(CurveSpinePitch, CurveSpinePitch2, HeadHeightCoefficient), RootRotation.Yaw, 0.f).GetNormalized();

	// Desired ribcage rotation is almost ready,
	// but CurveRibcagePitch isn't correct for sitting/leaning, so it needs an adjustment
	float RibcagePitch = GetCurveValue(CurveRibcagePitch, CurveRibcagePitch2, RelativeHeadRotPitch);
	if (HeadHeightCoefficient < 94.f)
	{
		float RibcagePitchMultiplier = FMath::GetMappedRangeValueClamped(FVector2f(75.f, 94.f), FVector2f(0.2f, 1.f), HeadHeightCoefficient);
		RibcagePitch = FMath::Lerp<float>((float)SpineRot.Pitch, RibcagePitch, RibcagePitchMultiplier);
	}
	FRotator RibcageRot = FRotator(RibcagePitch, TargetRibcageYawQuat.Rotator().Yaw, CurveRibcageRoll->GetFloatValue(RelativeHeadRotRoll)).GetNormalized();

	// RibageOffsetFromHeadJoint is offet from IKTargets.Head to IKTargets.Ribcage
	FVector RibcageOffsetFromHead = RibcageRot.RotateVector(RibageOffsetFromHeadJoint);

	// Ribcage is ready
	IKTargets.Ribcage = FTransform(RibcageRot, IKTargets.Head.GetTranslation() + RibcageOffsetFromHead);

	// Pelvis yaw rotation is static until some angle. Then it's returned to the head (root) rotation)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float DeltaYawAbs = FMath::Abs(UKismetMathLibrary::NormalizedDeltaRotator(RootRotation, LastPelvisYaw).Yaw);

		if (!bRestorePelvisYawRotation && InternalBodyPose == EYnnkBodyPose::YBP_Default)
		{
			if (DeltaYawAbs > ProceduralPelvisReturnToHeadAngle || HeadHeightCoefficientUnclamped > 106.f || HeadHeightCoefficientUnclamped < 75.f)
			{
				bRestorePelvisYawRotation = true;
				LastPelvisYawBlendStart = LastPelvisYaw;
				RestorePelvisSoftAngleTime = -1.f;
			}
			else if (DeltaYawAbs > ProceduralPelvisReturnToHeadAngleSoft)
			{
				if (RestorePelvisSoftAngleTime < 0.f)
				{
					RestorePelvisSoftAngleTime = CurrentTime;
				}
				else if (CurrentTime - RestorePelvisSoftAngleTime > 0.7f)
				{
					bRestorePelvisYawRotation = true;
					LastPelvisYawBlendStart = LastPelvisYaw;
					RestorePelvisSoftAngleTime = -1.f;
				}
			}
			else
			{
				RestorePelvisSoftAngleTime = -1.f;
			}
		}
		else if (DeltaYawAbs < 2.f)
		{
			bRestorePelvisYawRotation = false;
		}

		if (bRestorePelvisYawRotation)
		{
			if (DeltaYawAbs < ProceduralPelvisReturnToHeadAngle)
			{
				LastPelvisYaw = FMath::RInterpConstantTo(LastPelvisYaw, RootRotation, DeltaTime, 130.f);
			}
			else
			{
				LastPelvisYaw = FMath::RInterpTo(LastPelvisYaw, RootRotation, DeltaTime, 5.f);
			}
		}
	}
	//GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::White, FString::FromInt((int)bRestorePelvisYaw));
	float ApplyYaw = LastPelvisYaw.Yaw;

	// Rotation of pelvis (applied to IK target)
	FRotator PelvsRot = FRotator(GetCurveValue(CurvePelvisPitch, CurvePelvisPitch2, HeadHeightCoefficient), ApplyYaw, 0.f).GetNormalized();

	// pelvis is ready
	float SpineRealLength = (SpineLength + RibcageBoneOffsetFromJoint.GetTranslation().Size()) * MeshScale.Z;
	IKTargets.Pelvis = FTransform(PelvsRot, IKTargets.Ribcage.GetTranslation() - SpineRot.Quaternion().GetUpVector() * SpineRealLength);

	if (bHasChair)
	{
		IKTargets.Pelvis = ChairTransform.GetRelativeTransform(GroundRefComponent->GetComponentTransform());

		FTransform t5 = IKTargets.Pelvis * GroundRefComponent->GetComponentTransform();
		DrawDebugCoordinateSystem(GetWorld(), t5.GetTranslation(), t5.Rotator(), 30.f, false, 0.05f, 0, 0.5f);
	}

	// apply manual offset
	if (TorsoVerticalOffset != 0.f)
	{
		FVector Offset = SpineRot.Quaternion().GetUpVector() * TorsoVerticalOffset;
		IKTargets.Pelvis.AddToTranslation(Offset);
		IKTargets.Ribcage.AddToTranslation(Offset);
		IKTargets.Head.AddToTranslation(Offset);
	}
}

void UYnnkVrAvatarComponent::BuildFeetTargets(float DeltaTime)
{
	// If speed is high enough, we should animate step (i e. increase StepPhase).
	float CurrHeadSpeed = (float)HeadVelocity2D.Size();
	const float MoveStepTreshold = 40.f;
	const float MoveStepTresholdWhenMoving = 30.f;
	const float LowSpeedThrshold = 25.f;

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Decrease step length for small velocity
	const float StepLength = FMath::GetMappedRangeValueClamped(FVector2f(10.f, 80.f), FVector2f(10.f, StepSize), CurrHeadSpeed);
	const float StepLengthMultiplier = StepLength / 65.f;

	bool bWasWalking = (CurrentTime < HeadWalkingTime);

	// Is head moving?
	// Can't use size squared because of mapped range
	bool bSpeedIsHigh = CurrHeadSpeed > MoveStepTreshold;
	bool bSpeedIsOkAndWasMoving = bWasWalking && CurrHeadSpeed > MoveStepTresholdWhenMoving;
	bool bFawFromParkingLocation = !bSpeedIsHigh && FVector3f::DistSquared2D((FVector3f)IKTargets.Pelvis.GetTranslation(), (FVector3f)SavedIdleBodyLocation) > 15.f * 15.f;
	if (bSpeedIsHigh || bSpeedIsOkAndWasMoving || bFawFromParkingLocation)
	{
		// Synchronize speed of walking animation with step length and walking speed
		const float StepSpeed = 0.5f * CurrHeadSpeed / StepLength;

		// Update step phase
		StepPhase += DeltaTime * StepSpeed;
		if (StepPhase > 1.f) StepPhase -= 1.f;

		HeadWalkingTime = CurrentTime + .6f;
	}

	// Is moving or stopped recently? Show walking animation.
	if (CurrentTime < HeadWalkingTime && bLegsEnabled)
	{
		// walk direction (in component space)
		FRotator WalkDirectionRotator;
		float DirectionMul = 1.f;

		// walk direction as vector
		FVector WalkingForwVec = HeadVelocity2D.GetSafeNormal();
		// don't update rotation if speed is too small
		if (CurrHeadSpeed > LowSpeedThrshold)
		{
			// direction as rotator
			WalkDirectionRotator = UKismetMathLibrary::MakeRotFromX(WalkingForwVec);

			// clamp direction rotator relative to player forward
			float RelativeWalkDirection = UKismetMathLibrary::NormalizedDeltaRotator(WalkDirectionRotator, LastPelvisYaw).Yaw;

			if (FMath::Abs(RelativeWalkDirection) > 90.f)
			{
				DirectionMul = -1.f;
				float AbsWalkDirection = FRotator::NormalizeAxis(RelativeWalkDirection + 180.f + LastPelvisYaw.Yaw);
				WalkDirectionRotator = FRotator(0., AbsWalkDirection, 0.);
			}

			// walking started, need to select first leg (0 is left, 1 is right)
			if (WalkingFirstLeg == INDEX_NONE)
			{
				WalkingFirstLeg = (RelativeWalkDirection < -25.f) ? 0 : 1;
				WalkingStartedTime = CurrentTime;
				SavedWalkDirection = WalkDirectionRotator;
			}
			else
			{
				// smooth walk direction
				SavedWalkDirection = WalkDirectionRotator = FMath::RInterpTo(SavedWalkDirection, WalkDirectionRotator, DeltaTime, 5.f);
			}
		}
		else
		{
			WalkDirectionRotator = SavedWalkDirection;
		}

		// Foot animation phase
		float PhaseLeft = StepPhase, PhaseRight = StepPhase + 0.5f;
		if (WalkingFirstLeg == 0) Swap(PhaseRight, PhaseLeft);

		// sort-of root bone transform
		FTransform Root_CS = FTransform(WalkDirectionRotator, FVector(IKTargets.Pelvis.GetTranslation().X, IKTargets.Pelvis.GetTranslation().Y, 0.f));

		FVector FootRelativePos = FVector(DirectionMul * StepLengthMultiplier * CurveStepOffset->GetFloatValue(PhaseRight), 10.f, CurveStepHeight->GetFloatValue(PhaseRight));
		FRotator FootRelativeRot = FRotator(CurveStepPitch->GetFloatValue(PhaseRight), LastPelvisYaw.Yaw, 0.f);
		FTransform FootR_CS = FTransform(FootRelativeRot, FootRelativePos) * Root_CS; FootR_CS.SetRotation(FootRelativeRot.Quaternion());

		FootRelativePos = FVector(DirectionMul * StepLengthMultiplier * CurveStepOffset->GetFloatValue(PhaseLeft), -10.f, CurveStepHeight->GetFloatValue(PhaseLeft));
		FootRelativeRot = FRotator(CurveStepPitch->GetFloatValue(PhaseLeft), LastPelvisYaw.Yaw, 0.f);
		FTransform FootL_CS = FTransform(FootRelativeRot, FootRelativePos) * Root_CS; FootL_CS.SetRotation(FootRelativeRot.Quaternion());

		float StartTimeLegsAlpha = (CurrentTime - WalkingStartedTime) * 2.f;
		if (StartTimeLegsAlpha > 1.f)
		{
			FootRightRaw = FootR_CS;
			FootLeftRaw = FootL_CS;
		}
		else
		{
			FootRightRaw = UKismetMathLibrary::TInterpTo(FootRightRaw, FootR_CS, DeltaTime, 8.f);
			FootLeftRaw = UKismetMathLibrary::TInterpTo(FootLeftRaw, FootL_CS, DeltaTime, 8.f);
		}

		SavedIdleBodyLocation = IKTargets.Pelvis.GetTranslation();
		SavedBodyLoc_UpdateTime = CurrentTime;
	}
	else // restore default positions
	{
		FTransform Root_CS = FTransform(LastPelvisYaw, FVector(IKTargets.Pelvis.GetTranslation().X, IKTargets.Pelvis.GetTranslation().Y, 0.f));

		const FTransform TargetRight = FTransform(FRotator(LastPelvisYaw), Root_CS.GetTranslation() + LastPelvisYaw.RotateVector(FVector(3.f, 14.f, FootBoneHeight * MeshScale.Z)));
		const FTransform TargetLeft = FTransform(FRotator(LastPelvisYaw), Root_CS.GetTranslation() + LastPelvisYaw.RotateVector(FVector(-3.f, -14.f, FootBoneHeight * MeshScale.Z)));

		if (bLegsEnabled)
		{
			float LegsBlendAlpha = FMath::Max(CurrentTime - SavedBodyLoc_UpdateTime, 0.f);
			if (LegsBlendAlpha < 1.f)
			{
				if (LegsBlendAlpha < 0.5f)
					FootRightRaw = UKismetMathLibrary::TInterpTo(FootRightRaw, TargetRight, DeltaTime, 7.f);
				else
					FootLeftRaw = UKismetMathLibrary::TInterpTo(FootLeftRaw, TargetLeft, DeltaTime, 7.f);
			}

			// reset walking vars	
			WalkingFirstLeg = INDEX_NONE;
			SavedWalkDirection = LastPelvisYaw;
			StepPhase = 0.f;

			// update position
			if (CurrentTime > SavedBodyLoc_UpdateTime + 0.9f)
			{
				if (FVector3f::DistSquared2D((FVector3f)SavedIdleBodyLocation, (FVector3f)IKTargets.Pelvis.GetTranslation()) > 7.f * 7.f)
				{
					SavedIdleBodyLocation = IKTargets.Pelvis.GetTranslation();
					SavedBodyLoc_UpdateTime = CurrentTime;
				}
			}
		}
		else // if (!bLegsEnabled)
		{
			FootRightRaw = UKismetMathLibrary::TInterpTo(FootRightRaw, TargetRight, DeltaTime, 7.f);
			FootLeftRaw = UKismetMathLibrary::TInterpTo(FootLeftRaw, TargetLeft, DeltaTime, 7.f);
		}
	}

	// Trace feet targets for ground to adjust vertical coordinate
	IKTargets.FootRight = TestFootToGround(FootRightRaw, true, false);
	IKTargets.FootLeft = TestFootToGround(FootLeftRaw, false, false);
}

FTransform UYnnkVrAvatarComponent::TestFootToGround(const FTransform& FootTarget, bool bIsRight, bool bInputBoneWorldSpace) const
{
	// at this distance blend input orientation to snapped to ground
	const float BlendRotationDistance = 5.f;
	bool bFootWasModified = false;
	float Distance;
	FVector TargetLoc;
	FRotator TargetRot;

	// convert from (generic orientation, bone translation) to (generic orientation, at the ground)
	const FTransform& SpaceConverter = bIsRight ? FootRightToFloorGen : FootLeftToFloorGen;
	FTransform UpdatedFootTarget = FootTarget;
	FTransform FootGround;
	float AnimationVerticalOffset = 0.f;

	if (bInputBoneWorldSpace)
	{
		// to generic orientation
		const int32 FootBoneIndex = bIsRight ? FootRightIndex : FootLeftIndex;
		UpdatedFootTarget = FTransform().GetRelativeTransform(SkeletonTransforms[FootBoneIndex]) * UpdatedFootTarget;

		// from IK target to location at the foot bottom
		FootGround = SpaceConverter * UpdatedFootTarget;

		FTransform UpdatedFootTargetRel = FootGround.GetRelativeTransform(GroundRefComponent->GetComponentTransform());
		AnimationVerticalOffset = FMath::Clamp(UpdatedFootTargetRel.GetTranslation().Z, 0.f, 10.f);
	}
	else
	{
		// from IK target to location at the foot bottom
		FootGround = SpaceConverter * UpdatedFootTarget;

		AnimationVerticalOffset = FMath::Clamp(FootGround.GetTranslation().Z, 0.f, 10.f);

		// to world space
		UpdatedFootTarget *= GroundRefComponent->GetComponentTransform();
		FootGround *= GroundRefComponent->GetComponentTransform();
	}

	if (bFlatGround)
	{
		float GroundZ = GroundRefComponent->GetComponentLocation().Z;
		Distance = FootGround.GetTranslation().Z - GroundZ;
		TargetRot = FRotator(0.f, FootGround.Rotator().Yaw, 0.f);

		if (Distance <= 0.f)
		{
			TargetLoc = FootGround.GetTranslation();
			TargetLoc.Z = GroundZ;
			bFootWasModified = true;
		}
	}
	else
	{
		// need to trace
		UWorld* World = GetWorld();

		FCollisionQueryParams stTraceParams;
		stTraceParams = FCollisionQueryParams(FName(TEXT("SensXrGroundTrace")), true, GetOwner());
		stTraceParams.bTraceComplex = true;
		stTraceParams.bReturnPhysicalMaterial = false;

		FHitResult OutHit;
		const FVector Loc = FootGround.GetTranslation();
		const FVector Offset = GroundRefComponent->GetUpVector() * BlendRotationDistance;

		if (bTraceGroundByObjectType)
		{
			GetWorld()->LineTraceSingleByObjectType(OutHit, Loc + Offset * 8.f, Loc - Offset * 1.5f, FCollisionObjectQueryParams(GroundCollisionChannel), stTraceParams);
		}
		else
		{
			GetWorld()->LineTraceSingleByChannel(OutHit, Loc + Offset * 8.f, Loc - Offset * 1.5f, GroundCollisionChannel, stTraceParams);
		}
		// foot is high, don't modify
		if (!OutHit.bBlockingHit)
		{
			return FootTarget;
		}

		Distance = Loc.Z - OutHit.ImpactPoint.Z;
		// foot is high, don't modify
		if (Distance > BlendRotationDistance)
		{
			return FootTarget;
		}

		const FVector FootForward = FVector::VectorPlaneProject(FootGround.GetRotation().GetForwardVector(), OutHit.ImpactNormal);
		TargetRot = UKismetMathLibrary::MakeRotFromXY(FootForward, FootGround.GetRotation().GetRightVector());
		if (Distance <= 0.f)
		{
			TargetLoc = FootGround.GetTranslation();
			TargetLoc.Z = OutHit.ImpactPoint.Z + AnimationVerticalOffset;
			bFootWasModified = true;
		}
	}

	if (Distance <= 0.f)
	{
		// convert TargetLoc from ground to foot bone
		TargetLoc = (FTransform().GetRelativeTransform(SpaceConverter) * FTransform(TargetRot, TargetLoc)).GetTranslation();

		UpdatedFootTarget.SetTranslation(TargetLoc);
		UpdatedFootTarget.SetRotation(TargetRot.Quaternion());
	}
	else if (Distance < BlendRotationDistance)
	{
		FQuat Curr = FootGround.GetRotation();
		FQuat Targ = TargetRot.Quaternion();
		FQuat NewRot = FQuat::Slerp(Targ, Curr, Distance / BlendRotationDistance);

		UpdatedFootTarget.SetRotation(
			(FTransform().GetRelativeTransform(SpaceConverter) * FTransform(NewRot)).GetRotation()
		);
	}

	UpdatedFootTarget.AddToTranslation(FVector(0.f, 0.f, FeetVerticalOffset));
	UpdatedFootTarget = UpdatedFootTarget.GetRelativeTransform(GroundRefComponent->GetComponentTransform());

	return UpdatedFootTarget;
}

float UYnnkVrAvatarComponent::FilterNNValue(int32 Index, float Value, float* ValuesArray, float& SD_Sum, float& Soften_Sum, bool bPelvis)
{
	// number of frames to soften
	int32 SoftingNumber = FMath::Clamp((int32)NeuralNet_SmoothFrames, 3, SD_NUM);

	// Update variables
	float OldValue = ValuesArray[Index];
	int32 OldSoftIndex = (Index - SoftingNumber);
	if (OldSoftIndex < 0) OldSoftIndex += SD_NUM;

	SD_Sum += (Value - OldValue);

	// Calc standard deviation and mean

	float sd = 0.f;
	for (int32 i = 0; i < SD_NUM; i++)
	{
		sd += FMath::Square(ValuesArray[i] - SD_Sum);
	}
	sd = FMath::Sqrt(sd / (SD_NUM - 1));

	float Mean = SD_Sum / SD_NUM;
	const float Delta = Value - Mean;

	const float sigma3 = (bPelvis ? sd * 2.f : sd * 3.f) * NeuralNet_FilterPower;
	const float sigma2 = (bPelvis ? sd * 1.5f : sd * 2.f) * NeuralNet_FilterPower;

	if (FMath::Abs(Delta) > sigma3)
	{
		int32 PrevIndex1 = Index - 1; if (PrevIndex1 < 0) PrevIndex1 = SD_NUM - 1;
		int32 PrevIndex2 = PrevIndex1 - 1; if (PrevIndex2 < 0) PrevIndex2 = SD_NUM - 1;
		int32 PrevIndex3 = PrevIndex2 - 1; if (PrevIndex3 < 0) PrevIndex3 = SD_NUM - 1;

		Value = ValuesArray[PrevIndex1] + (ValuesArray[PrevIndex1] - ValuesArray[PrevIndex2]) * (ValuesArray[PrevIndex2] - ValuesArray[PrevIndex3]) * 0.5f;
	}
	else if (FMath::Abs(Delta) > sigma2)
	{
		int32 PrevIndex1 = Index - 1; if (PrevIndex1 < 0) PrevIndex1 = SD_NUM - 1;
		Value = ValuesArray[PrevIndex1] + (Delta > 0.f ? sd * 2.f : sd * -2.f);
	}	

	// Save new value
	ValuesArray[Index] = Value;

	if (FMath::Abs((int)Soften_Sum) > 300 * SoftingNumber)
	{
		Soften_Sum = 0.f;
	}

	// Soften	

	float OutVal = 0.f;
	for (int32 i = 0; i < SoftingNumber; i++)
	{
		OutVal += ValuesArray[(SD_NUM + Index - i) % SD_NUM];
	}
	return OutVal / (float)SoftingNumber;

	/*
	// @TODO: investigate
	// for some reason doesn't work in packaged project
	Soften_Sum += (Value - ValuesArray[OldSoftIndex]);
	return Soften_Sum / (float)SoftingNumber;
	*/
}

FTransform UYnnkVrAvatarComponent::GetHandTargetTransform(bool bRightHand, bool bWorldSpace) const
{
	FTransform TargetTr;
	if (bRightHand)
	{
		if (bHandAttachedRight && HandParentRight)
		{
			TargetTr = HandAttachSocketRight.IsNone()
				? HandParentRight->GetComponentTransform()
				: HandParentRight->GetSocketTransform(HandAttachSocketRight);

			if (!HandAttachTransformRight.Equals(FTransform::Identity))
			{
				TargetTr = HandAttachTransformRight * TargetTr;
			}
		}
		else
		{
			TargetTr = HandRightComponent->GetComponentTransform();
		}
	}
	else /* left hand */
	{
		if (bHandAttachedLeft && HandParentLeft)
		{
			TargetTr = HandAttachSocketLeft.IsNone()
				? HandParentLeft->GetComponentTransform()
				: HandParentLeft->GetSocketTransform(HandAttachSocketLeft);

			if (!HandAttachTransformLeft.Equals(FTransform::Identity))
			{
				TargetTr = HandAttachTransformLeft * TargetTr;
			}
		}
		else
		{
			TargetTr = HandLeftComponent->GetComponentTransform();
		}
	}

	if (!bWorldSpace)
	{
		const FTransform InputBaseTr = GroundRefComponent->GetComponentTransform();
		TargetTr = TargetTr.GetRelativeTransform(InputBaseTr);
	}

	return TargetTr;
}

FTransform UYnnkVrAvatarComponent::GetHMDTransform(bool bWorldSpace) const
{
	if (!bInitialized) return FTransform::Identity;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	bool bLocallyControlled = !OwnerPawn || OwnerPawn->IsLocallyControlled();

	FTransform t;
	if (GetIsReplicated() && !bLocallyControlled)
	{
		t = IKInputsRep.HMD.AsTransform();
		if (bWorldSpace)
		{
			t *= GroundRefComponent->GetComponentTransform();
		}
	}
	else
	{
		t = HeadComponent->GetComponentTransform();
		if (!bWorldSpace)
		{
			t = t.GetRelativeTransform(GroundRefComponent->GetComponentTransform());
		}
	}

	return t;
}

FString UYnnkVrAvatarComponent::GetResourcesPath()
{
	FString ResourcesPath;
	FString PlatformDir = TEXT("Win64");

#if PLATFORM_ANDROID
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
#endif

#if WITH_EDITOR
	auto ThisPlugin = IPluginManager::Get().FindPlugin(TEXT("VRIKBody"));
	if (ThisPlugin.IsValid())
	{
		ResourcesPath = FPaths::ConvertRelativePathToFull(ThisPlugin->GetBaseDir()) / TEXT("Resources");
	}
	else
	{
		ResourcesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Binaries") / PlatformDir;
	}
#else
	ResourcesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Binaries") / PlatformDir;
#endif
	return ResourcesPath;
}

float UYnnkVrAvatarComponent::GetCurveValue(UCurveFloat* Curve1, UCurveFloat* Curve2, float X) const
{
	if (SpineCurvesAlpha == 0.f || !IsValid(Curve2) || Curve1 == Curve2)
	{
		return Curve1->GetFloatValue(X);
	}
	else
	{
		return FMath::Lerp(Curve1->GetFloatValue(X), Curve2->GetFloatValue(X), SpineCurvesAlpha);
	}
}

bool UYnnkVrAvatarComponent::IsGroundComponentMoving() const
{
	if (bInitialized)
	{
		FTransform SubBase = GroundRefComponent->GetComponentTransform();
		return LastRefTransform.EqualsNoScale(SubBase, 0.01f);
	}
	else return false;
}

FRotator UYnnkVrAvatarComponent::CalculateRootRotation(const FVector& ForwardVector, const FVector& UpVector) const
{
	return YnnkHelpers::CalculateRootRotation(ForwardVector, UpVector);
}

void UYnnkVrAvatarComponent::InitializeFingersReplication(float UpdateInterval, UVRIKFingersFKIKSolver* RightSolver, UVRIKFingersFKIKSolver* LeftSolver)
{
	bReplicateFingers = IsValid(RightSolver) && IsValid(LeftSolver);
	if (bReplicateFingers)
	{
		RightHandSolver = RightSolver;
		LeftHandSolver = LeftSolver;
		FingersUpdateInterval = UpdateInterval;
	}
}

void UYnnkVrAvatarComponent::RefreshCalibrationForAllConnectedPlayers()
{
	if (__uev_IsServer)
	{
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled())
		{
			ClientsUpdateCalibration(PlayerHeight, PlayerArmSpan);
		}
		else
		{
			ClientsRefreshCalibrationForAll();
		}		
	}
}

void UYnnkVrAvatarComponent::ClientsRefreshCalibrationForAll_Implementation()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled())
	{
		ServerUpdateCalibration(PlayerHeight, PlayerArmSpan);
	}
}

void UYnnkVrAvatarComponent::SetChairTransform(const FTransform& ChairInWorldSpace)
{
	bHasChair = true;
	ChairTransform = ChairInWorldSpace;
	SetBodyPose(EYnnkBodyPose::YBP_Sit);
}

void UYnnkVrAvatarComponent::ResetChairTransform()
{
	bHasChair = false;
	SetBodyPose(EYnnkBodyPose::YBP_Default);
}

#undef TRUE
#undef FALSE
#undef PushTransformToArr
#undef ReadTransformFromArr