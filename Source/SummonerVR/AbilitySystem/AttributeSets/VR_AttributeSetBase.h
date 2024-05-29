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

UCLASS()
class SUMMONERVR_API UVR_AttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

	public:

	UVR_AttributeSetBase();

	//like PostAttributeChange we should do the clamping on it and not the logic but it's better to do it in PostAttributeChange
	// but we also doing a lot of things in PostAttributeChange so we use this instead
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

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


private:

	void SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const;


protected:

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

};