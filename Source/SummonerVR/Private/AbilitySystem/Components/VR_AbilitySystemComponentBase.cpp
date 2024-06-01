// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"

void UVR_AbilitySystemComponentBase::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UVR_AbilitySystemComponentBase::EffectApplied);
}

void UVR_AbilitySystemComponentBase::EffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	GEngine->AddOnScreenDebugMessage(1, 8.f, FColor::Blue, FString("Effect Applied!"));
}
