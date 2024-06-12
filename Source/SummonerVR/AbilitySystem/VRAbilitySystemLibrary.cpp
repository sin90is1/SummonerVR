// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/VRAbilitySystemLibrary.h"
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
