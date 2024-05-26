// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawnBase.h"

#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"

#include "Actors/WidgetContainerActor.h"
#include "UI/Widget/VRUserWidget.h"

#include "VRPlayerController.h"
#include "UI/HUD/VRHUD.h"

#include "Components/ChildActorComponent.h"
#include "Actors/WidgetContainerActor.h"
// #include "UI/Widget/VRUserWidget.h"
// #include "Components/WidgetComponent.h"
// 
// Sets default values
AVRPawnBase::AVRPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UVR_AbilitySystemComponentBase>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UVR_AttributeSetBase>("AttributeSetBase");



	//vr
	// Initialize the WidgetContainerActorComponent
	//WidgetContainerActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("WidgetContainerActorComponent"));
//	WidgetContainerActorComponent->SetupAttachment(RootComponent);

	// Set the child actor class
	//WidgetContainerActorComponent->SetChildActorClass(AWidgetContainerActor::StaticClass());
}

// Called when the game starts or when spawned
void AVRPawnBase::BeginPlay()
{
	Super::BeginPlay();

	//VR
// 	AWidgetContainerActor* WidgetContainerActor = Cast<AWidgetContainerActor>(WidgetContainerActorComponent->GetChildActor());
// 	if (WidgetContainerActor)
// 	{
// 		APlayerController* PC = GetWorld()->GetFirstPlayerController();
// 		if (PC)
// 		{
// 			UAbilitySystemComponent* ASC = nullptr; // Retrieve or create your ASC
// 			UAttributeSet* AS = nullptr; // Retrieve or create your AS
// 			WidgetContainerActor->InitOverlay(PC, ASC, AS);
// 		}
//	}

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


	//AWidgetContainerActor* WidgetContainerActor = Cast<AWidgetContainerActor>(WidgetContainerActorComponent->GetChildActor());

	if (AVRPlayerController* VRPlayerController = Cast<AVRPlayerController>(GetController()))
	{
		if (AVRHUD* VRHUD = Cast<AVRHUD>(VRPlayerController->GetHUD()))
		{
			VRHUD->InitOverlay(VRPlayerController, AbilitySystemComponent, AttributeSet);

			//VR
// 			if (WidgetContainerActor)
// 			{
// 				WidgetContainerActor->InitOverlay(VRPlayerController, AbilitySystemComponent, AttributeSet);
// 			}
			
		}
	}

	//GiveAbilities();
	//ApplyStartupEffects();
}
