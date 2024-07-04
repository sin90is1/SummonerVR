// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/VRDamageGameplayAbility.h"
#include "VRProjectileSpell.generated.h"


class AVRProjectile;
class UGameplayEffect;
/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVRProjectileSpell : public UVRDamageGameplayAbility
{
	GENERATED_BODY()

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVRProjectile> ProjectileClass;

};
