// Fill out your copyright notice in the Description page of Project Settings.


#include "VRFletcherPawn.h"

#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"

#include "VRPlayerController.h"
/*#include "UI/HUD/VRHUD.h"*/


AVRFletcherPawn::AVRFletcherPawn()
{
}

void AVRFletcherPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilityActorInfo();
	//not needed because of 3d widget
// 	if (AVRPlayerController* VRPlayerController = Cast<AVRPlayerController>(GetController()))
// 	{
// 		if (AVRHUD* VRHUD = Cast<AVRHUD>(VRPlayerController->GetHUD()))
// 		{
// 			VRHUD->InitOverlay(VRPlayerController, AbilitySystemComponent, AttributeSet);
// 		}
// 	}
}

void AVRFletcherPawn::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UVR_AbilitySystemComponentBase>(AbilitySystemComponent)->AbilityActorInfoSet();
	InitializeDefaultAttributes();
};

