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


USTRUCT()
struct FEffectProperties
{
	GENERATED_BODY()

	FEffectProperties(){}

	FGameplayEffectContextHandle EffectContextHandle;

	UPROPERTY()
	UAbilitySystemComponent* SourceASC = nullptr;

	UPROPERTY()
	AActor* SourceAvatarActor = nullptr;

	UPROPERTY()
	AController* SourceController = nullptr;

	UPROPERTY()
	APawn* SourcePawn = nullptr;

	UPROPERTY()
	UAbilitySystemComponent* TargetASC = nullptr;

	UPROPERTY()
	AActor* TargetAvatarActor = nullptr;

	UPROPERTY()
	AController* TargetController = nullptr;

	UPROPERTY()
	APawn* TargetPawn = nullptr;
};


// typedef is specific to the FGameplayAttribute() signature, but TStaticFunPtr is generic to any signature chosen
//typedef TBaseStaticDelegateInstance<FGameplayAttribute(), FDefaultDelegateUserPolicy>::FFuncPtr FAttributeFuncPtr;
template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;


UCLASS()
class SUMMONERVR_API UVR_AttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

	public:

	UVR_AttributeSetBase();

	TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	//like PostAttributeChange we should do the clamping on it and not the logic but it's better to do it in PostAttributeChange
// but we also doing a lot of things in PostAttributeChange so we use this instead
/*	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;*/

private:

	void SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const;


public:
	/*
	 * Primary Attributes
	 */

	UPROPERTY(BlueprintReadOnly, Category = "Primary Attributes")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Strength);

	UPROPERTY(BlueprintReadOnly, Category = "Primary Attributes")
	FGameplayAttributeData Intelligence;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Intelligence);

	UPROPERTY(BlueprintReadOnly, Category = "Primary Attributes")
	FGameplayAttributeData Defence;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Defence);

	UPROPERTY(BlueprintReadOnly, Category = "Primary Attributes")
	FGameplayAttributeData Vigor;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Vigor);

	
	/*
	 * Secondary Attributes
	 */

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Armor);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData ArmorPenetration;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, ArmorPenetration);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData BlockChance;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, BlockChance);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData CriticalHitChance;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, CriticalHitChance);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData CriticalHitDamage;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, CriticalHitDamage);

		UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData CriticalHitResistance;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, CriticalHitResistance);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData HealthRegeneration;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, HealthRegeneration);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData ManaRegeneration;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, ManaRegeneration);

	UPROPERTY(BlueprintReadOnly, Category = "Secondary Attributes")
	FGameplayAttributeData EnergyRegeneration;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, EnergyRegeneration);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxHealth)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxMana)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, MaxEnergy)

	/*
	 * Vital Attributes
	 */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Health)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Mana)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vital Attributes")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, Energy)
	/*
	*Meta Attributes
	*/

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UVR_AttributeSetBase, IncomingDamage)
};