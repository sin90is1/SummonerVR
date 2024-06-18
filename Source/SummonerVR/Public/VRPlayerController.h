// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "VRPlayerController.generated.h"

class UVRInputConfig;
/**
 * 
 */
UCLASS()
class SUMMONERVR_API AVRPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	virtual void SetupInputComponent() override;

private:

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UVRInputConfig> InputConfig;
};
