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
	AddCharacterAbilities();
	//not needed because of 3d widget
// 	if (AVRPlayerController* VRPlayerController = Cast<AVRPlayerController>(GetController()))
// 	{
// 		if (AVRHUD* VRHUD = Cast<AVRHUD>(VRPlayerController->GetHUD()))
// 		{
// 			VRHUD->InitOverlay(VRPlayerController, AbilitySystemComponent, AttributeSet);
// 		}
// 	}
}

int32 AVRFletcherPawn::GetPlayerLevel()
{
	return Level;
}

void AVRFletcherPawn::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UVR_AbilitySystemComponentBase>(AbilitySystemComponent)->AbilityActorInfoSet();
	InitializeDefaultAttributes();
};

//it works an it is flexible but I think I could Improve it a little in future 
FVector AVRFletcherPawn::GetCombatSocketLocation()
{
  	check(WeaponToUse)
	return WeaponToUse->GetSocketLocation(WeaponTipSocketName);
}


void AVRFletcherPawn::BP_SetWeapon(USkeletalMeshComponent* Weapon)
{
	WeaponToUse = Weapon;
}

