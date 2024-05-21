// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/VRHUD.h"
#include "UI/Widget/VRUserWidget.h"

void AVRHUD::BeginPlay()
{
	Super::BeginPlay();

	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	Widget->AddToViewport();
}
