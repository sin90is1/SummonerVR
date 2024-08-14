// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interaction/CombatInterface.h"
#include "Actors/WidgetContainerActor.h"
#include "VRPawnBase.generated.h"


class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;

// 
// //for VR HUD
//  class AWidgetContainerActor;
//  class UVRUserWidget;

UCLASS()
class SUMMONERVR_API AVRPawnBase : public ACharacter, public  IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:

	AVRPawnBase();

protected:

	virtual void BeginPlay() override;

public:	

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetUAttributeSet() const { return AttributeSet; }

	// Add a UPROPERTY to reference the WidgetContainerActor
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Widget")
    AWidgetContainerActor* WidgetContainerActor;

	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	virtual void Die() override;

protected:

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	virtual void InitAbilityActorInfo();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	virtual void InitializeDefaultAttributes() const;

	void AddCharacterAbilities();

	/* Dissolve Effect*/
	void Dissolve();

	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(const TArray<UMaterialInstanceDynamic*>& ArrayOfDynamicMaterialInstance);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TObjectPtr<UMaterialInstance>> DissolveMaterialInstances;

protected:
	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> WeaponToUse;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

	virtual FVector GetCombatSocketLocation_Implementation() override;

	virtual FRotator GetCombatSocketRotation_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void BP_SetWeapon(USkeletalMeshComponent* Weapon);

private:

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<UAnimMontage> HitReactMontage;
};
