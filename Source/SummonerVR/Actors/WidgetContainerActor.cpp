// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WidgetContainerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/VRUserWidget.h"
#include "VRPawnBase.h"
#include "UI/WidgetController/OverlayWidgetController.h"


	//VR
AWidgetContainerActor::AWidgetContainerActor()
{
	ShowedWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ShowedWidget"));
	ShowedWidget->SetDrawAtDesiredSize(true);
	ShowedWidget->SetupAttachment(RootComponent);
}

UOverlayWidgetController* AWidgetContainerActor::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
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

void AWidgetContainerActor::InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_VRWidgetContainerActor"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_VRWidgetContainerActor"));

	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UVRUserWidget>(Widget);

	const FWidgetControllerParams WidgetControllerParams(PC, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	OverlayWidget->SetWidgetController(WidgetController);
	WidgetController->BroadcastInitialValues();

	//VR
	SetOverlayWidget();
}


void AWidgetContainerActor::BP_InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	AWidgetContainerActor::InitOverlay( PC,ASC, AS);
}

//VR
 void AWidgetContainerActor::SetOverlayWidget()
 {
	if (OverlayWidget && ShowedWidget)
 	{
		ShowedWidget->SetWidget(OverlayWidget);
	}
 }
