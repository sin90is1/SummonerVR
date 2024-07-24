// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VRIKFingersSolverSetup.h"
#include "VRIKFingersTypes.h"
#include "VRIKFingersFKIKSolver.generated.h"

/**
 * Trace and calculate fingers transforms
 * Result saved in FVRIK_Knuckle::RelativeTransform
 */
UCLASS(Blueprintable, meta = (DisplayName = "Fingers FK/IK Solver"))
class VRIKBODYRUNTIME_API UVRIKFingersFKIKSolver : public UObject
{
	GENERATED_BODY()
	
public:
	UVRIKFingersFKIKSolver();

	/**
	* Hand side associated with this object
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	EVRIK_VRHand Hand;

	/**
	* Trace distance from knuckle to inside and outside
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	float TraceHalfDistance;

	/**
	* Trace channel to probe: Visible, Camera etc
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/**
	* Should trace by trace channel (false) or object type (true)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Hand")
	bool bTraceByObjectType;

	/**
	* Out fingers data. Initialized from UVRIKFingersSolverSetup object.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	TMap<EVRIKFingerName, FVRIK_FingerSolver> Fingers;

	/**
	* If valid, Trace() function would ignore all objects but this
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	UPrimitiveComponent* FilterObject;

	/**
	* Lower border for VR input (in degrees). VR input values (0..1) are interpolated to (InputMinRotation, InputMaxRotation)
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	float InputMinRotation;

	/**
	* Upper border for VR input (in degrees). VR input values (0..1) are interpolated to (InputMinRotation, InputMaxRotation)
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	float InputMaxRotation;

	/**
	* Interpolatoin speed for processing poses applied by SetFingersPose function
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Setup")
	float PosesInterpolationSpeed;

	/**
	* Curve to blend between finger poses
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	class UCurveFloat* BlendCurve;

	/**
	* Should draw debug lines for tracing?
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Debug")
	bool bDrawDebugGeometry;

	/**
	* Initialize object. Should be called before using.
	* @param	SolverSetup			Reference to FingersSolverSetup object wtih information about fingers for this hand.
	* @param	SkeletalMeshComp	Reference to controlled skeletal mesh component
	*/
	UFUNCTION()
	bool Initialize(UVRIKFingersSolverSetup* SolverSetup, USkeletalMeshComponent* SkeletalMeshComp);

	/**
	* Create and initialize new FingersFKIKSolver object
	* @param	SolverSetup			Reference to FingersSolverSetup object wtih information about fingers for this hand.
	* @param	SkeletalMeshComp	Reference to controlled skeletal mesh component
	*/
	UFUNCTION(BlueprintCallable, Meta=(DisplayName="Create Fingers FK/IK Solver"), Category = "Fingers Solver")
	static UVRIKFingersFKIKSolver* CreateFingersFKIKSolver(UVRIKFingersSolverSetup* SolverSetup, USkeletalMeshComponent* SkeletalMeshComp);

	/**
	* Update current fingers transform. Call this function at the end of the Update() event in animation blueprint.
	* @param	bTrace				Should do tracing to detect object in hand (reference pose should be initiaized)?
	* @param	bUpdateFingersPose	Should apply fingers pose initialized by SetFingersPose(...)?
	*/
	UFUNCTION(BlueprintCallable, Category = "Fingers Solver")
	void UpdateFingers(bool bTrace = false, bool bUpdateFingersPose = true);

	/**
	* Trace scene to find object to grab and update fingers positions. Don't call this function directly.
	* @param	bTracingInTick		Notifies if function called every tick. If true, interpolation would be used to soften fingers movement. Otherwise, position would be updated instantly.
	*/
	UFUNCTION(BlueprintCallable, Category = "Fingers Solver")
	void Trace(bool bTracingInTick);

	/**
	* Apply fingers curl values from Valve Knuckles or Oculus Touch controllers. This function should be called every Tick.
	* @param	ThumbCurl		Curl value returned by curresponding input axis for thumb finger
	* @param	IndexCurl		Curl value returned by curresponding input axis for index finger
	* @param	MiddleCurl		Curl value returned by curresponding input axis for middle finger
	* @param	AnnularCurl		Curl value returned by curresponding input axis for ring finger
	* @param	MercurialCurl	Curl value returned by curresponding input axis for pinky finger
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Apply VR Input"), Category = "Fingers Solver")
	void ApplyVRInput(const FVRIK_FingersPosePreset& NewFingersRotation);
	
	/**
	* Input values in degrees for all bones of all fingers
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Apply VR Input Detailed"), Category = "Fingers Solver")
	void ApplyVRInputDetailed(const FVRIK_FingersDetailedInfo& NewFingersRotation);

	/**
	* Input values in degrees relative to parent bone rather then reference pose
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Apply VR Input Raw"), Category = "Fingers Solver")
	void ApplyVRInputRaw(const FVRIK_FingersRawPreset& RawFingersRotation);

	/**
	* Apply fingers pose from OpenXR interface
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Apply OpenXR Input"), Category = "Fingers Solver")
	bool ApplyOpenXRInput();

	/**
	* Was object initialized successfully?
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	bool IsInitialized() const { return bIsInitialized; };

	/**
	* Debug function, returns relative transforms of finger knuckles in String
	* @param	FingerName		Name of finger to check
	* @return					String formatted as A [<Alpha>] <KnuckleBone1> [loc=<RelaiveLocation> rol=<RelativeRotation>] <KnuckleBone2> [loc=<RelaiveLocation> rol=<RelativeRotation>] ...
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	FString GetFingerDescription(EVRIKFingerName FingerName) const;

	/**
	* Get information about knuckles for specified finger
	* @param	FingerName		Name of finger to check
	* @return					Array of knuckles with transforms and bone names
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	void GetFingerKnuckles(EVRIKFingerName FingerName, TArray<FVRIK_Knuckle>& OutKnuckles);

	/**
	* Check if finger enabled
	* @param	FingerName		Name of finger to check
	* @return					True if finger was enabled in the solver
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	bool IsFingerEnabled(EVRIKFingerName FingerName) const
	{
		const FVRIK_FingerSolver* Finger = Fingers.Find(FingerName);
		return Finger ? Finger->bEnabled : false;
	};

	/**
	* Set fingers pose from variable
	* @param NewPose variable containing pose to use
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	bool SetFingersPose(const FVRIK_FingersPosePreset& NewPose);

	/**
	* DEPRECATED. Set detailed fingers pose from variable
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	bool SetFingersPoseDetailed(const FVRIK_FingersDetailedInfo& NewPose);

	/**
	* Set new detailed fingers pose from variable
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	bool SetFingersPoseRaw(const FVRIK_FingersRawPreset& NewPose);

	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	void ResetFingersPose();

	/**
	* Set reference pose for fingers tracing. It should be 'grabbing' pose which need
	* to be adjusted to attached object by tracing.
	* @param PoseName Key in FingerPoses map
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	bool SetFingersTraceReferencePose(const FVRIK_FingersPosePreset& NewRefPose);

	/**
	* Set reference pose for VR input from Valve Knuckles or Oculus Touch. It should be pose of fully open hand.
	* @param PoseName Key in FingerPoses map
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands", meta = (DisplayName = "Set VR Input Reference Pose"))
	bool SetVRInputReferencePose(const FVRIK_FingersPosePreset& NewRefPose);

	/**
	* Set reference pose for tightened hand. Component will interpolate VR input between "Grip Reference Pose" and "Input Reference Pose"
	* @param PoseName Key in FingerPoses map, use None to reset
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands", meta = (DisplayName = "Set VR Input Grip Pose"))
	bool SetVRInputGripPose(const FVRIK_FingersPosePreset& NewRefPose);

	/**
	* Clear reference pose for tightened hand.
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	void ResetFingersGripReferencePose();
	
	/**
	* Grab gameplay object by hand. The function doesn't attach object to hand mesh and only apply rotation to fingers.
	* @param GrabReferencePose	Name (key in FingerPoses map) of tracing reference pose. This pose will be adjusted by tracing
	* @param Object				Component to grab
	*/
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	void GrabObject(UPrimitiveComponent* Object);

	/** Return fingers rotation from traced to current pose. */
	UFUNCTION(BlueprintCallable, Category = "VR Hands")
	void ReleaseObject();

	/**
	* Enable or disable finger solving
	* @param	FingerName		Name of finger to enable or disable
	* @param	bNewEnabled		Value to set
	*/
	UFUNCTION(BlueprintCallable, Category = "Fingers Solver")
	void SetFingerEnabled(EVRIKFingerName FingerName, bool bNewEnabled)
	{
		FVRIK_FingerSolver* Finger = Fingers.Find(FingerName);
		if (Finger) Finger->bEnabled = bNewEnabled;
	};

	/**
	* Convert multiplier-based finger orientation (used in poses, VR input) to degrees (used in detailed VR input)
	* @param	InFingerRot		Finger orientation with values from 0 to 1
	* @return					Finger orientation in degrees
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	static FVRIK_FingerRotation ConvertFingerRotationToDegrees(const FVRIK_FingerRotation& InFingerRot)
	{
		FVRIK_FingerRotation OurFRot;
		OurFRot.CurlValue = InFingerRot.CurlValue * 90.f;
		OurFRot.RollValue = InFingerRot.RollValue * 20.f;
		OurFRot.SpreadValue = InFingerRot.SpreadValue * 20.f;
		return OurFRot;
	}

	/**
	* Convert finger orientation in degrees (used in detailed VR input) to multiplier-based (used in poses, VR input)
	* @param	InFingerRot		Finger orientation in degree
	* @return					Finger orientation with values from 0 to 1
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fingers Solver")
	static FVRIK_FingerRotation ConvertFingerRotationFromDegrees(const FVRIK_FingerRotation& InFingerRot)
	{
		FVRIK_FingerRotation OurFRot;
		OurFRot.CurlValue = InFingerRot.CurlValue / 90.f;
		OurFRot.RollValue = InFingerRot.RollValue / 20.f;
		OurFRot.SpreadValue = InFingerRot.SpreadValue / 20.f;
		return OurFRot;
	}

	// Get raw pose for replication
	UFUNCTION(BlueprintPure, Category = "Fingers Solver")
	void GetOpenXRInputAsByteArray(TArray<uint8>& PosePacked) const;

	// Apply repliated pose at remote object
	UFUNCTION(BlueprintCallable, Category = "Fingers Solver")
	void ApplyOpenXRInputFromByteArray(const TArray<uint8>& PosePacked);

private:
	UPROPERTY()
	bool bIsInitialized;

	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	UVRIKFingersSolverSetup* FingersSolverSetup;

	UPROPERTY()
	TMap<EVRIKFingerName, bool> TracingStatus;

	UPROPERTY()
	FVRIK_FingersPosePreset VRInput;

	UPROPERTY()
	FVRIK_FingersDetailedInfo VRInputDetailed;

	UPROPERTY()
	FVRIK_FingersRawPreset VRInputRaw;

	UPROPERTY()
	bool bHasVRInputInFrame;
	
	UPROPERTY()
	bool bHasGripInputPose;

	UPROPERTY()
	bool bHasDetailedVRInputInFrame;
	UPROPERTY()
	bool bDetailedVRInputIsRaw;
	/** Has traced pose for fingers? */
	UPROPERTY()
	bool bUseRuntimeFingersPose;
	/** Trace start time when grabbing object */
	UPROPERTY()
	float TraceStartTime;

	UPROPERTY()
	TMap<EVRIKFingerName, bool> VRStatus;

	UPROPERTY()
	FVRIK_FingersPosePreset CurrentFingersPose;

	UPROPERTY()
	FVRIK_FingersDetailedInfo CurrentFingersPoseDetailed;

	UPROPERTY()
	FVRIK_FingersRawPreset CurrentFingersPoseRaw;

	UPROPERTY()
	bool bHasFingersPose = false;

	UPROPERTY()
	bool bHasRawFingersPose = false;

	UPROPERTY()
	bool bHasDetailedFingersPose = false;

	UPROPERTY()
	float HandSideMultiplier;

	UPROPERTY()
	int32 WristBoneIndex;
	UPROPERTY()
	FName WristBoneName;
	UPROPERTY()
	FTransform WristFromGenericConverter;

	// Used for smooth interpolation
	TMap<EVRIKFingerName, TArray<FQuat> > CachedHandPose;
	// time when interpolation started
	float InterpolationStartTime = 0.f;
	// current interp alpha between poses
	float InterpolationAlpha = 0.f;

	// @TODO: unite pose & input processing to one function
	void ProcessVRInput();
	void ProcessFingersPose(bool bUseInterpolation = true);
	// @TODO: unite pose & input processing to one function
	void ProcessVRInputDetailed();
	void ProcessFingersPoseDetailed(bool bUseInterpolation);
	// Used to apply OpenXR input (no interpolation) or raw pose (interpolated)
	void ProcessFingersPoseRaw(const FVRIK_FingersRawPreset& Pose, bool bUseInterpolation);

	/** Update reference pose for fingers tracing or VR curls input */
	bool UpdateReferencePoseFromPoseName(const FVRIK_FingersPosePreset& NewRefPose, uint8 PoseType);
	FTransform GetKnuckleRefTransform(const FVRIK_FingerSolver& Finger, int32 KnuckleIndex);
	FVector TraceKnuckle(UWorld* World, const FVRIK_FingerSolver& Finger, const FVRIK_Knuckle& Knuckle, const FTransform& KnuckleTr, FHitResult& HitResult, int32& Pass, int32 KnuckleIndex);
	/** Helper. Add rotation around local axes. */
	FRotator AddLocalRotation(const FRotator& AdditionRot, const FRotator& BaseRot);
	/** Helper function to apply rotation to rotator by axis */
	void SetRotationAxisValue(FRotator& OutRot, EVRIK_BoneOrientationAxis Axis, float Value);
	/** Helper function to apply rotation to rotator by axis */
	void AddRotationAxisValue(FRotator& OutRot, EVRIK_BoneOrientationAxis Axis, float Value);

	/* Interpolation between poses: cache current pose and save start time */
	void CacheFingersPose();
};
