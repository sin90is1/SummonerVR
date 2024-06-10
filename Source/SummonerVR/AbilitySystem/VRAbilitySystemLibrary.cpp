// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/VRAbilitySystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UI/WidgetController/VRWidgetController.h"
#include "VRPawnBase.h"
#include "UI/HUD/VRHUD.h"

UOverlayWidgetController* UVRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		
		if (AVRPawnBase* PlayerPawn = Cast<AVRPawnBase>(PC->GetPawn()))
		{
			UAbilitySystemComponent* ASC = PlayerPawn->GetAbilitySystemComponent();
			UAttributeSet* AS = PlayerPawn->GetUAttributeSet();
			const FWidgetControllerParams WidgetControllerParams(PC, ASC, AS);

			if (AVRHUD* VRHUD = Cast<AVRHUD>(PC->GetHUD())) 
			{
				return VRHUD->GetOverlayWidgetController(WidgetControllerParams);
			}

		}
	}
	return nullptr;
}
