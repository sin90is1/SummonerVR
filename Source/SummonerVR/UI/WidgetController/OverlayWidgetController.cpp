// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	const UVR_AttributeSetBase* VRAttributeSet = CastChecked<UVR_AttributeSetBase>(AttributeSet);

	OnHealthChanged.Broadcast(VRAttributeSet->GetHealth());
	OnMaxHealthChanged.Broadcast(VRAttributeSet->GetMaxHealth());

	OnManaChanged.Broadcast(VRAttributeSet->GetMana());
	OnMaxManaChanged.Broadcast(VRAttributeSet->GetMaxMana());

	OnEnergyChanged.Broadcast(VRAttributeSet->GetEnergy());
	OnMaxEnergyChanged.Broadcast(VRAttributeSet->GetMaxEnergy());
}


void UOverlayWidgetController::BindCallbacksToDependencies()
{
	const UVR_AttributeSetBase* VRAttributeSet = CastChecked<UVR_AttributeSetBase>(AttributeSet);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetHealthAttribute()).AddUObject(this, &UOverlayWidgetController::HealthChanged);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxHealthAttribute()).AddUObject(this, &UOverlayWidgetController::MaxHealthChanged);



	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetManaAttribute()).AddUObject(this, &UOverlayWidgetController::ManaChanged);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxManaAttribute()).AddUObject(this, &UOverlayWidgetController::MaxManaChanged);



	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetEnergyAttribute()).AddUObject(this, &UOverlayWidgetController::EnergyChanged);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxEnergyAttribute()).AddUObject(this, &UOverlayWidgetController::MaxEnergyChanged);

	Cast<UVR_AbilitySystemComponentBase>(AbilitySystemComponent)->EffectAssetTags.AddLambda(
		[this](const FGameplayTagContainer& AssetTags)
		{
			for (const FGameplayTag& Tag : AssetTags)
			{
				// For example, say that Tag = Message.HealthPotion
				// "Message.HealthPotion".MatchesTag("Message") will return True, "Message".MatchesTag("Message.HealthPotion") will return False
				FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message"));
				if (Tag.MatchesTag(MessageTag))
				{
					const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag);
					MessageWidgetRowDelegate.Broadcast(*Row);
				}

			}
		}
	);
}


void UOverlayWidgetController::HealthChanged(const FOnAttributeChangeData& Data) const
{
	OnHealthChanged.Broadcast(Data.NewValue);
}


void UOverlayWidgetController::MaxHealthChanged(const FOnAttributeChangeData& Data) const
{
	OnMaxHealthChanged.Broadcast(Data.NewValue);
}

void UOverlayWidgetController::ManaChanged(const FOnAttributeChangeData& Data) const
{
	OnManaChanged.Broadcast(Data.NewValue);
}

void UOverlayWidgetController::MaxManaChanged(const FOnAttributeChangeData& Data) const
{
	OnMaxManaChanged.Broadcast(Data.NewValue);
}

void UOverlayWidgetController::EnergyChanged(const FOnAttributeChangeData& Data) const
{
	OnEnergyChanged.Broadcast(Data.NewValue);
}

void UOverlayWidgetController::MaxEnergyChanged(const FOnAttributeChangeData& Data) const
{
	OnMaxEnergyChanged.Broadcast(Data.NewValue);
}
