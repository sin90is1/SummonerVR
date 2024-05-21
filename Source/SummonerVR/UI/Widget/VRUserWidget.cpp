// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widget/VRUserWidget.h"

void UVRUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}
