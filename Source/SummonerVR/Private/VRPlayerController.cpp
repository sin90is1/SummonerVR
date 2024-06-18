// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayerController.h"
#include "Input/VRInputComponent.h"


void AVRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UVRInputComponent* VRInputComponent = CastChecked<UVRInputComponent>(InputComponent);
	VRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}


void AVRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Red, *InputTag.ToString());
}


void AVRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Blue, *InputTag.ToString());
}


void AVRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::Green, *InputTag.ToString());
}
