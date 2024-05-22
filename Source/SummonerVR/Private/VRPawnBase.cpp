// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawnBase.h"

#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "VRPlayerController.h"
#include "UI/HUD/VRHUD.h"

// Sets default values
AVRPawnBase::AVRPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UVR_AbilitySystemComponentBase>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UVR_AttributeSetBase>("AttributeSetBase");

	//AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaxHealth);

}

// Called when the game starts or when spawned
void AVRPawnBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AVRPawnBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AVRPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

UAbilitySystemComponent* AVRPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AVRPawnBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (AVRPlayerController* VRPlayerController = Cast<AVRPlayerController>(GetController()))
	{
		if (AVRHUD* VRHUD = Cast<AVRHUD>(VRPlayerController->GetHUD()))
		{
			VRHUD->InitOverlay(VRPlayerController, AbilitySystemComponent, AttributeSet);
		}
	}

	//GiveAbilities();
	//ApplyStartupEffects();
}

