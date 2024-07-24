// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "keras2cpp/model.h"
#include "YnnkVRIKTypes.h"
#include "KerasElbow.generated.h"

/**
* Helper class to compute elbow targets
*/
UCLASS()
class VRIKBODYRUNTIME_API UKerasElbow : public UObject
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UKerasElbow();

	virtual void BeginDestroy() override;

	// Should save request time?
	UPROPERTY(BlueprintReadWrite, Category = "Elbow")
	bool bDebugInfo;

	// Static function to create and initialize new UKerasElbow object
	UFUNCTION(BlueprintCallable, Category = "Elbow")
	static UKerasElbow* CreateKerasElbowModel(FString ModelFileName, bool bCollectDebugInfo, int32 SmoothNum = 5);

	// Load elbows model from file
	UFUNCTION(BlueprintCallable, Category = "Elbow")
	void Initialize(FString FileName, int32 SmoothNum);

	// Compute elbows location
	UFUNCTION(BlueprintCallable, Category = "Elbow")
	void Evaluate(const FTransform& RibcageRaw, const FTransform& HandR, const FTransform& HandL, FVector& JointTargetRight, FVector& JointTargetLeft);

	// Get average request time
	UFUNCTION(BlueprintPure, Category = "Elbow")
	float GetRequestTimeMean() const;

	// Get duration of the latest request
	UFUNCTION(BlueprintPure, Category = "Elbow")
	float GetLastRequestTime() const;

	// Debug: get last platform time in seconds
	UFUNCTION(BlueprintPure, Category = "Elbow")
	float GetPlatformSeconds() const { return (float)FPlatformTime::Seconds(); }

private:
	// Is model loaded from file to object?
	bool bModelLoaded;
	
	// Smoothen N last frames
	int32 SmoothFrames = 5;

	// Accumulated time of all requests
	double RequestTimeAccumulated;
	// Time of the last request
	double LastTimeInMs;
	// Total number of Evaluate(...) requests
	uint64 FramesNumAccumulated;

	// Right hand rotator from the last request
	FRotator3f PrevHandR;
	// Left hand rotator from the last request
	FRotator3f PrevHandL;
	
	// Results of recent requests
	TArray<FVector> HistoryRight;
	TArray<FVector> HistoryLeft;
	int32 SaveIndex = 0;

	// Keras model loaded from file
	keras2cpp::Model* KerasModel = nullptr;
};
