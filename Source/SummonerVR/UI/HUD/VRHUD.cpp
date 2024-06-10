// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/VRHUD.h"
#include "UI/Widget/VRUserWidget.h"
#include "VRPawnBase.h"
#include "UI/WidgetController/OverlayWidgetController.h"

#include "Actors/WidgetContainerActor.h"


UOverlayWidgetController* AVRHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();

		return OverlayWidgetController;
	}
	return OverlayWidgetController;
}

void AVRHUD::InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_VRHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_VRHUD"));

	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UVRUserWidget>(Widget);

	const FWidgetControllerParams WidgetControllerParams(PC, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	OverlayWidget->SetWidgetController(WidgetController);
	WidgetController->BroadcastInitialValues();

/*	Widget->AddToViewport();*/

}
