// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Animation/PoseSnapshot.h"
#include "VRIKBody.h"
#include "YnnkVRIKTypes.h"
#include "VRIKFingersTypes.h"
#include "ReferenceSkeleton.h"
#include "SimpleTorchController.h"
#include "YnnkVrAvatar.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogYnnk, Log, All);

class UVRIKFingersFKIKSolver;

/* Ynnk Avatar: Body Source */
UENUM(BlueprintType, Blueprintable)
enum class EYnnkBodyModelSource : uint8
{
	YBM_YnnkNeuralNet					UMETA(DisplayName = "Ynnk Neural Net (Windows Only)"),
	YBM_Procedural						UMETA(DisplayName = "Procedural")
};

/* Ynnk Avatar: Body Source */
UENUM(BlueprintType, Blueprintable)
enum class EYnnkElbowModelSource : uint8
{
	YEM_Procedural						UMETA(DisplayName = "Procedural"),
	YEM_NeuralNet						UMETA(DisplayName = "Neural (Lightweight)")
};

/* Ynnk Avatar: Body Pose */
UENUM(BlueprintType, Blueprintable)
enum class EYnnkBodyPose : uint8
{
	YBP_Default					UMETA(DisplayName = "Default"),
	YBP_Sit						UMETA(DisplayName = "Sitting")
};

/* Calibration data to replicate it automatically via network */
USTRUCT(BlueprintType)
struct FYnnkBodyCalibrationRep
{
	GENERATED_BODY()

	/* Time of calibration */
	UPROPERTY()
	FDateTime Time;

	/* Position of right hand target relative to GroundRef component */
	UPROPERTY()
	float PlayerHeight;

	/* Position of left hand target relative to GroundRef component */
	UPROPERTY()
	float PlayerArmSpan;
};

#define RUNVALUES_NUM 16
#define SD_NUM 40

class USceneComponent;
class USkeletalMeshComponent;
class UCurveFloat;

/* Equal to FYnnkBodyState, but optimized for replication */
USTRUCT(BlueprintType)
struct VRIKBODYRUNTIME_API FYnnkVRInputsRep
{
	GENERATED_USTRUCT_BODY()

	/* Position of VR headset relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform HMD;

	/* Position of right hand target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform HandRight;

	/* Position of left hand target relative to GroundRef component */
	UPROPERTY()
	FNetworkTransform HandLeft;

	FYnnkVRInputsRep() {}
	FYnnkVRInputsRep(const FNetworkTransform& InHMD, const FNetworkTransform& InRight, const FNetworkTransform& InLeft)
		: HMD(InHMD)
		, HandRight(InRight)
		, HandLeft(InLeft)
	{}
};

/*******************************************************************************************************/

/**
* This component takes data from 3 inputs (head and hands), adds three inputs from nn (pelvis and feet)
* and builds final full-body pose from 6-points IK model
*/
UCLASS(
	Blueprintable,
	ClassGroup=(IKBody),
	meta=(BlueprintSpawnableComponent, DisplayName="Ynnk VR-Avatar")
)
class VRIKBODYRUNTIME_API UYnnkVrAvatarComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UYnnkVrAvatarComponent();
	virtual void InitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray <FLifetimeProperty> &OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Name of component used as head IK target in this actor (usually VR Camera) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName CameraComponentName;

	/** Name of component used as right hand IK target in this actor (usually right motion controller) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName HandRightComponentName;

	/** Offset of right hand bone relative to HandRightComponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	FTransform HandRightOffset;

	/** Name of component used as left hand IK target in this actor (usually left motion controller) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName HandLeftComponentName;

	/** Offset of right hand bone relative to HandLeftComponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	FTransform HandLeftOffset;

	/** Name of skeletal mesh component used as avatar (CharacterMesh0 in ACharacter) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName AvatarSkeletalMeshName;

	/** Name of a component used as a ground and reference (usually, parent component of the camera and controllers) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName GroundComponentName;

	/** Default player height, calibration overwrites this value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Parameters")
	float PlayerHeight;

	/** Default player arm span, calibration overwrites this value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Parameters")
	float PlayerArmSpan;

	/** Max length of step for procedural walking animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Parameters")
	float StepSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Parameters")
	float TorsoVerticalOffset;

	/** Skeleton data: name of the head bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Head"))
	FName HeadBoneName;

	/** Skeleton data: name of the right hand/wrist bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Hand Right"))
	FName HandRightBoneName;

	/** Skeleton data: name of the left hand/wrist bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Hand Left"))
	FName HandLeftBoneName;

	/** Skeleton data: name of the right foot bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Foot Right"))
	FName FootRightBoneName;

	/** Skeleton data: name of the left foot bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Foot Left"))
	FName FootLeftBoneName;

	/** Skeleton data: name of the left foot bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Pelvis (Optional)"))
	FName PelvisBoneName;

	/** Skeleton data: name of the right foot virtual bone, only needed for blended legs animation */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Foot Right Virtual", EditCondition = "bBlendLegsAnimation"))
	FName FootRightVirtualBoneName;

	/** Skeleton data: name of the left foot virtual bone, only needed for blended legs animation */
	UPROPERTY(EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Bone: Foot Left Virtual", EditCondition = "bBlendLegsAnimation"))
	FName FootLeftVirtualBoneName;

	/** Vertical offsets of eyes above the head bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton")
	float CalibrationOffset_Head;

	/** Distance from the center of hand palm to the hand bone */
	UPROPERTY(EditAnywhere, Category = "Skeleton")
	float CalibrationOffset_Hand;

	/** Decrease this value if spine has many bones and bended incorrectly */
	UPROPERTY(BlueprintReadWrite, Category = "IK Settings")
	float SpineBendingMultiplier;

	UPROPERTY(EditAnywhere, Category = "IK Settings")
	EYnnkElbowModelSource ElbowModel;

	/* How strongly should elbow go down when wrist is close to shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	float ElbowSinkFactor;

	/* How strongly should elbow go down when wrist is far from shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	float ElbowFarSinkFactor;

	/** Offset from VR headset to the head bone of the skeletal mesh in HMD space */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	FVector HeadJointOffsetFromHMD;

	/** Ribcage is calculated as a middle point between collarbones */
	UPROPERTY(BlueprintReadWrite, Category = "IK Settings")
	FVector RibageOffsetFromHeadJoint;

	/** Should use GroundComponent as ground vertical coordinate (true) or perform tracing (false)? */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	bool bFlatGround;

	/** Is GroundCollisionChannel an object type of the groun  (true) or tracing channel (false)? */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	bool bTraceGroundByObjectType;

	/** Ground collision trace channel/object type */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	TEnumAsByte<ECollisionChannel> GroundCollisionChannel;

	/** Manual adjustment of feet vertical offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Foot Vertical Offset"), Category = "IK Settings")
	float FeetVerticalOffset;

	/** Orientation adjustment used to blend legs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	float ComponentForwardYaw;

	/** Is legs animation blended in animation blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	bool bBlendLegsAnimation;

	/** Algorithm used to compute IK targets for humanoid body (pelvis, feet) */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	EYnnkBodyModelSource BodyModel;

	/** Controls intensity of smoothing (larger value increases smoothing but adds delay) */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	uint8 NeuralNet_SmoothFrames;

	/** Sets treshold to filter out invalid values (1.0 is 3-sigma) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	float NeuralNet_FilterPower;

	/** Substract player velocity from inputs of neural net */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	bool bNeuralNet_IgnoreOwnerMovement;

	/** Angle which triggers correction of pelvis Yaw rotation */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	float ProceduralPelvisReturnToHeadAngle;

	/** Angle which triggers correction of pelvis Yaw rotation after a small delay */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	float ProceduralPelvisReturnToHeadAngleSoft;

	/** Do not modify */
	UPROPERTY(BlueprintReadWrite, Category = "IK Settings")
	int32 ProceduralTorsoRotationSmoothFrames;

	/** Should drag torso to reach hands position? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Upper Body Priority to Hands"), Category = "IK Settings")
	bool bUpperBodyPriorityHands = true;

	// Don't use it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Settings")
	bool UseSpecialRibcageAlgorithm = false;

	/** Curves to control dependencies of spine: pitch ribcage bone. Second curve to blend from the default. */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	UCurveFloat* CurveRibcagePitch2;
	/** Curves to control dependencies of spine: pitch of pelvis -> ribcage line. Second curve to blend from the default. */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	UCurveFloat* CurveSpinePitch2;
	/** Curves to control dependencies of spine: pitch of pelvis bone. Second curve to blend from the default. */
	UPROPERTY(EditAnywhere, Category = "IK Settings")
	UCurveFloat* CurvePelvisPitch2;

	/** Interpolation speed for remote transforms */
	UPROPERTY(EditAnywhere, Category = "Networking")
	float NetworkInterpolationSpeed;

	/** Update interval to send data to server (0 to update in every tick) */
	UPROPERTY(EditAnywhere, Category = "Networking")
	float ServerUpdateInterval;
	float NextServerUpdateTime = 0.f;
	
	/** Use timer to detect body replication changed and send update to server */
	UPROPERTY(EditAnywhere, Category = "Networking")
	bool bAutoReplicateBodyCalibration;

		/** Replicate fingers animated from controllers. Poses should be replicated manually. */
	UPROPERTY()
	bool bReplicateFingers = false;

	/** Interval to send fingers data to server */
	UPROPERTY()
	float FingersUpdateInterval = 0.1f;
	UPROPERTY()
	float NextFingersUpdateTime = 0.f;

	/** Enable/Disable legs animation */
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk VR-Avatar")
	bool bLegsEnabled = true;

	/** 6 base targets in actor space used to animate a whole body */
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk VR-Avatar")
	FYnnkBodyState IKTargets;

	/** Current skeleton bones positions relative to Ground Component */
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk VR-Avatar")
	TArray<FTransform> BonesActorSpace;

	/** Current skeleton bones positions in local space */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk VR-Avatar")
	FPoseSnapshot CurrentPoseSnapshot;

	/** Elbow target for TwoBoneIK solver relative to selected Ground Component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk VR-Avatar", meta = (DisplayName = "Elbow Target Right"))
	FVector ElbowTargetRight_AS;

	/** Elbow target for TwoBoneIK solver relative to selected Ground Component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk VR-Avatar", meta = (DisplayName = "Elbow Target Left"))
	FVector ElbowTargetLeft_AS;

	/** Knee target for TwoBoneIK solver relative to selected Ground Component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk VR-Avatar", meta = (DisplayName = "Knee Target Right"))
	FVector KneeTargetRight_AS;

	/** Knee target for TwoBoneIK solver relative to selected Ground Component */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk VR-Avatar", meta = (DisplayName = "Knee Target Left"))
	FVector KneeTargetLeft_AS;

	/** Output value to apply TwoBoneIK after blending legs animation: location of right foot */
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	FVector FootRight_Location;

	/** Output value to apply TwoBoneIK after blending legs animation: location of left foot */
	UPROPERTY(BlueprintReadOnly, Category = "Output")
	FVector FootLeft_Location;

	/** Call to initialize internal data and get references to input components and skeletal mesh component */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	bool Initialize();

	/** Call to initialize internal data and get references to input components and skeletal mesh component */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	bool InitializeWithComponents(USceneComponent* HeadSource, USceneComponent* HandRightSource, USceneComponent* HandLeftSource, USceneComponent* GroundSource);

	/** Set pre-saved HandRightOffset and HandLeftOffset transforms for typical types of motion controllers */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetDefaultHandOffsets(EMotionControllerModel ControllerModel);

	/** Update component in client PC */
	UFUNCTION(Client, Reliable)
	void ClientSetComponent(uint8 Component, USceneComponent* NewTarget);
	void ClientSetComponent_Implementation(uint8 Component, USceneComponent* NewTarget);

	/** Manually update input component for head */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetHeadComponent(USceneComponent* NewHeadTarget);

	/** Manually update input component for the right hand */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetHandRightComponent(USceneComponent* NewHandTarget);

	/** Manually update input component for the left hand */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetHandLeftComponent(USceneComponent* NewHandTarget);

	/**
	* Manually recalibrate body for desired height and arm span
	* ~ ~ ~
	* This function should be called at Locally Controlled pawn!
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetBodySizeManually(float Height, float ArmSpan);

	/** Debug function to check skeleton setup */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void DrawGenericSkeletonCoordinates();

	/** Set blend alpha (0..1) between default spine-control curves and curves defined in 'IK Settings' group */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetSpineCurvesBlendAlpha(float NewAlpha);

	/**
	* Collect current coordinates of head and hands,
	* Then get pelvis and feet from NN
	*/
	void UpdateIKTargets(float DeltaTime);

	/** Check body calibration and replicate if needed */
	void UpdateBodyCalibration();

	float NextCalibrationCheckTime = 0.f;


	/** Is component ready to animate avatar? */
	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	bool IsInitialized() const { return bInitialized; }

	/** Enable or disable avatar update */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetEnabled(bool bNewIsEnabled);

	/** Update player's height and arm span. Call this function when player is in T pose */
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar", meta=(DisplayName="Calibrate in T-Pose"))
	bool CalibrateInTPose(FString& Errors);

	/** Is capturing? */
	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	bool IsEnabled() const { return IsComponentTickEnabled(); }

	/** Get current walk yaw (from controllers) relative to forward direction (useful to blend legs animation) */
	UFUNCTION(BlueprintPure, Category = "Sensorium VR Avatar")
	float GetLowerBodyRotation(bool bWorldSpace) const;

	/** Detaches hand from Motion Controller and attaches to primitive component (for example, two-handed rifle)
	* @param bHandRight True for right hand, false for left hand
	* @param Component Attachment target for the hand
	* @param SocketName Socket to attach at the Component (can be None)
	* @param RelativeTransform Attachment offset relative to socket or pivot of the component
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	bool AttachHandToComponent(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform);
	bool AttachHandToComponentLocal(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform);

	/* Called on server? */
	UFUNCTION(NetMulticast, Reliable)
	void ClientAttachHandToComponent(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform);
	void ClientAttachHandToComponent_Implementation(bool bHandRight, UPrimitiveComponent* Component, const FName& SocketName, const FTransform& RelativeTransform);

	/** Reattach hand palm to Motion Controller Component
	* @param bHandRight True for right hand, false for left hand
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void DetachHandFromComponent(bool bHandRight);
	void DetachHandFromComponentLocal(bool bHandRight);

	/* Called on server? */
	UFUNCTION(NetMulticast, Reliable)
	void ClientDetachHandFromComponent(bool bHandRight);
	void ClientDetachHandFromComponent_Implementation(bool bHandRight);

	/** Put player in virtual chair */
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void SetChairTransform(const FTransform& ChairInWorldSpace);

	/** Take player from the virtual chair */
	UFUNCTION(BlueprintCallable, Category = "VR IK Skeletal Mesh Translator")
	void ResetChairTransform();

	/** Get current walk speed (from controllers) */
	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	float GetWalkSpeed(bool bMeanValue) const { return bMeanValue ? WalkingSpeedMean : WalkingSpeed; }

	/** Get current walk yaw (from controllers) relative to forward direction (useful to blend legs animation) */
	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	float GetWalkYaw() const { return WalkingYaw; }	

	/** Is walking using controller? */
	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	bool IsGroundComponentMoving() const;

	UFUNCTION(BlueprintCallable, Category = "Ynnk VR-Avatar")
	void SetBodyPose(EYnnkBodyPose NewPose)
	{
		InternalBodyPose = NewPose;
	}

	UFUNCTION(BlueprintPure, Category = "Ynnk VR-Avatar")
	EYnnkBodyPose GetBodyPose() const
	{
		return InternalBodyPose;
	}

	/**
	* Calculate rotation in horizontal plane for root bone
	* Use Pelvis forward rotation when it's projection to horizontal plane is large enougth
	* or interpolated value between forward and up vectors in other casese
	*/
	UFUNCTION(BlueprintPure, Category = "Debug")
	FRotator CalculateRootRotation(const FVector& ForwardVector, const FVector& UpVector) const;

	/** Get current scale applied to skeletal mesh */
	UFUNCTION(BlueprintPure, Category = "Debug")
	void GetMeshScale(float& X, float& Z) const { X = MeshScale.X; Z = MeshScale.Z; }

	/** Get default size of skeletal mesh computed in BeginPlay */
	UFUNCTION(BlueprintPure, Category = "Debug")
	void GetMeshSize(float& X, float& Z) const { X = MeshSize.X; Z = MeshSize.Z; }

	/** Initialize fingers replication */
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void InitializeFingersReplication(float UpdateInterval, UVRIKFingersFKIKSolver* RightSolver, UVRIKFingersFKIKSolver* LeftSolver);

	/** Call on server to update calibration */
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void RefreshCalibrationForAllConnectedPlayers();

	/** Call this function on server to refresh body calibration for all cnnected clients */
	UFUNCTION(NetMulticast, Reliable)
	void ClientsRefreshCalibrationForAll();
	void ClientsRefreshCalibrationForAll_Implementation();

	/**
	* Manually recalibrate body for desired height and arm span
	* ~ ~ ~
	* This function should be called on server!
	*/
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Ynnk VR-Avatar")
	void ServerUpdateCalibration(float Height, float ArmSpan);
	bool ServerUpdateCalibration_Validate(float Height, float ArmSpan) { return true; };
	void ServerUpdateCalibration_Implementation(float Height, float ArmSpan)
	{
		SetBodySizeManually(Height, ArmSpan);
		ClientsUpdateCalibration(Height, ArmSpan);
	}
	
	/**
	* Automatic calibration: replicate data to server from locally controlled instance
	*/
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAutoUpdateCalibration(FDateTime Time, float Height, float ArmSpan);
	bool ServerAutoUpdateCalibration_Validate(FDateTime Time, float Height, float ArmSpan) { return true; };
	void ServerAutoUpdateCalibration_Implementation(FDateTime Time, float Height, float ArmSpan)
	{
		BodyCalibrationRep.Time = Time;
		BodyCalibrationRep.PlayerHeight = Height;
		BodyCalibrationRep.PlayerArmSpan = ArmSpan;
	}

	/** Internal function to update calibration on remote clients */
	UFUNCTION(NetMulticast, Reliable)
	void ClientsUpdateCalibration(float Height, float ArmSpan);
	void ClientsUpdateCalibration_Implementation(float Height, float ArmSpan);

protected:

	// Are skeleton/skeletal mesh and references valid?
	UPROPERTY()
	bool bInitialized;

	/** Curves to control procedual animation: height of foot in a step cycle */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveStepHeight;
	/** Curves to control procedual animation: local pitch of foot in a step cycle */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveStepPitch;
	/** Curves to control procedual animation: local pitch of foot in a step cycle */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveStepOffset;
	/** Curves to control dependencies of spine: pitch ribcage bone */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveRibcagePitch;
	/** Curves to control dependencies of spine: roll of ribcage bone */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveRibcageRoll;
	/** Curves to control dependencies of spine: pitch of pelvis -> ribcage line */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurveSpinePitch;
	/** Curves to control dependencies of spine: pitch of pelvis bone */
	UPROPERTY(BlueprintReadOnly, Category = "IK Settings")
	UCurveFloat* CurvePelvisPitch;

	UPROPERTY()
	float SpineCurvesAlpha = 0.f;

	// Output component
	UPROPERTY()
	USkeletalMeshComponent* BodyMesh;
	// Input components: head and hands
	UPROPERTY()
	USceneComponent* HeadComponent;
	UPROPERTY()
	USceneComponent* HandRightComponent;
	UPROPERTY()
	USceneComponent* HandLeftComponent;
	// Transformations of head and hands are calculated relative to this base:
	UPROPERTY()
	USceneComponent* GroundRefComponent;
	UPROPERTY()
	UVRIKFingersFKIKSolver* RightHandSolver;
	UPROPERTY()
	UVRIKFingersFKIKSolver* LeftHandSolver;

	UPROPERTY()
	class UKerasElbow* KerasElbowModel;

	UPROPERTY()
	float StandingPercent = 0.f;

	FRotator ShoulderRightLocal;
	FRotator ShoulderLeftLocal;

	UPROPERTY()
	EYnnkBodyPose InternalBodyPose;

	// Replication
	UPROPERTY(Replicated)
	FYnnkBodyStateRep IKTargetsRep;
	UPROPERTY(Replicated)
	FYnnkVRInputsRep IKInputsRep;
	UPROPERTY(Replicated)
	FYnnkBodyCalibrationRep BodyCalibrationRep;
	UPROPERTY(Replicated)
	TArray<uint8> FingersRepRight;
	UPROPERTY(Replicated)
	TArray<uint8> FingersRepLeft;

	FDateTime LastCalibrationUpdateTime;

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerSendIK(const FYnnkBodyStateRep& Targets);
	bool ServerSendIK_Validate(const FYnnkBodyStateRep& Targets) { return true; };
	void ServerSendIK_Implementation(const FYnnkBodyStateRep& Targets) { IKTargetsRep = Targets; };

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerSendIKInputs(const FYnnkVRInputsRep& InputTransforms);
	bool ServerSendIKInputs_Validate(const FYnnkVRInputsRep& InputTransforms) { return true; };
	void ServerSendIKInputs_Implementation(const FYnnkVRInputsRep& InputTransforms) { IKInputsRep = InputTransforms; }

		// Send fingers data to server from locally controlled pawn
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerUpdateFingersData(const TArray<uint8>& DataR, const TArray<uint8>& DataL);
	bool ServerUpdateFingersData_Validate(const TArray<uint8>& DataR, const TArray<uint8>& DataL) { return true; };
	void ServerUpdateFingersData_Implementation(const TArray<uint8>& DataR, const TArray<uint8>& DataL) { FingersRepRight = DataR; FingersRepLeft = DataL; };

	// Skeleton Bone Indices
	UPROPERTY()
	int32 HeadIndex;
	UPROPERTY()
	int32 PelvisIndex;
	UPROPERTY()
	int32 HandRightIndex;
	UPROPERTY()
	int32 HandLeftIndex;
	UPROPERTY()
	int32 RibcageIndex;
	UPROPERTY()
	int32 ClavicleRightIndex;
	UPROPERTY()
	int32 UpperarmRightIndex;
	UPROPERTY()
	int32 LowerarmRightIndex;
	UPROPERTY()
	int32 ClavicleLeftIndex;
	UPROPERTY()
	int32 UpperarmLeftIndex;
	UPROPERTY()
	int32 LowerarmLeftIndex;
	UPROPERTY()
	int32 ThighRightIndex;
	UPROPERTY()
	int32 CalfRightIndex;
	UPROPERTY()
	int32 ThighLeftIndex;
	UPROPERTY()
	int32 CalfLeftIndex;
	UPROPERTY()
	int32 FootRightIndex;
	UPROPERTY()
	int32 FootLeftIndex;

	// Limbs Setup
	UPROPERTY()
	FTransform ClavicleRightToRibcageGen;
	UPROPERTY()
	FTransform ClavicleLeftToRibcageGen;
	UPROPERTY()
	FTransform UpperarmRightToClavicleGen;
	UPROPERTY()
	FTransform UpperarmLeftToClavicleGen;
	UPROPERTY()
	FTransform ThighRightToPelvisGen;
	UPROPERTY()
	FTransform ThighLeftToPelvisGen;
	UPROPERTY()
	FTransform FootRightToFloorGen;
	UPROPERTY()
	FTransform FootLeftToFloorGen;
	UPROPERTY()
	float FootBoneHeight;

	// Spine and Neck setup
	UPROPERTY()
	TMap<int32, FSpineBonePreset> SpineBones;
	UPROPERTY()
	TArray<int32> SpineBoneIndices;
	UPROPERTY()
	int32 SpineBones_RibcageIndex;
	UPROPERTY()
	float SpineLength;
	UPROPERTY()
	float NeckLength;
	UPROPERTY()
	FTransform HeadBoneOffsetfromJoint;
	// Convert transform from generic ribcage to target spine bone
	UPROPERTY()
	FTransform RibcageBoneOffsetFromJoint;

	// Converting Bones Space generic -> skeleton
	UPROPERTY()
	TMap<int32, FTransform> SkeletonTransforms;

	// Current hand transforms in world space:
	// Upperarm --> Lowerarm --> Hand
	UPROPERTY()
	TArray<FTransform> CapturedSkeletonR;
	UPROPERTY()
	TArray<FTransform> CapturedSkeletonL;
	/** Procedural animation. 'Standing' rotation of pelvis, i. e. this rotator includes only Yaw coordinate */
	UPROPERTY()
	FQuat LastRotationOffset;
	UPROPERTY()
	FRotator LastPelvisYaw;
	UPROPERTY()
	float SavedPelvisYaw;
	UPROPERTY()
	FRotator LastPelvisYawBlendStart;
	UPROPERTY()
	bool bRestorePelvisYawRotation;
	UPROPERTY()
	float RestorePelvisSoftAngleTime;

	UPROPERTY()
	FVector SavedIdleBodyLocation;
	UPROPERTY()
	float SavedBodyLoc_UpdateTime;
	UPROPERTY()
	float WalkingStartedTime;

	UPROPERTY()
	float HeadWalkingTime;
	// -1 unknown, 0 left, 1 right
	UPROPERTY()
	int32 WalkingFirstLeg;
	UPROPERTY()
	FRotator SavedWalkDirection;
	UPROPERTY()
	float StepPhase;
	UPROPERTY()
	FTransform FootRightRaw;
	UPROPERTY()
	FTransform FootLeftRaw;

	UPROPERTY()
	bool bHasChair = false;
	FTransform ChairTransform;

	// Limbs size with scale applied
	UPROPERTY()
	FSkeletalMeshSizeData BoneSize;
	UPROPERTY()
	FSkeletalMeshSizeData BoneSizeScaled;
	UPROPERTY()
	FVector MeshScale;
	UPROPERTY()
	FVector MeshSize;
	// Arm span and height multipliers used for NN inputs.
	// NN was trained for HMD_height == 170cm and controllers_distance == 140cm (approximately)
	UPROPERTY()
	FVector NnInputScale;

	// Transforms of bones in actor space to avoid recalculation
	UPROPERTY()
	TArray<FTransform> SnapshotTransformsCache;
	UPROPERTY()
	TArray<uint8> SnapshotTransformsCache_State;

	// Hands Attachment
	UPROPERTY()
	bool bHandAttachedRight;
	UPROPERTY()
	bool bHandAttachedLeft;
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

	/** Output speed data for movement from controllers (i. e. ground ref component movement) */
	float WalkingSpeed;
	float WalkingSpeedMean;
	TArray<float> WalkingSpeedHistory;
	int32 WalkingSpeedHistoryIndex;
	float WalkingYaw;

	/** Physical room-scale novement */
	FVector HeadVelocity2D;
	TArray<FVector> HeadVelocityHistory;
	int32 HeadVelocityIndex;

	// Average right vector for torso computed from left to right hand
	TArray<FVector> TorsoYawByHands;
	int32 TorsoYawByHandsIndex;
	FVector TorsoYawByHandsSum;
	FVector TorsoYawByHandsMean;

	// Helper functions

	// Analyze skeleton to create generic-to-real sceleton transformations setup
	bool BuildSkeletalModel();

	// Update BonesActorSpace for spine/neck
	FTransform CalculateBezierSpine(const FTransform& LowerTr, const FTransform& UpperTr, int32 LowerIndex, int32 UpperIndex, float CurveLength, bool bDrawDebugGeometry = false);
	FTransform CalculateNeck(const FTransform& LowerTr, const FTransform& UpperTr, int32 LowerIndex, int32 UpperIndex, bool bDrawDebugGeometry = false);
	FTransform CalculateNeckNew(const FTransform& Ribcage, const FTransform& Head, bool bRibacgeInBoneSpace, bool bApplyOffset);

	// Apply FABRIK solver for spine
	void CalcTorsoIK(bool bDebugDraw = false);

	// Very simple clavicles rotation
	void CalcShouldersWithOffset(EControllerHand Hand, float DeltaTime);

	// Convert actor-space transforms (in fact, transforms relative to GroundRef component) to local space
	void UpdatePoseSnapshot();

	// Get transform of bone relative to ground ref component from CurrentPoseSnapshot
	FTransform RestoreSnapshotTransformInAS(const FReferenceSkeleton& RefSkeleton, const FTransform& MeshToGround, const int32 CurrentBoneIndex);
	FVector CalculateElbow(const FTransform& Ribcage, const FTransform& Shoulder, const FTransform& Wrist, bool bRight);

private:
	// Everything related to neural net is here

	// previous transform of base component in world space
	UPROPERTY()
	FTransform LastRefTransform;
	// previous transform of head in component space
	UPROPERTY()
	FTransform LastHeadTargetTransform;

	// last received values from NN: [coordinate][frame]
	float RuntimeValues[RUNVALUES_NUM][SD_NUM];
	// standard deviations of recent received values from NN: [coordinate]
	float RuntimeSdSum[RUNVALUES_NUM];
	// sum of recent received values from NN: [coordinate]
	float RuntimeSoftSum[RUNVALUES_NUM];
	// index of current frame in all lists above
	int32 RuntimeIndex;
	// history of input velocities
	TArray<TArray<FVector>> VelocitiesHistory;
	// previous transforms of head and hands in world space
	UPROPERTY()
	TArray<FTransform> LastTargetsTransforms;
	// previous transform of GroundRef component
	UPROPERTY()
	FTransform LastActorTransform;
	// is model loaded in TorchModule?
	UPROPERTY()
	bool bPyTorchAvailable;
	// Input tensor
	UPROPERTY()
	FSimpleTensor NnInputs;
	// Output tensor
	UPROPERTY()
	FSimpleTensor NnOutputs;

	UPROPERTY()
	USimpleTorchController* TorchModule;

	int32 CriticalHandsToHeadRotation = 0;

	// Fill NN inputs, execute call, extract and unscale NN outputs
	UFUNCTION()
	void BuildBodyInternal_YNN(float DeltaTime);
	// Use procedural functions to build body IK targets
	UFUNCTION()
	void BuildBodyInternal_Procedural(float DeltaTime);
	// update velocities from component and IKTargets.Head
	UFUNCTION()
	void UpdateVelocitiesInternal(float DeltaTime);
	// internal to procedurally build ribcage and pelvis IK targets
	UFUNCTION()
	void BuildTorsoTargets(float DeltaTime);
	UFUNCTION()
	void BuildTorsoTargetsSG(float DeltaTime);
	// internal to procedurally build feet IK targets
	UFUNCTION()
	void BuildFeetTargets(float DeltaTime);

	// Feet IK adjustment for ground
	FTransform TestFootToGround(const FTransform& FootTarget, bool bIsRight, bool bInputBoneWorldSpace) const;

	// Apply filter and smoothing to NN output float value
	float FilterNNValue(int32 Index, float Value, float* ValuesArray, float& SD_Sum, float& Soften_Sum, bool bPelvis);

	FTransform GetHandTargetTransform(bool bRightHand, bool bWorldSpace) const;
	FTransform GetHMDTransform(bool bWorldSpace) const;

	FString GetResourcesPath();

	FORCEINLINE float GetCurveValue(UCurveFloat* Curve1, UCurveFloat* Curve2, float X) const;
};