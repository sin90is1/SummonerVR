// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/VRGameplayAbility.h"
#include "VRDamageGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVRDamageGameplayAbility : public UVRGameplayAbility
{
	GENERATED_BODY()
	
protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TMap<FGameplayTag, FScalableFloat> DamageTypes;
};
