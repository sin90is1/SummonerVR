// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VRPawnBase.h"
#include "EnemyCharacterBase.generated.h"



UCLASS()
class SUMMONERVR_API AEnemyCharacterBase : public AVRPawnBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

protected:
	
	virtual void BeginPlay() override;

public:	

	virtual void Tick(float DeltaTime) override;


};