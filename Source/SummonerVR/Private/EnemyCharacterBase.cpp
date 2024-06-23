// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyCharacterBase.h"

#include "SummonerVR.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"


//Sets default values
AEnemyCharacterBase::AEnemyCharacterBase()
{
	
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	RootComponent = CapsuleComponent;

	// Set default values for Capsule Size
	CapsuleHalfHeight = 96.0f;
	CapsuleRadius = 42.0f;
	CapsuleComponent->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>("CharacterMesh");
	CharacterMesh->SetupAttachment(RootComponent);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	CharacterMesh->SetGenerateOverlapEvents(true);
	//change and fix the line below in future
/*	Weapon->SetupAttachment(Get(), WeaponHandSocket);*/

}

// Called when the game starts or when spawned
void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	InitAbilityActorInfo();
}

void AEnemyCharacterBase::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UVR_AbilitySystemComponentBase>(AbilitySystemComponent)->AbilityActorInfoSet();
}

FVector AEnemyCharacterBase::GetCombatSocketLocation()
{
	check(CharacterMesh);
	return CharacterMesh->GetSocketLocation(WeaponTipSocketName);
}

FRotator AEnemyCharacterBase::GetCombatSocketRotation()
{
	check(CharacterMesh)
	return CharacterMesh->GetSocketRotation(WeaponTipSocketName);
}

// Called every frame
void AEnemyCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

int32 AEnemyCharacterBase::GetPlayerLevel()
{
	return Level;
}
