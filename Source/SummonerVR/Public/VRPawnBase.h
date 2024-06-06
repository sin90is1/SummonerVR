// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "VRPawnBase.generated.h"


class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;

// 
// //for VR HUD
// class AWidgetContainerActor;
//  class UVRUserWidget;

UCLASS()
class SUMMONERVR_API AVRPawnBase : public APawn, public  IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AVRPawnBase();

protected:

	virtual void BeginPlay() override;

public:	

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UAttributeSet* GetUAttributeSet() const { return AttributeSet; }

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


	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	void InitializeDefaultAttributes() const;
};
