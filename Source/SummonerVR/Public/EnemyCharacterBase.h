// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "VRPawnBase.h"
#include "EnemyCharacterBase.generated.h"


class UCapsuleComponent;
class UWidgetComponent;
class UBehaviorTree;
class AVRAIController;

UCLASS()
class SUMMONERVR_API AEnemyCharacterBase : public AVRPawnBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

	virtual void PossessedBy(AController* NewController) override;

protected:

    // Editable properties for Capsule Size
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Capsule Settings")
    float CapsuleHalfHeight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Capsule Settings")
    float CapsuleRadius;

	
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;
	virtual FVector GetCombatSocketLocation() override;
	virtual FRotator GetCombatSocketRotation() override;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

public:	

	virtual void Tick(float DeltaTime) override;

	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	virtual void Die() override;
	/** end Combat Interface */

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	void HitReactTagChanged(const FGameplayTag CallBackTag, int32 NewCount);

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHitReacting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float BaseWalkSpeed = 250.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float LifeSpan = 5.f;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	ECharacterClass CharacterClass = ECharacterClass::Warrior;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY()
	TObjectPtr<AVRAIController> VRAIController;

};
