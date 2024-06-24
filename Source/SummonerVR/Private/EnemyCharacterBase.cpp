// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyCharacterBase.h"

#include "SummonerVR.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/VRUserWidget.h"


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
	CharacterMesh->SetupAttachment(GetRootComponent());
	CharacterMesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	CharacterMesh->SetGenerateOverlapEvents(true);


	HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

}

// Called when the game starts or when spawned
void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	InitAbilityActorInfo();


	if (UVRUserWidget* VRUserWidget = Cast<UVRUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		VRUserWidget->SetWidgetController(this);
	}

	if (const UVR_AttributeSetBase* VrAS = Cast<UVR_AttributeSetBase>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(VrAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(VrAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);

		OnHealthChanged.Broadcast(VrAS->GetHealth());
		OnMaxHealthChanged.Broadcast(VrAS->GetMaxHealth());
	}

}

void AEnemyCharacterBase::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UVR_AbilitySystemComponentBase>(AbilitySystemComponent)->AbilityActorInfoSet();

	InitializeDefaultAttributes();
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
