// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "VRPlayerController.generated.h"

class UDamageTextComponent;
class UVRInputConfig;
class UVR_AbilitySystemComponentBase;
/**
 * 
 */
UCLASS()
class SUMMONERVR_API AVRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
		UFUNCTION()
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);
protected:

	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;
private:

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UVRInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UVR_AbilitySystemComponentBase> VRAbilitySystemComponent;

	UVR_AbilitySystemComponentBase* GetASC();
};
