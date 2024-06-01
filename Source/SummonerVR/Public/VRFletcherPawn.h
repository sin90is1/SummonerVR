// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRPawnBase.h"
#include "VRFletcherPawn.generated.h"

/**
 * 
 */
UCLASS()
class SUMMONERVR_API AVRFletcherPawn : public AVRPawnBase
{
	GENERATED_BODY()
public:
	AVRFletcherPawn();
	virtual void PossessedBy(AController* NewController) override;

private:
	virtual void InitAbilityActorInfo() override;
};
