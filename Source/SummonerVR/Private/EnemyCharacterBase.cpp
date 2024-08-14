// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyCharacterBase.h"

#include "SummonerVR.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework\CharacterMovementComponent.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/VRAbilitySystemLibrary.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/VRUserWidget.h"
#include "VRGameplayTags.h"
#include "AI/VRAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"


//Sets default values
AEnemyCharacterBase::AEnemyCharacterBase()
{
	
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CapsuleHalfHeight = 96.0f;
	CapsuleRadius = 42.0f;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);

	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);


	HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void AEnemyCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!HasAuthority()) return;

	VRAIController = Cast<AVRAIController>(NewController);
	VRAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	VRAIController->RunBehaviorTree(BehaviorTree);
	VRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), false);
	VRAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);

}

// Called when the game starts or when spawned
void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	InitAbilityActorInfo();
	UVRAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);


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

		AbilitySystemComponent->RegisterGameplayTagEvent(FVRGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
		this,
			&AEnemyCharacterBase::HitReactTagChanged
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

void AEnemyCharacterBase::InitializeDefaultAttributes() const
{
	UVRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
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

void AEnemyCharacterBase::Die()
{
	SetLifeSpan(LifeSpan);

	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	HealthBar->DestroyComponent();
	Super::Die();

}

AActor* AEnemyCharacterBase::GetCombatTarget_Implementation() const
{
	return CombatTarget;
}

void AEnemyCharacterBase::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	CombatTarget = InCombatTarget;
}

void AEnemyCharacterBase::HitReactTagChanged(const FGameplayTag CallBackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
	VRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
}
