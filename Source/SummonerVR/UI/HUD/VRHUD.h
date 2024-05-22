// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VRHUD.generated.h"


class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UVRUserWidget;

struct FWidgetControllerParams;

UCLASS()
class SUMMONERVR_API AVRHUD : public AHUD
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TObjectPtr<UVRUserWidget> OverlayWidget;

	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	void InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS);



private:

	UPROPERTY(EditAnywhere)
	TSubclassOf<UVRUserWidget> OverlayWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
};
