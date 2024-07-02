// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/VRAbilitySystemLibrary.h"
#include "Game/VRGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "UI/WidgetController/VRWidgetController.h"
#include "VRPawnBase.h"


UOverlayWidgetController* UVRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (AVRPawnBase* PlayerPawn = Cast<AVRPawnBase>(PC->GetPawn()))
		{
			// Use the WidgetContainerActor property directly
			if (AWidgetContainerActor* WidgetContainer = Cast<AWidgetContainerActor>(PlayerPawn->WidgetContainerActor))
			{
				UAbilitySystemComponent* ASC = PlayerPawn->GetAbilitySystemComponent();
				UAttributeSet* AS = PlayerPawn->GetUAttributeSet();
				const FWidgetControllerParams WidgetControllerParams(PC, ASC, AS);

				return WidgetContainer->GetOverlayWidgetController(WidgetControllerParams);
			}
		}
	}
	return nullptr;
}

UAttributeMenuWidgetController* UVRAbilitySystemLibrary::GetAttributeMenuWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (AVRPawnBase* PlayerPawn = Cast<AVRPawnBase>(PC->GetPawn()))
		{
			// Use the WidgetContainerActor property directly
			if (AWidgetContainerActor* WidgetContainer = Cast<AWidgetContainerActor>(PlayerPawn->WidgetContainerActor))
			{
				UAbilitySystemComponent* ASC = PlayerPawn->GetAbilitySystemComponent();
				UAttributeSet* AS = PlayerPawn->GetUAttributeSet();
				const FWidgetControllerParams WidgetControllerParams(PC, ASC, AS);

				return WidgetContainer->GetAttributeMenuWidgetController(WidgetControllerParams);
			}
		}
	}
	return nullptr;
}

void UVRAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);

	FGameplayEffectContextHandle PrimaryAttributesContextHandle = ASC->MakeEffectContext();
	PrimaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimaryAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.PrimaryAttributes, Level, PrimaryAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*PrimaryAttributesSpecHandle.Data.Get());

	FGameplayEffectContextHandle SecondaryAttributesContextHandle = ASC->MakeEffectContext();
	SecondaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle SecondaryAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->SecondaryAttributes, Level, SecondaryAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*SecondaryAttributesSpecHandle.Data.Get());

	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->VitalAttributes, Level, VitalAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

void UVRAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);

	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}
}

UCharacterClassInfo* UVRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	AVRGameModeBase* VRGameMode = Cast<AVRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (VRGameMode == nullptr) return nullptr;

	return VRGameMode->CharacterClassInfo;
}
