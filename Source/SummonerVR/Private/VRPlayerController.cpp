// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "Input/VRInputComponent.h"
#include "GameFramework/Character.h"
#include "UI/Widget/DamageTextComponent.h"


void AVRPlayerController::ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter, bool bBlockedHit, bool bCriticalHit)
{
	if (IsValid(TargetCharacter) && DamageTextComponentClass)
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount, bBlockedHit, bCriticalHit);
	}
}

void AVRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UVRInputComponent* VRInputComponent = CastChecked<UVRInputComponent>(InputComponent);
	VRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}


void AVRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
}


void AVRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);
}


void AVRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagHeld(InputTag);
}

UVR_AbilitySystemComponentBase* AVRPlayerController::GetASC()
{
	if (VRAbilitySystemComponent == nullptr)
	{
		VRAbilitySystemComponent = Cast<UVR_AbilitySystemComponentBase>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return VRAbilitySystemComponent;
}
