// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "GameplayEffectExtension.h"


UVR_AttributeSetBase::UVR_AttributeSetBase()
{
	InitHealth(70.f);
	InitMaxHealth(200.f);

	InitMana(30.f);
	InitMaxMana(100.f);

	InitEnergy(50.f);
	InitMaxEnergy(100.f);
}

void UVR_AttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
 	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		Health.SetCurrentValue(FMath::Clamp(Health.GetCurrentValue(), 0.f, MaxHealth.GetCurrentValue()));
		Health.SetBaseValue(FMath::Clamp(Health.GetBaseValue(), 0.f, MaxHealth.GetCurrentValue()));
		

	}

	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		Mana.SetCurrentValue(FMath::Clamp(Mana.GetCurrentValue(), 0.f, MaxMana.GetCurrentValue()));
		Mana.SetBaseValue(FMath::Clamp(Mana.GetBaseValue(), 0.f, MaxMana.GetCurrentValue()));

	}

	if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
	{
		Energy.SetCurrentValue(FMath::Clamp(Energy.GetCurrentValue(), 0.f, MaxEnergy.GetCurrentValue()));
		Energy.SetBaseValue(FMath::Clamp(Energy.GetBaseValue(), 0.f, MaxEnergy.GetCurrentValue()));

	}
 }
