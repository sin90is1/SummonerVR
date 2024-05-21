// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "VR_AttributeSetBase.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)


UCLASS()
class SUMMONERVR_API UVR_AttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

	public:

	UVR_AttributeSetBase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxHealth)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Health)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Mana)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxMana)


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Energy)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Character")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxEnergy)


protected:

virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

};