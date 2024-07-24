// VR IK Body Plugin
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRIKBody.h"
#include "Animation/PoseSnapshot.h"
#include "ReferenceSkeleton.h"
#include "Animation/AnimationAsset.h"
#include "VRIKSkeletalMeshTranslator.generated.h"

//Logging category
DECLARE_LOG_CATEGORY_EXTERN(LogVRIKSkeletalMeshTranslator, Log, All);

// Custom-to-universal skeleton bone orientation translation
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FBoneRotatorSetup
{
	GENERATED_USTRUCT_BODY()

	// X direction along the main bone axis
	EAxis::Type ForwardAxis;
	float ForwardDirection;

	// Y direction, right relative to Forward-Up
	EAxis::Type HorizontalAxis;
	float RightDirection;
	// Direction to the outer side. Can be equal to RightDirection or opposite. Used to calculate knee/elbows in TwoBoneIK.
	float ExternalDirection;

	// Z Direction 
	EAxis::Type VerticalAxis;
	float UpDirection;
};

// Custom-to-universal skeleton bone orientation translation
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FSkeletalMeshSetup
{
	GENERATED_USTRUCT_BODY()

	// Name of Skeletal Mesh Object
	UPROPERTY()
	FName SkeletalMeshName;

	// Upperarm Length not scaled
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float MeshUpperarmLength;
	// Lowerarm Length not scaled
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float MeshForearmLength;
	// Distance from one palm to another when hands are horisontal in T Pose
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float MeshHandsFullLength;
	// Distance from ground to head bone
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float MeshHeight;
	// Upperarm Length scaled
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float fUpperarmLength;
	// Lowerarm Length scaled
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	float fForearmLength;
	
	// Local Transform of Right Collerbone
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform ShoulderToRibcageRight;
	// Local Transform of Left Collerbone
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform ShoulderToRibcageLeft;
	// Local Transform of Right Upperarm
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform UpperarmToShoulderRight;
	// Local Transform of Left Upperarm
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform UpperarmToShoulderLeft;
	// Local Transform of Head Bone
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform HeadToNeck;
	// Local Transform of Ribcage Bone
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform RibcageToSpine;
	// Right foot relative to ground and forward
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform FootToGroundRight;
	// Left foot relative to ground and forward
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FTransform FootToGroundLeft;
	UPROPERTY()
	FTransform RelativeRibcageOffset;

	// Spine bones relative transforms
	UPROPERTY()
	TMap<FName, FTransform> SpineBones;
	// Neck bones relative transforms
	UPROPERTY()
	TMap<FName, FTransform> NeckBones;
	// Reversed list of intermediate spine bones names (spine_02, spine_01, ...)
	UPROPERTY()
	TArray<FName> SpineBoneNames;
	// Reversed list of intermediate neck bones names: (neck_03, neck_02, neck_01, ...)
	UPROPERTY()
	TArray<FName> NeckBoneNames;
	// Length of spine from pelvis to ribcage
	float SpineLength;

	// Scale of Mesh Component in local space
	UPROPERTY(BlueprintReadOnly, Category = "Skeletal Mesh Setup")
	FVector MeshScale;

	// Skeleton Bones: Orientation Solvers

	// Pelvis Orientation
	UPROPERTY()
	FBoneRotatorSetup PelvisRotation;
	// Ribcage Orientation
	UPROPERTY()
	FBoneRotatorSetup RibcageRotation;
	// Head Orientation
	UPROPERTY()
	FBoneRotatorSetup HeadRotation;
	// Right Shoulder Orientation
	UPROPERTY()
	FBoneRotatorSetup RightShoulderRotation;
	// Left Shoulder Orientation
	UPROPERTY()
	FBoneRotatorSetup LeftShoulderRotation;
	// Right Arm Orientation
	UPROPERTY()
	FBoneRotatorSetup RightHandRotation;
	// Left Arm Orientation
	UPROPERTY()
	FBoneRotatorSetup LeftHandRotation;
	// Right Leg Orientation
	UPROPERTY()
	FBoneRotatorSetup RightLegRotation;
	// Left Leg Orientation
	UPROPERTY()
	FBoneRotatorSetup LeftLegRotation;
	// Right Hand Orientation
	UPROPERTY()
	FBoneRotatorSetup PalmRightRotation;
	// Left Hand Orientation
	UPROPERTY()
	FBoneRotatorSetup PalmLeftRotation;
	// Right Hand Orientation
	UPROPERTY()
	FBoneRotatorSetup FootRightRotation;
	// Left Hand Orientation
	UPROPERTY()
	FBoneRotatorSetup FootLeftRotation;
	// Rotation of root bone relative in component space
	UPROPERTY()
	FRotator RootBoneRotation;

	// Bones need to be evaluated in the first pass
	UPROPERTY()
	TArray<bool> FirstPassBones;

	FSkeletalMeshSetup()
		: SkeletalMeshName(NAME_None)
		, MeshUpperarmLength(0.f)
		, MeshForearmLength(0.f)
		, MeshHandsFullLength(0.f)
		, MeshHeight(0.f)
		, fUpperarmLength(0.f)
		, fForearmLength(0.f)
		, SpineLength(0.f)
		, MeshScale(FVector::ZeroVector)
		, RootBoneRotation(FRotator::ZeroRotator)
	{}
};

/*
This component transforms internal skeleton data from VRIKBody component to PoseSnapshot for any custom skeletal mesh
*/
UCLASS(
	ClassGroup=(IKBody),
	meta=(BlueprintSpawnableComponent)
)
class VRIKBODYRUNTIME_API UVRIKSkeletalMeshTranslator : public UActorComponent
{
	GENERATED_BODY()

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:	
	// Sets default values for this component's properties
	UVRIKSkeletalMeshTranslator();

	// ---------------------------------- PROPERTIES ---------------------------------- //

	// Skeletal Mesh Bones Map

	// Name of Root Bone at the controlled Skeletal Mesh
	// Keep it as None if your skeleton doesn't have a separate root bone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName RootBone;
	// Name of Pelvis Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName Pelvis;
	// Name of Ribcage Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName Ribcage;
	// Name of Head Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName Head;
	// Name of Right Collarbone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName ShoulderRight;
	// Name of Right Upperarm Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName UpperarmRight;
	// Name of Right Lowerarm Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName LowerarmRight;
	// Name of Right Hand Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName PalmRight;
	// Name of Left Collarbone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName ShoulderLeft;
	// Name of Left Upperarm Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName UpperarmLeft;
	// Name of Left Lowerarm Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName LowerarmLeft;
	// Name of Left Hand Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName PalmLeft;
	// Name of Right Thigh Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName ThighRight;
	// Name of Right Calf Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName CalfRight;
	// Name of Right Foot Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName FootRight;
	// Name of Left Thigh Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName ThighLeft;
	// Name of Left Calf Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName CalfLeft;
	// Name of Left Foot Bone at the controlled Skeletal Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	FName FootLeft;
	// Twist bones and multipliers for right forearm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TMap<FName, float> LowerarmTwistsRight;
	// Twist bones and multipliers for left forearm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TMap<FName, float> LowerarmTwistsLeft;
	// Twist bones and multipliers for right forearm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TMap<FName, float> UpperarmTwistsRight;
	// Twist bones and multipliers for left forearm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TMap<FName, float> UpperarmTwistsLeft;

	// Rotators applied to spine bones from pelvis to ribcage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TArray<FRotator> ManualSpineCorrection;

	UPROPERTY(EditAnywhere, Category = "Setup")
	bool bUpdateAutomaticallyInTick;

	// Apply player's height to Mesh Relative Scale?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	bool bRescaleMesh;
	// Mesh scale coefficient (only if RescaleMesh is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float ScaleMultiplierVertical;
	// Mesh scale coefficient (only if RescaleMesh is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float ScaleMultiplierHorizontal;
	// Should scale arm starting at upperarm bone or collarbone?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bHandScaleFromUpperbone;
	// If true, automatically moves skeletal mesh component when you call UpdateSkeleton()
	// Otherwise, keeps default mesh position and only moves bones.
	// Ignored if Pelvis is root bone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	bool bRootMotion;
	// If true, root bone always keep zero rotation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	bool bLockRootBoneRotation;
	// Turn on if elbows are bended at the wrong side
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bInvertElbows;
	// Component makes additional Two-Bone IK calcualtion for hands when updating PoseSnapshot
	// to make sure skeletal mesh sizes are taken into account correctly and hands follow
	// motion cotrollers precisely. Disable to save CPU time.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	bool bHandsIKLateUpdate;
	// This parameter allow to shift torso up and down to proper fit custom skeletal mesh. This setting
	// is not affected by MeshScale.Z
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float SpineVerticalOffset;
	// Half height of skeletal mesh head (to calculate full height)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float MeshHeadHalfHeight;
	// Maximum possible twist value
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float HandTwistLimit;
	// Disable torso bending (for debug purposes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bDisableFlexibleTorso;
	// Legs are calculated for valid VirtualBone_FootRight and VirtualBone_FootLeft
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bCalculateAnimatedLegs;
	// Orientation adjustment used for blending legs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float ComponentForwardYaw;
	// Decrease this value if spine has many bones and bended incorrectly
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float SpineBendingMultiplier;
	// Pelvis rotation in world space
	UPROPERTY()
	FRotator PelvisRotator_WS;
	// Debug settings: flexible spine
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawSpineDebug;
	// Debug settings: bones setup
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawSkeletonSetup;

	// Output data: location of right foot with tracing
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	FVector FootRight_Location;

	// Output data: location of left foot with tracing
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	FVector FootLeft_Location;

	// Output data: body movement speed
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	float WalkingSpeed;

	// Output data: direction of movement relative to body forward direction
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	float WalkingYaw;

	// ---------------------------------- FUNCTIONS ---------------------------------- //

	// Initialize references and build information about controlled skeletal mesh. References can be automatically extracted from the owner actor
	// if it has VRIKBody component and a single Skeletal Mesh Component
	// Skeletal Mesh Component must contain a Skeletal Mesh object in T Pose
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	bool Initialize(class USkinnedMeshComponent* ControlledSkeletalMesh = nullptr, class UVRIKBody* VRIKBodyComponent = nullptr);

	// Is component initialized or not?
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	bool IsInitialized() { return bIsInitialized; };

	// Get current skeletal mesh bones setup. Use this function to save/restore mesh data without reinitialization.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void GetSkeletalMeshSetup(FSkeletalMeshSetup& ReturnValue) { ReturnValue = BodySetup; };

	// Load skeletal mesh bones setup to component. Use this function to save/restore mesh data without reinitialization.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void RestoreSkeletalMeshSetup(const FSkeletalMeshSetup& SkeletalMeshSetup);

	// Call after VRIKBody::CalculateFrame to load calculated data and update skeletal mesh position if Root Motion is enabled
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void UpdateSkeleton();

	// Update data for quickly moving actor. Call before GetLastPose.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void RefreshComponentPosition(bool bUpdateCapturedBones);

	// Get current pose from VRIKBody component adjusted for controlled skeletal mesh
	// @return Pose Snapshot object to use in Animation Blueprint
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	bool GetLastPose(UPARAM(ref) FPoseSnapshot& ReturnPoseValue);

	// Get last transform of pelvis bone in component space
	UFUNCTION(BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	FTransform GetLastPelvisBone() const;

	// Get last transform of ribcage bone in component space
	UFUNCTION(BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	FTransform GetLastRibcageBone() const;

	// Put player in virtual chair
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void SetChairTransform(const FTransform& ChairInWorldSpace);

	// Take player from the virtual chair
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void ResetChairTransform();

	// Update poseable mesh using local transform from FPoseSnapshot
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void UpdatePoseableMesh(class UPoseableMeshComponent* TargetMeshComp, UPARAM(ref) const FPoseSnapshot& PoseSnapshot);

	// @return current Location and Rotation of Skeletal Mesh Component in World Space in Root Motion is enabled
	// or predicted character position if Root Motion is disabled
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	void GetMeshWorldTransform(FVector& Location, FRotator& Rotation) { Location = ComponentWorldTransform.GetTranslation(); Rotation = ComponentWorldTransform.Rotator(); };

	// Debug Function. Returns text information describing bone orientation.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	FString PrintBoneInfo(const FBoneRotatorSetup& BoneSetup);

	// Function mark body as calibrated and allow to receive input from VRIKBody component.
	// Note: function isn't replicated, call it on all clients manually.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void ForceSetComponentAsCalibrated() { VRIKBody_OnCalibrationComplete(); };

	// Debug function. Returns calculated component yaw from pelvis forward and up vectors.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	float DebugGetYawRotation(const FVector ForwardVector, const FVector UpVector);

	// Debug function. Draws orientation axes for all calculated bones of the skeletal mesh.
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void DebugShowBonesOrientation();

	// Overrride scaling values based on calibration data by default.
	// Note:
	// ScaleMultiplierVertical and ScaleMultiplierHorizontal will affect these scale coefficients
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void OverrideMeshScale(float HeightScale, float ArmScale);

	// Is component calibrated?
	UFUNCTION(BlueprintPure, Category = "VR IK Skeletal Mesh Translator")
	bool IsCalibrated() const { return bIsCalibrated; }

	// Send calibration data to remote machines. Call this function on local PC which was calibrated locally
	// and then connected to server.
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "VR IK Skeletal Mesh Translator")
	void Server_UpdateCalibration(float HeightScale, float ArmScale);
	bool Server_UpdateCalibration_Validate(float HeightScale, float ArmScale) { return true; };
	void Server_UpdateCalibration_Implementation(float HeightScale, float ArmScale)
	{
		Local_UpdateCalibration(HeightScale, ArmScale);
		Clients_UpdateCalibration(HeightScale, ArmScale);
	}

	// Apply rotation to root bone
	void SetVerticalAxisRotation(FRotator& OutRot, float Value) const;

protected:
	// Are BodySetup and references initialized?
	UPROPERTY()
	bool bIsInitialized;

	UPROPERTY()
	bool bIsCalibrated;

	UPROPERTY()
	bool bTrial;

	UPROPERTY()
	bool bHasRootBone;

	UPROPERTY()
	int32 TrialMonth;

	UPROPERTY()
	int32 TrialYear;

	// Reference to player's VRIKBody Component
	UPROPERTY()
	UVRIKBody* VRIKSolver;

	// Reference to controlled skeletal mesh
	UPROPERTY()
	class USkinnedMeshComponent* BodyMesh;

	// Current Root Bone Transform
	UPROPERTY()
	FTransform ComponentWorldTransform;
	UPROPERTY()
	FTransform ComponentActorTransform;

	UPROPERTY()
	bool bHasChair = false;
	FTransform ChairTransform;

	// Current Skeletal Mesh Setup
	UPROPERTY()
	FSkeletalMeshSetup BodySetup;

	// Map of current bone transforms in world space
	UPROPERTY()
	FIKBodyData CapturedBodyRaw;

	// Map of current bone transforms in world space
	UPROPERTY()
	TArray<FTransform> CapturedSkeletonR;
	UPROPERTY()
	TArray<FTransform> CapturedSkeletonL;
	UPROPERTY()
	FQuat LastRotationOffset;

	// Map of current bone transforms in world space
	UPROPERTY()
	TMap<FName, FTransform> CapturedBody;
	UPROPERTY()
	FTransform LastPelvisBonePosition;
	UPROPERTY()
	FTransform LastRibcageBonePosition;

	// Captured Body: calculated forearm twist
	UPROPERTY()
	float LowerarmTwistRight;
	// Captured Body: calculated forearm twist
	UPROPERTY()
	float LowerarmTwistLeft;
	// Captured Body: calculated forearm twist
	UPROPERTY()
	float UpperarmTwistRight;
	// Captured Body: calculated forearm twist
	UPROPERTY()
	float UpperarmTwistLeft;

	UPROPERTY()
	bool bComponentWasMovedInTick;

	// Blend space settings
	UPROPERTY()
	FRotator BodyDirection;
	// Animations data
	UPROPERTY()
	TArray<FBlendSampleData> BlendSampleDataCache;
	// 
	UPROPERTY()
	float CurrentBlendAnimationAlpha;

	// ---------------------------------- FUNCTIONS ---------------------------------- //

	UFUNCTION()
	void CalcTorsoIK();

	// Save Skeletal mesh bones orientation
	UFUNCTION()
	bool UpdateSkeletonBonesSetup();

	// Load bones from VRIKBody, apply reorientation and calculate intermediate bones
	FORCEINLINE bool UpdateBonesState();
	void ApplyBlendSpace();

	UFUNCTION()
	void CalculateSpine(const FTransform& Start, FTransform& End, const TArray<FName>& RelativeBoneNames, const TMap<FName, FTransform>& RelativeTransforms, TArray<FTransform>& ResultTransforms, bool bDrawDebugGeometry, bool SnapToEnd = false);

	// Helper. Find axis of rotator the closest to being parallel to the specified vectors. Returns +1.f in Multiplier if co-directed and -1.f otherwise
	FORCEINLINE EAxis::Type FindCoDirection(const FRotator& BoneRotator, const FVector& Direction, float& ResultMultiplier) const;
	// Helper. Returns rotation value around axis.
	FORCEINLINE float GetAxisRotation(const FRotator& Rotator, EAxis::Type Axis);
	// Helper. Set Rotator value for a specified axis
	void SetAxisRotation(FRotator& Rotator, EAxis::Type Axis, float Value);
	FORCEINLINE FRotator BuildTransformFromAxes(const FRotator& SourceRotator, EAxis::Type XAxis, float ForwardDirection, EAxis::Type YAxis, float RightDirection, EAxis::Type ZAxis, float UpDirection);
	FORCEINLINE float GetYawRotation(const FVector& ForwardVector, const FVector& UpVector);
	FORCEINLINE FVector GetScaleInDirection(EAxis::Type Axis, float Value) { FVector ret = FVector(1.f, 1.f, 1.f); if (Axis == EAxis::X) ret.X = Value; else if (Axis == EAxis::Y) ret.Y = Value; else ret.Z = Value; return ret; };
	FORCEINLINE FTransform RestoreBoneTransformInWS(const FPoseSnapshot& CurrentPose, const FReferenceSkeleton& RefSkeleton, const int32 CurrentBoneIndex);
	FORCEINLINE float GetLateUpperarmTwist(const FTransform& Collarbone, const FTransform& Upperarm, float CurrentValue, bool bSide);
	// Helper
	UFUNCTION()
	void DrawAxes(UWorld* World, const FVector& Location, FBoneRotatorSetup RotSetup, FRotator Rot);
	// Helper
	UFUNCTION()
	FRotator MakeRotByTwoAxes(EAxis::Type MainAxis, FVector MainAxisDirection, EAxis::Type SecondaryAxis, FVector SecondaryAxisDirection);

	UFUNCTION()
	void VRIKBody_OnCalibrationComplete();

	// Math. Calculate IK transforms for two-bone chain.
	// Need to be refactored to consider skeleton bones orientation!
	UFUNCTION()
	void CalculateTwoBoneIK(const FVector& ChainStart, const FVector& ChainEnd, const FVector& JointTarget,
							const float UpperBoneSize, const float LowerBoneSize,
							FTransform& UpperBone, FTransform& LowerBone, FBoneRotatorSetup& LimbSpace,
							bool bInvertBending, bool bSnapToChainEnd = false);

	// Replication
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CalibrateComponent(float RealWidth, float RealHeight);
	bool Server_CalibrateComponent_Validate(float RealWidth, float RealHeight) { return true; };
	void Server_CalibrateComponent_Implementation(float RealWidth, float RealHeight);

	void Local_CalibrateComponent(float RealWidth, float RealHeight);

	UFUNCTION(NetMulticast, Reliable)
	void Clients_CalibrateComponent(float RealWidth, float RealHeight);
	void Clients_CalibrateComponent_Implementation(float RealWidth, float RealHeight);

	void Local_UpdateCalibration(float HeightScale, float ArmScale);

	UFUNCTION(NetMulticast, Reliable)
	void Clients_UpdateCalibration(float HeightScale, float ArmScale);
	void Clients_UpdateCalibration_Implementation(float HeightScale, float ArmScale)
	{
		Local_UpdateCalibration(HeightScale, ArmScale);
	}
};
