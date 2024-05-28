// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/VREffectActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

// Sets default values
AVREffectActor::AVREffectActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));

}

// Called when the game starts or when spawned
void AVREffectActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AVREffectActor::BP_ApplyEffectToTarget(AActor* Target, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	//0.best way to get ASC
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (TargetASC == nullptr) return;

	check(GameplayEffectClass);

	//3. MakeEffectContext means is like telling us who is causing the effect and who is taking it and what is it's tags(I think)
	//we create it's handle which again is a wrap class and tell it what is it's source and Data is the true context 
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);

	//2.any ASC can make an EffectSpect using MakeOutgoingSpec function that actually make the wrapper of EffectSpect that is effectspectHandle but this function needs 
	//an EffectContextHandle that we make in 3.
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, 1.f, EffectContextHandle);

	//1.one way to Apply a GameplayEffect is using "ApplyGameplayEffectSpecToSelf" and it needs a const FGameplayEffectSpec
	TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

}

