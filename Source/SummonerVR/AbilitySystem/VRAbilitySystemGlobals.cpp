// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/VRAbilitySystemGlobals.h"

#include "VRAbilityTypes.h"

FGameplayEffectContext* UVRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FVRGameplayEffectContext();
}
