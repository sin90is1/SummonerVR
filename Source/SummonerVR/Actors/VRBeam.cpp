// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/VRBeam.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "SummonerVR.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AVRBeam::AVRBeam()
{
 	
	PrimaryActorTick.bCanEverTick = false;

	// Create a scene component and make it the root
	USceneComponent* RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	BoxCol = CreateDefaultSubobject<UBoxComponent>("Box");
	BoxCol->SetupAttachment(RootSceneComponent);
	BoxCol->SetCollisionObjectType(ECC_Projectile);
	BoxCol->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxCol->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxCol->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BoxCol->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	BoxCol->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}


void AVRBeam::BeginPlay()
{
	Super::BeginPlay();
	BoxCol->OnComponentBeginOverlap.AddDynamic(this, &AVRBeam::OnBoxOverlap);
	BoxCol->OnComponentEndOverlap.AddDynamic(this, &AVRBeam::OnEndOverlap);
	UGameplayStatics::PlaySoundAtLocation(this, CastSound, GetActorLocation(), FRotator::ZeroRotator);
	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
}



void AVRBeam::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
							int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (DamageEffectSpecHandle.Data.IsValid() && DamageEffectSpecHandle.Data.Get()->GetContext().GetEffectCauser() == OtherActor)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation(), FRotator::ZeroRotator);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, GetActorLocation());
	if (LoopingSoundComponent) LoopingSoundComponent->Stop();

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
	{
		const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}
}


void AVRBeam::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!IsValid(TargetASC)) return;
//remove spec handle with damageeffect spec handle

	TArray<FActiveGameplayEffectHandle> HandlesToRemove;
	for (TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*> HandlePair : ActiveEffectHandles)
	{
		if (TargetASC == HandlePair.Value)
		{
			TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
			HandlesToRemove.Add(HandlePair.Key);
		}
	}
	for (FActiveGameplayEffectHandle& Handle : HandlesToRemove)
	{
		ActiveEffectHandles.FindAndRemoveChecked(Handle);
	}
}







//Save all EffectHandles with TargetASC in the Map and will remove them on EndOverlap based by they TargetASC

//I can have 2, GameplayEffect type, one for "Durational" and other for "Infinite".
//This way I can Activate the durational on EndOverlap and the infinite on Overlap, like a side effect after the overlap.
// I could parent the VREffectActor class to stop using the repetion
