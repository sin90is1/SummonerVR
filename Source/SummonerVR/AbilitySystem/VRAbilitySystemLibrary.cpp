// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/VRAbilitySystemLibrary.h"
#include "Game/VRGameModeBase.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UI/WidgetController/VRWidgetController.h"
#include "VRPawnBase.h"
#include "VRAbilityTypes.h"


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

void UVRAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return;

	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}

	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(ASC->GetAvatarActor()))
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CombatInterface->GetPlayerLevel());
			ASC->GiveAbility(AbilitySpec);
		}
	}
}

UCharacterClassInfo* UVRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	AVRGameModeBase* VRGameMode = Cast<AVRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (VRGameMode == nullptr) return nullptr;

	return VRGameMode->CharacterClassInfo;
}

bool UVRAbilitySystemLibrary::IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FVRGameplayEffectContext* VRContext = static_cast<const FVRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return VRContext->IsBlockedHit();
	}
	return false;
}

bool UVRAbilitySystemLibrary::IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FVRGameplayEffectContext* VRContext = static_cast<const FVRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return VRContext->IsCriticalHit();
	}
	return false;
}

void UVRAbilitySystemLibrary::SetIsBlockedHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit)
{
	if (FVRGameplayEffectContext* VRContext = static_cast<FVRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		VRContext->SetIsBlockedHit(bInIsBlockedHit);
	}
}

void UVRAbilitySystemLibrary::SetIsCriticalHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsCriticalHit)
{
	if (FVRGameplayEffectContext* VRContext = static_cast<FVRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		VRContext->SetIsCriticalHit(bInIsCriticalHit);
	}
}

void UVRAbilitySystemLibrary::GetLivePlayersWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin)
{
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SphereOrigin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);
		
		for (FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
			}
		}
	}
}
