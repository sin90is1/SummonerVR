// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/VRWidgetController.h"

void UVRWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController = WCParams.PlayerController;
	AbilitySystemComponent = WCParams.AbilitySystemComponent;
	AttributeSet = WCParams.AttributeSet;
}
