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
		VRAttributeSet->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
	);



	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetManaAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnManaChanged.Broadcast(Data.NewValue);
			}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxManaAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxManaChanged.Broadcast(Data.NewValue);
			}
	);



	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetEnergyAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnEnergyChanged.Broadcast(Data.NewValue);
			}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		VRAttributeSet->GetMaxEnergyAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxEnergyChanged.Broadcast(Data.NewValue);
			}
	);

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