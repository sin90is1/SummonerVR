// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/VRGameplayAbility.h"
#include "VRProjectileSpell.generated.h"


class AVRProjectile;
/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVRProjectileSpell : public UVRGameplayAbility
{
	GENERATED_BODY()

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVRProjectile> ProjectileClass;

};
