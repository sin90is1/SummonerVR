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
	AVRGameModeBase* VRGameMode = Cast<AVRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (VRGameMode == nullptr) return;

	AActor* AvatarActor = ASC->GetAvatarActor();

	UCharacterClassInfo* CharacterClassInfo = VRGameMode->CharacterClassInfo;
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
