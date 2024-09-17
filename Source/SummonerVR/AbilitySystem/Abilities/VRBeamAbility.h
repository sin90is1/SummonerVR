// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/VRDamageGameplayAbility.h"
#include "VRBeamAbility.generated.h"

class AVRBeam;
/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVRBeamAbility : public UVRDamageGameplayAbility
{
	GENERATED_BODY()

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	AActor* SpawnBeam();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVRBeam> BeamClass;
};
