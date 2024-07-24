// VR IK Body Plugin
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "IMotionController.h"
#include "Engine/NetSerialization.h"
#include "YnnkVRIKTypes.h"
#include "VRIKBody.generated.h"

#define HEIGHT_INIT					0
#define HEIGHT_MODIFY				1
#define HEIGHT_STABLE				2
#define CORR_POINTS_NUM				10
#define CORR_SAVE_POINTS_NUM		100
#define UpperarmForearmRatio		1.08f
#define ThighCalfRatio				1.f

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIKBodyRepeatable, int32, Iteration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIKBodySimple);

DECLARE_LOG_CATEGORY_EXTERN(LogVRIKBody, Log, All);

#define _vrikversion12

/* VR headset and controllers input mode:
DirectVRInput - input from UHeadMountedDisplayFunctionLibrary and I Motioncontroller interface
InputFromComponents - input from scene components (recommended)
InputFromVariables - input from variables manually updated in blueprint */
UENUM(BlueprintType, Blueprintable)
enum class EVRInputSetup : uint8
{
	DirectVRInput				UMETA(DisplayName = "Direct VR Input"),
	InputFromComponents			UMETA(DisplayName = "Input From Components"),
	InputFromVariables			UMETA(DisplayName = "Input From Variables"),
	EmulateInput				UMETA(DisplayName = "Emulate (Debug)")
};

/* Current body orientation state. Body calculation depends on this state. */
UENUM(BlueprintType, Blueprintable)
enum class EBodyOrientation : uint8
{
	Stand					UMETA(DisplayName = "Stand"),
	Sit						UMETA(DisplayName = "Sit"),
	Crawl					UMETA(DisplayName = "Crawl"),
	LieDown_FaceUp			UMETA(DisplayName = "Lie Down with Face Up"),
	LieDown_FaceDown		UMETA(DisplayName = "Lie Down with Face Down")
};

/* Data set describing calibrated body state. Required to save and load body params on player respawn */
USTRUCT(Blueprintable)
struct VRIKBODYRUNTIME_API FCalibratedBody
{
	GENERATED_USTRUCT_BODY()

	/* Player body width (only for calubrated body) */
	UPROPERTY()
	float fBodyWidth;
	/* Player height from ground to eyes */
	UPROPERTY()
	float CharacterHeight;
	/* Clear vertical distance from ground to camera without any adjustments */
	UPROPERTY()
	float CharacterHeightClear;
	/* Clear horizontal distance between two motion controllers at T pose */
	UPROPERTY()
	float ArmSpanClear;
	/* Feet-pelvis approximate vertical distance */
	UPROPERTY()
	float LegsLength;
	/* Location of a neck relative to VR headset */
	UPROPERTY()
	FVector vNeckToHeadsetOffset;
	/* Approximate pelvis-to-ricage length */
	UPROPERTY()
	float fSpineLength;
	/* Player's hand length (only for calubrated body) */
	UPROPERTY()
	float fHandLength;

	FCalibratedBody()
		: fBodyWidth(0.f)
		, CharacterHeight(0.f)
		, CharacterHeightClear(0.f)
		, ArmSpanClear(0.f)
		, LegsLength(0.f)
		, vNeckToHeadsetOffset(FVector::ZeroVector)
		, fSpineLength(0.f)
		, fHandLength(0.f)
	{}
};

/* Internal struct to store VR Input history */
USTRUCT()
struct VRIKBODYRUNTIME_API FVRInputData
{
	GENERATED_USTRUCT_BODY()

	/* VR Headset Location */
	UPROPERTY()
	FVector HeadLoc;
	/* Left Motion Controller Location */
	UPROPERTY()
	FVector LeftHandLoc;
	/* Right Motion Controller Location */
	UPROPERTY()
	FVector RightHandLoc;
	/* VR Headset Rotation */
	UPROPERTY()
	FRotator HeadRot;
	/* Left Motion Controller Rotation */
	UPROPERTY()
	FRotator LeftHandRot;
	/* Right Motion Controller Rotation */
	UPROPERTY()
	FRotator RightHandRot;

	FVRInputData()
		: HeadLoc(FVector::ZeroVector)
		, LeftHandLoc(FVector::ZeroVector)
		, RightHandLoc(FVector::ZeroVector)
		, HeadRot(FRotator::ZeroRotator)
		, LeftHandRot(FRotator::ZeroRotator)
		, RightHandRot(FRotator::ZeroRotator)
	{}
};

/* Calculated instantaneous body state */
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FIKBodyData
{
	GENERATED_USTRUCT_BODY()

	/* Pelvis Transform (always in world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform Pelvis;
	/* Pelvis Transform (always in world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform PelvisRaw;
	/* Ribcage/Spine Transform in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform Ribcage;
	/* Neck Transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform Neck;
	/* Head Transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform Head;
	/* Right collarbone relative (yaw, pitch) rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FRotator CollarboneRight;
	/* Right Upperarm Transform (only if ComputeHandsIK is on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform UpperarmRight;
	/* Right Forearm Transform (only if ComputeHandsIK is on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform ForearmRight;
	/* Right Palm Transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform HandRight;
	/* Left collarbone relative (yaw, pitch) rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FRotator CollarboneLeft;
	/* Left Forearm Transform (only if ComputeHandsIK is on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform UpperarmLeft;
	/* Left Forearm Transform (only if ComputeHandsIK is on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform ForearmLeft;
	/* Left Palm Transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform HandLeft;
	/* IK Joint Target for right hand in world space (if ComputeHandsIK is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FVector ElbowJointTargetRight;
	/* IK Joint Target for left hand in world space (if ComputeHandsIK is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FVector ElbowJointTargetLeft;
	/* Right Feet IK Transform (don't use this value) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform FootRightTarget;
	/* Left Feet IK Transform (don't use this value) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform FootLeftTarget;
	/* Right Thigh Transform (only if ComputeLegsIK is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform ThighRight;
	/* Right Calf Transform (only if ComputeLegsIK is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform CalfRight;
	/* Left Thigh Transform (only if ComputeLegsIK is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform ThighLeft;
	/* Left Calf Transform (only if ComputeLegsIK is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform CalfLeft;
	/* Right Feet IK instantaneous Transform (only if ComputeFeetIKTargets is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform FootRightCurrent;
	/* Left Feet IK instantaneous Transform (only if ComputeFeetIKTargets is set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody Bone Transform")
	FTransform FootLeftCurrent;
	/* Flag indicates if character is jumping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	bool IsJumping;
	/* is player crawling or laying down? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	EBodyOrientation BodyOrientation;
	/* Flag indicates if character is sitting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	bool IsSitting;
	/* Current Player Vector Velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	FVector Velocity;
	/* Current Ground Z Coordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	float GroundLevel;
	/* Current ground Z coordinate for right foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	float GroundLevelRight;
	/* Current ground Z coordinate for left foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	float GroundLevelLeft;
	/* Twist value between elbow and wrist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	float LowerarmTwistRight;
	/* Twist value between elbow and wrist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IKBody State")
	float LowerarmTwistLeft;

	FIKBodyData()
		: CollarboneRight(FRotator::ZeroRotator)
		, CollarboneLeft(FRotator::ZeroRotator)
		, ElbowJointTargetRight(FVector::ZeroVector)
		, ElbowJointTargetLeft(FVector::ZeroVector)
		, IsJumping(false)
		, BodyOrientation(EBodyOrientation::Stand)
		, IsSitting(false)
		, Velocity(FVector::ZeroVector)
		, GroundLevel(0.f)
		, GroundLevelRight(0.f)
		, GroundLevelLeft(0.f)
		, LowerarmTwistRight(0.f)
		, LowerarmTwistLeft(0.f)
	{}
};

/*******************************************************************************************************/

/* This component uses input received from VR head set and controllers to calculate approximate player's body state */
UCLASS(
	Blueprintable,
	ClassGroup=(IKBody),
	meta=(BlueprintSpawnableComponent)
)
class VRIKBODYRUNTIME_API UVRIKBody : public UActorComponent
{
	GENERATED_BODY()

public:	
	UVRIKBody();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray <FLifetimeProperty> &OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* DEPRECATED. Set this flag to True if you need Feet Transform Predictions */
	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	bool ComputeFeetIKTargets;
	/* DEPRECATED. Set this flag to True if you need thigh and calf transforms. Only works if ComputeFeetIKTargets is true. */
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	bool ComputeLegsIK;
	/* DEPRECATED. Set this flag to True if you need upperarm and forearm transforms */
	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	bool ComputeHandsIK;

	UPROPERTY(EditAnywhere, Category = "Setup")
	bool bUpdateAutomaticallyInTick;

	/* If true, Owning Pawn location Z will be used as floor coordinate; doesn't work properly on slopes and staircases */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool UseActorLocationAsFloor;
	/* (only if UseActorLocationAsFloor is false) If true, component is tracing groun by Object Type, if false - by channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool TraceFloorByObjectType;
	/* (only if UseActorLocationAsFloor is false) Collision channel (Object Type) of floor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	TEnumAsByte<ECollisionChannel> FloorCollisionObjectType;
	/* If VR Input Option is equal to 'Input from Components', call Initialize(...) function to setup Camera and Hand components.
	Don't use 'Input from Variables' if component is replicated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="VR Input Option"), Category = "Setup")
	EVRInputSetup VRInputOption;
	/* If true, collarbones don't rotate to follow hands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool LockShouldersRotation;
	/* Rotation of collarbones up/down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float CollarboneVerticalRotation;
	/* Rotation of collarbones : sides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float CollarboneHorizontalRotation;
	/* This flag only works if VRInputOption is DirectVRInput. If true, component uses Pawn Actor Transform to locate body
	in world space. Otherwise it retuns body relative to Pawn Origin. World space calculation is required for unnatural locomotion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool FollowPawnTransform;
	/* If true, VRIKBody will try to detect continuous head rotations. Otherwise hip is aligned to Head Yaw by timeout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool DetectContinuousHeadYawRotation;
	/* Enable twisting to decrease errors in torso orientation by rotiting ribcage. False value locks ribcage and pelvis rotations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool EnableTorsoTwist;
	/* How strongly should torso follow head rotation? From 0 to 1. 1 is highest level, see also LockTorsoToHead (below). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float TorsoRotationSensitivity;
	/* Check for hard attachment of torso rotation to head rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bLockTorsoToHead;
	/* Always rotate torso with head when actor is sitting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bFollowHeadRotationWhenSitting;

	/* Left palm location relative to Motion Controller Component transform. Default value works fine if you hold HTC Vive controller
	as a pistol. For other cases reset this transform to zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	FTransform LeftPalmOffset;
	/* Right palm location relative to Motion Controller Component transform. Default value works fine if you hold HTC Vive controller
	as a pistol. For other cases reset this transform to zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	FTransform RightPalmOffset;	
	/* Tick Interval of the timer, calculating correlation betwen hands and head */
	UPROPERTY(EditAnywhere, Category = "Setup")
	float TimerInterval;
	/** Timer interval to reset torso rotation to head */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float TorsoResetToHeadInterval;
	/** Force component to blend player's spine when sitting/bending */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bDoManualBodyBending;
	/** Should detect and process body poses (standing/sitting/laying)? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	bool bDetectBodyOrientation;
	/** New algorithm is based on wirst orientation relative to body rather then reist location.
	It's more correct, but wan't properly tested yet and can cause artifacts. */
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	bool bUseNewElbowPredictionAlgorithm;
	/** Maximum allowed distance from head to controller before returning hand in default pose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float MaxDistanceFromHeadToController;
	UPROPERTY()
	float MaxDistanceFromHeadToControllerSquared;
	/** Orient torso to hand rather then head when player is sitting or bent down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	bool bOrientLoweredPlayerToHands;

	/* Relative Y-axis body size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float BodyWidth;
	/* Relative X-axis body size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float BodyThickness;
	/* Approximate head radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float HeadHalfWidth;
	/* Approximate head height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float HeadHeight;
	/* Approximate distance from pelvis to neck */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float SpineLength;
	/* Approximate distance from collarbone to palm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float HandLength;
	/* Distance from feet to ground, useful to correct skeletal mesh feet bones Z-offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float FootOffsetToGround;

	/* Default value, replaced by calibration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	FVector NeckToHeadsetOffset;
	/* Used in calibration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	FVector RibcageToNeckOffset;
	/* Max possible angle between head and ribcage Yaw rotations (used to correct pelvis rotation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float MaxHeadRotation;
	/* Angle between head and ribcage Yaw rotations to  trigger ribcage rotation (torso twist, EnableTorsoTwist must be enabled); 0 to disable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float SoftHeadRotationLimit;
	/* How strongly should elbow go down when wrist is close to shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float ElbowSinkFactor;
	/* How strongly should elbow go down when wrist is far from shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float ElbowFarSinkFactor;
	/* Value added to headset height when calculating player's height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float CalibrationArmSpanAdjustment;
	/* Value added to distance between controllers when calculating arm span */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float CalibrationHeightAdjustment;
	/* Offset from center of coordinates of motion controller to wrist location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	FVector WristCalibrationOffset;
	/* Offset from regular camera height to detect player jumping in physical space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params")
	float PhysicalJumpingOffset;
	/* Should adjust NeckToHeadsetOffset value at calibration? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Params", meta=(DisplayName="Calibrate Ribcage at T-Pose"))
	bool bCalibrateRibcageAtTPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugOutput;

	/* Only available if VR Input Option is 'Input from components' or components are initialized by Initialize(...)
	function. This flag orders component to move remote Motion Controllers and Head component with a body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Networking")
	bool ReplicateComponentsMovement;

	/* If ReplicateInWorldSpace is set to false (by default), all body data would be converted to actor space
	before replication and reconverted to world space on remote machines. This operation adds a lot of transforms
	calculations, but allow to use sliding (Onward-style) locomotion which implies that pawn locations
	on different PCs are slightly asynchronous at the same moment. Set ReplicateInWorldSpace to true if
	you don't use sliding player locomotion to optimize CPU usage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Networking")
	bool ReplicateInWorldSpace;

	/* Interpolation speed for bones transforms received via replication. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Networking")
	float SmoothingInterpolationSpeed;

	/* Useful for blending/interpolation. 1 when player is standing, 0 when player is sitting/lowered */
	UPROPERTY(BlueprintReadOnly, Category = "IKBody")
	float BodyLoweringAlpha;

	/* Body Calibration Complete successfully */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodySimple OnCalibrationComplete;

	// TODO: add player movements analysis
	/* Event called when player starts a jump, NOT IN USE */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodySimple OnJumpStarted;
	/* Event called when player ends a jump, NOT IN USE */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodySimple OnGrounded;
	/* Event called when player finishing squatting, NOT IN USE */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodySimple OnSitDown;
	/* Event called when player stands up, NOT IN USE */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodySimple OnStandUp;
	/* Not in use! */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodyRepeatable OnHeadShake;
	/* Not in use! */
	UPROPERTY(BlueprintAssignable, Category = "IKBody")
	FIKBodyRepeatable OnHeadNod;

	/* Calibrate Body Params at T-Pose (hand to the left and right) */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool CalibrateBodyAtTPose();
	/* Calibrate Body Params at I-Pose (hand down) */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool CalibrateBodyAtIPose();
	/* Calibrate Body Params at T or I pose (detected automatically) */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool AutoCalibrateBodyAtPose(FString& StatusMessage);

	/* This function activates recording of HMD and motion controllers locations and orientation. If use replication or Input from Components, use Initialize(...) instead.
	@param RootComp in a Parent Component for VR Camera and controllers, equal to player area centre at the floor.
	@return true if successful. */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool ActivateInput(USceneComponent* RootComp = nullptr);

	/* This function stops recording of HMD and motion controllers locations and orientation */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void DeactivateInput();

	/* Call this function on BeginPlay to setup component references if VRInputOption is equal to 'Input From Components' and in a multiplayer games.
	For networking use only Motion Controller components as hand controllers or make sure this components are properly initialized
	(tracking must be enabled on local PCs and disabled on remotes) manually. */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void Initialize(USceneComponent* Camera, USceneComponent* RightController, USceneComponent* LeftController);

	/* Main function to calculate Body State.
	@param DeltaTime - world tick delta time
	@return a struct describing current body state */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void ComputeFrame(float DeltaTime, FIKBodyData& ReturnValue);

	/* Returns a last body state struct calculated by ComputeFrame(...) function */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	FIKBodyData GetLastFrameData();

	/* Returns a last body state struct calculated by ComputeFrame(...) function */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	void GetRefLastFrameData(FIKBodyData& LastFrameData);

	/* Get world transform of root component for a moment CapturedBody was calculated */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	FTransform GetLastFrameBaseTransform() const;

	/* Legacy function! Only works for default UE4 Skeleton.
	Converts calculated body parts orientation to skeleton bones
	orientation. It's useful to directly set bone transforms in Anim Blueprint
	using 'Modify (Transform) Bone' node. Keep in mind that transforms in the
	returned struct are still in world space. */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	FIKBodyData ConvertDataToSkeletonFriendly(const FIKBodyData& WorldSpaceIKBody);

	/* Call this function to set Pitch and Roll of Pelvis to zero and Yaw equal to Head Yaw */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void ResetTorso();

	/* Returns calculated or calibrated character height */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	float GetCharacterHeight() const { return CharacterHeight; };

	/* Returns calculated or calibrated legs length */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	float GetCharacterLegsLength() const { return LegsLength; };

	/* Detaches Palm from Motion Controller and attaches to primitive component (for example, two-handed rifle) */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool AttachHandToComponent(EControllerHand Hand, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform);

	/* Reattach hand palm to Motion Controller Component */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void DetachHandFromComponent(EControllerHand Hand);

	/* Head/hands Input if VRInputOption is 'Input from Variables' */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void UpdateInputTransforms(const FTransform& HeadTransform, const FTransform& RightHandTransform, const FTransform& LeftHandTransform) { InputHMD = HeadTransform; InputHandRight = RightHandTransform; InputHandLeft = LeftHandTransform; };

	/* @return The object describing body calibration results. Use it to save and restore body params if you need to respawn player. */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	FCalibratedBody GetCalibratedBody() const;

	/* Load body calibration params */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void RestoreCalibratedBody(const FCalibratedBody& BodyParams);

	/* Mark body as non-calibrated. Function (replicated) keeps existing body params, but allow to recalibrate body if necessary. */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void ResetCalibrationStatus();

	/* Check body calibration params */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool IsValidCalibrationData(const FCalibratedBody& BodyParams) const { return (BodyParams.fBodyWidth != 0.f && BodyParams.CharacterHeight != 0.f && BodyParams.CharacterHeightClear != 0.f && BodyParams.ArmSpanClear != 0.f); };

	/* Clear vertical distance from ground to camera without any adjustments */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	float GetClearCharacterHeight() const { return CharacterHeightClear; };
	
	/* Clear horizontal distance between palms without any adjustments */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	float GetClearArmSpan() const { return ArmSpanClear; };

	/* Return true if calibration was complete or calibration data is loaded by RestoreCalibratedBody */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	bool IsBodyCalibrated() const { return (bCalibratedT && bCalibratedI); };

	/* Return already calibrated poses */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	void GetCalibrationProgress(bool& TPose, bool& IPose) const { TPose = bCalibratedT; IPose = bCalibratedI; };

	/* @return Joint Targets for elbows to use in TwoBodyIK animation node (in world space) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	void GetElbowJointTarget(FVector& RightElbowTarget, FVector& LeftElbowTarget) {
		RightElbowTarget = SkeletonTransformData.ElbowJointTargetRight;
		LeftElbowTarget = SkeletonTransformData.ElbowJointTargetLeft;
	};

	/* @return Joint Targets for knees to use in TwoBodyIK animation node (in world space) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	void GetKneeJointTarget(FVector& RightKneeTarget, FVector& LeftKneeTarget);

	/* @return true if component was initialized by ActivateInput(...) or Initialize(...) function */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	bool IsInitialized() { return bIsInitialized; };

	/* Manually set body orientation (for example, lying at the ground) */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void SetManualBodyOrientation(EBodyOrientation NewOrientation) { bManualBodyOrientation = true; SkeletonTransformData.BodyOrientation = NewOrientation; };

	/* Restore automatic body calibration */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	void UseAutoBodyOrientation() { bManualBodyOrientation = false; };

	/* Is body in air in game (i. e. player can stand on the ground in real room space)? */
	UFUNCTION(BlueprintCallable, Category = "IKBody")
	bool IsInAir() const { return bIsInAir; };

	FVector GetCharacterVelocity() const { return SkeletonTransformData.Velocity; };

	/* Returns delta rotation between FloorComp rotation and zero rotator */
	void GetFloorRotationAdjustment(FTransform& SimulatedBaseTransform, FTransform& RealBaseTransform);

	bool TraceFloorInPoint(const FVector& TraceLocation, FVector& OutHitPoint);

	/* Get used VR origin component */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IKBody")
	USceneComponent* GetRefComponent() const { return FloorBaseComp; };

	/**
	* Send calibration data to remote instances.
	* This function should be called on server for newly logged players who calibrated before connecting to server.
	* Use OnPostLogin event in game mode class. I recommend to add a delay about .5 seconds.
	*/
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Capture")
	void SendCalibrationToServer();
	void SendCalibrationToServer_Implementation();

	/** Send calibration data to clients */
	UFUNCTION(BlueprintCallable, Category = "Capture")
	void ServerRefreshCalibration();

protected:
	/* Load body calibration params (replicated version) */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRestoreCalibratedBody(const FCalibratedBody& BodyParams);
	bool ServerRestoreCalibratedBody_Validate(const FCalibratedBody& BodyParams) { return true; };
	void ServerRestoreCalibratedBody_Implementation(const FCalibratedBody& BodyParams) { DoRestoreCalibratedBody(BodyParams); ClientRestoreCalibratedBody(BodyParams); };

	/* Restore body calibration params on clients (replicated version) */
	UFUNCTION(NetMulticast, Reliable)
	void ClientRestoreCalibratedBody(const FCalibratedBody& BodyParams);
	void ClientRestoreCalibratedBody_Implementation(const FCalibratedBody& BodyParams) { DoRestoreCalibratedBody(BodyParams); };

	// Restoring body calibration from calbirated body struct variable
	UFUNCTION()
	void DoRestoreCalibratedBody(const FCalibratedBody& BodyParams);

	/* Reset body calibration state (replicated version) */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResetCalibrationStatus();
	bool ServerResetCalibrationStatus_Validate() { return true; };
	void ServerResetCalibrationStatus_Implementation() { DoResetCalibrationStatus(); ClientResetCalibrationStatus(); };

	/* Reset body calibration state on clients (replicated version) */
	UFUNCTION(NetMulticast, Reliable)
	void ClientResetCalibrationStatus();
	void ClientResetCalibrationStatus_Implementation() { DoResetCalibrationStatus(); };

	// Reset body calibration state
	UFUNCTION()
	void DoResetCalibrationStatus();

	// Replication notifies
	UFUNCTION()
	void OnRep_UnpackReplicatedData();

private:
	/* HMD camera component pointer */
	UPROPERTY()
	USceneComponent* CameraComponent;
	/* Motion Controller component pointer for right hand */
	UPROPERTY()
	USceneComponent* RightHandController;
	/* Motion Controller component pointer for left hand */
	UPROPERTY()
	USceneComponent* LeftHandController;
	// Pointer to player's pawn
	UPROPERTY()
	APawn* OwningPawn;
	UPROPERTY()
	USceneComponent* FloorBaseComp;
	UPROPERTY()
	FTransform LastFloorCompTransform;
	UPROPERTY()
	float PelvisToHeadYawOffset;

	// Global State
	UPROPERTY()
	bool bIsInitialized;
	UPROPERTY()
	bool bManualBodyOrientation;

	// Replicated body state and VR Input
	UPROPERTY(Replicated)
	FNetworkTransform NT_InputHMD;
	UPROPERTY(Replicated)
	FNetworkTransform NT_InputHandRight;
	UPROPERTY(Replicated)
	FNetworkTransform NT_InputHandLeft;
	UPROPERTY(Replicated)
	float NT_PelvisToHeadYawOffset;
	UPROPERTY()
	FVector PelvisLocationLastFrame;
	UPROPERTY(Replicated)
	FVector_NetQuantize100 NT_Velocity;

	// Calculation result
	UPROPERTY()
	FIKBodyData SkeletonTransformData;
	UPROPERTY()
	FIKBodyData SkeletonTransformDataRelative;

	// Current Input for manual input from variables
	UPROPERTY()
	FTransform InputHMD;
	UPROPERTY()
	FTransform InputHandRight;
	UPROPERTY()
	FTransform InputHandLeft;

	// Target (not current) replicated transforms. Current body state is interpolated to target.
	UPROPERTY()
	FIKBodyData SkeletonTransformData_Target;
	UPROPERTY()
	FTransform InputHMD_Target;
	UPROPERTY()
	FTransform InputHandRight_Target;
	UPROPERTY()
	FTransform InputHandLeft_Target;

	// Previous headset transform - local
	UPROPERTY()
	FVector vPrevCamLocation;
	UPROPERTY()
	FRotator rPrevCamRotator;

	// Hands Attachment
	// In some interactions hands need to be attached to scene components instead of motion controllers
	UPROPERTY()
	bool HandAttachedRight;
	UPROPERTY()
	bool HandAttachedLeft;
	UPROPERTY()
	UPrimitiveComponent* HandParentRight;
	UPROPERTY()
	UPrimitiveComponent* HandParentLeft;
	UPROPERTY()
	FTransform HandAttachTransformRight;
	UPROPERTY()
	FTransform HandAttachTransformLeft;
	UPROPERTY()
	FName HandAttachSocketRight;
	UPROPERTY()
	FName HandAttachSocketLeft;

	// Timers
	UPROPERTY()
	FTimerHandle hVRInputTimer;
	UPROPERTY()
	FTimerHandle hResetFootLTimer;
	UPROPERTY()
	FTimerHandle hResetFootRTimer;
	UPROPERTY()
	FTimerHandle hTorsoYawTimer;
	UPROPERTY()
	FTimerHandle hResetTorsoTimer;

	// Feet animation
	UPROPERTY()
	FTransform FootTargetTransformL;
	UPROPERTY()
	FTransform FootTargetTransformR;
	UPROPERTY()
	FTransform FootTickTransformL;
	UPROPERTY()
	FTransform FootTickTransformR;
	UPROPERTY()
	FTransform FootLastTransformL;
	UPROPERTY()
	FTransform FootLastTransformR;
	UPROPERTY()
	float FeetCyclePhase;
	UPROPERTY()
	float FeetMovingStartedTime;
	UPROPERTY()
	float fDeltaTimeL;
	UPROPERTY()
	float fDeltaTimeR;
	UPROPERTY()
	bool bResetFeet;

	// Reset ribcage yaw rotation after timeout
	UPROPERTY()
	uint8 nYawControlCounter;
	UPROPERTY()
	bool bYawInterpToHead;
	UPROPERTY()
	bool bResetTorso;
	UPROPERTY()
	FVector SavedTorsoDirection;

	// Initial auto calibration setup
	UPROPERTY()
	bool ResetFootLocationL;
	UPROPERTY()
	bool ResetFootLocationR;
	UPROPERTY()
	uint8 nModifyHeightState; // 0 - waiting; 1 - modify on; 2 - don't modify
	UPROPERTY()
	float ModifyHeightStartTime;
	
	// Internal current body state
	UPROPERTY()
	bool bTorsoYawRotation;
	UPROPERTY()
	bool bTorsoPitchRotation;
	UPROPERTY()
	bool bTorsoRollRotation;
	UPROPERTY()
	bool bTorsoYawIsSeparated;
	UPROPERTY()
	bool bIsSitting;
	UPROPERTY()
	bool bIsStanding;
	UPROPERTY()
	bool bIsWalking;
	UPROPERTY()
	bool bIsInAir;
	UPROPERTY()
	float JumpingOffset;
	UPROPERTY()
	float CharacterHeight;
	UPROPERTY()
	float CharacterHeightClear;
	UPROPERTY()
	float ArmSpanClear;
	UPROPERTY()
	float LegsLength;
	UPROPERTY()
	uint8 nIsRotating;
	UPROPERTY()
	float StartRotationYaw;
	UPROPERTY()
	float UpperarmLength;
	UPROPERTY()
	float ThighLength;

	// Calibration Setup
	UPROPERTY()
	FTransform CalT_Head;
	UPROPERTY()
	FTransform CalT_ControllerL;
	UPROPERTY()
	FTransform CalT_ControllerR;
	UPROPERTY()
	FTransform CalI_Head;
	UPROPERTY()
	FTransform CalI_ControllerL;
	UPROPERTY()
	FTransform CalI_ControllerR;
	UPROPERTY()
	bool bCalibratedT;
	UPROPERTY()
	bool bCalibratedI;
	UPROPERTY()
	float TracedFloorLevel;
	UPROPERTY()
	float TracedFloorLevelR;
	UPROPERTY()
	float TracedFloorLevelL;

	// Variables to calculate body state
	UPROPERTY()
	float HeadPitchBeforeSitting;
	UPROPERTY()
	FVector HeadLocationBeforeSitting;
	UPROPERTY()
	float RecalcCharacteHeightTime;
	UPROPERTY()
	float RecalcCharacteHeightTimeJumping;
	UPROPERTY()
	float ShoulderYawOffsetRight;
	UPROPERTY()
	float ShoulderYawOffsetLeft;
	UPROPERTY()
	float ShoulderPitchOffsetRight;
	UPROPERTY()
	float ShoulderPitchOffsetLeft;

	UPROPERTY()
	FRotator PrevFrameActorRot;

	// Saved VR Input Data
	UPROPERTY()
	uint8 CurrentVRInputIndex;

	FVRInputData VRInputData[CORR_SAVE_POINTS_NUM + 4];

	UPROPERTY()
	float PreviousElbowRollRight;
	UPROPERTY()
	float PreviousElbowRollLeft;

	// Data Correltaion arrays
	float cor_rot_a[3][CORR_POINTS_NUM];
	float cor_rot_b[3][CORR_POINTS_NUM];
	float cor_rot_c[3][CORR_POINTS_NUM];
	float cor_loc_a[3][CORR_POINTS_NUM];
	float cor_loc_b[3][CORR_POINTS_NUM];
	float cor_loc_c[3][CORR_POINTS_NUM];
	float cor_rotv_a[3][CORR_POINTS_NUM];
	float cor_rotv_b[3][CORR_POINTS_NUM];
	float cor_rotv_c[3][CORR_POINTS_NUM];
	float cor_linear[3][CORR_POINTS_NUM];

	float EmulateOffset;

	//////////////////////////////////////////////////////////////////////////////

	UFUNCTION()
	void VRInputTimer_Tick();

	UFUNCTION()
	void RibcageYawTimer_Tick();

	UFUNCTION()
	void ResetFootsTimerL_Tick();

	UFUNCTION()
	void ResetFootsTimerR_Tick();

	UFUNCTION()
	void ResetTorsoTimer_Tick(); //hResetTorsoTimer

	UFUNCTION()
	void GetCorrelationKoef(int32 StartIndex = -1);

	UFUNCTION()
	void CalcShouldersWithOffset(EControllerHand Hand);

	UFUNCTION()
	void CalcHandIKTransforms(EControllerHand Hand);

	UFUNCTION()
	void CalcFeetInstantaneousTransforms(float DeltaTime, float FloorZ);

	UFUNCTION()
	void CalcFeetIKTransforms2(float DeltaTime, float FloorZ);

	inline float GetAngleToInterp(const float Current, const float Target);
	inline float GetFloorCoord();
	inline FTransform GetHMDTransform(bool bRelative = false);
	inline FTransform GetHandTransform(EControllerHand Hand, bool UseDefaults = true, bool PureTransform = true, bool bRelative = false);
	inline bool ConvertBodyToRelative();
	inline bool RestoreBodyFromRelative();
	inline float DistanceToLine(FVector LineA, FVector LineB, FVector Point);
	inline bool IsHandInFront(const FVector& Forward, const FVector& Ribcage, const FVector& Hand);
	FORCEINLINE EBodyOrientation ComputeCurrentBodyOrientation(const FTransform& Head, const FTransform& HandRight, const FTransform& HandLeft);
	FORCEINLINE FVector GetForwardDirection(const FVector ForwardVector, const FVector UpVector);
	FORCEINLINE float GetForwardYaw(const FVector ForwardVector, const FVector UpVector);
	FORCEINLINE bool GetKneeJointTargetForBase(const FTransform& BaseTransform, const float GroundZ, FVector& RightJointTarget, FVector& LeftJointTarget);
	FORCEINLINE FVector CalculateElbowNew(const FTransform& Ribcage, const FTransform& Shoulder, const FTransform& Wrist);

	// perform calibration
	UFUNCTION()
	void CalibrateSkeleton();

	// single line trace to floor
	UFUNCTION()
	void TraceFloor(const FVector& HeadLocation);

	// simple two-bone IK
	UFUNCTION()
	void CalculateTwoBoneIK(const FVector& ChainStart, const FVector& ChainEnd, const FVector& JointTarget, const float UpperBoneSize, const float LowerBoneSize, FTransform& UpperBone, FTransform& LowerBone, float BendSide = 1.f);

	// helper function to calculate elbow IK targets
	UFUNCTION()
	void ClampElbowRotation(const FVector InternalSide, const FVector UpSide, const FVector HandUpVector, FVector& CurrentAngle);

	// send VR input to server
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerUpdateInputs(const FNetworkTransform& Head, const FNetworkTransform& HandRight, const FNetworkTransform& HandLeft, float PelvisHeadYaw, FVector_NetQuantize100 Vel);
	bool ServerUpdateInputs_Validate(const FNetworkTransform& Head, const FNetworkTransform& HandRight, const FNetworkTransform& HandLeft, float PelvisHeadYaw, FVector_NetQuantize100 Vel) { return true; };
	void ServerUpdateInputs_Implementation(const FNetworkTransform& Head, const FNetworkTransform& HandRight, const FNetworkTransform& HandLeft, float PelvisHeadYaw, FVector_NetQuantize100 Vel)
	{
		NT_InputHMD = Head;
		NT_InputHandLeft = HandLeft;
		NT_InputHandRight = HandRight;
		NT_PelvisToHeadYawOffset = PelvisHeadYaw;
		NT_Velocity = Vel;
		OnRep_UnpackReplicatedData();
	};

	// pack calc/input to networking structs
	UFUNCTION()
	void PackDataForReplication(const FTransform& Head, const FTransform& HandRight, const FTransform& HandLeft);
};