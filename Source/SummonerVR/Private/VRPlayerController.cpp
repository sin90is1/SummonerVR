// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "Input/VRInputComponent.h"


void AVRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UVRInputComponent* VRInputComponent = CastChecked<UVRInputComponent>(InputComponent);
	VRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}


void AVRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	//GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Red, *InputTag.ToString());
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
