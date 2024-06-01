// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawnBase.h"

#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"

// Sets default values
AVRPawnBase::AVRPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UVR_AbilitySystemComponentBase>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UVR_AttributeSetBase>("AttributeSetBase");
}

// Called when the game starts or when spawned
void AVRPawnBase::BeginPlay()
{
	Super::BeginPlay();

}

UAbilitySystemComponent* AVRPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AVRPawnBase::InitAbilityActorInfo()
{
}

