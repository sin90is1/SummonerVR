// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VR_GameInstance.generated.h"

/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVR_GameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable)
	void SaveCalibration(float Height, float ArmSpan);

	UFUNCTION(BlueprintPure)
	void LoadCalibration(bool& OutCalibrated, float& OutHeight, float& OutArmSpan) const;

private:

	bool bCalibrated = false;
	float PlayerHeight = 0.f;
	float PlayerArmSpan = 0.f;
};
