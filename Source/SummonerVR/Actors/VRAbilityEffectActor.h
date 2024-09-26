// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRAbilityEffectActor.generated.h"

UCLASS()
class SUMMONERVR_API AVRAbilityEffectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVRAbilityEffectActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	

};
