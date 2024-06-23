// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VRPawnBase.h"
#include "EnemyCharacterBase.generated.h"


class UCapsuleComponent;

UCLASS()
class SUMMONERVR_API AEnemyCharacterBase : public AVRPawnBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

protected:
    // Capsule Component
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
    UCapsuleComponent* CapsuleComponent;

    // Editable properties for Capsule Size
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Capsule Settings")
    float CapsuleHalfHeight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Capsule Settings")
    float CapsuleRadius;

	
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual FVector GetCombatSocketLocation() override;
	virtual FRotator GetCombatSocketRotation() override;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

public:	

	virtual void Tick(float DeltaTime) override;

	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	/** end Combat Interface */

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;
};
