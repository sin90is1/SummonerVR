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

	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	/** end Combat Interface */

private:
	virtual void InitAbilityActorInfo() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;

};
