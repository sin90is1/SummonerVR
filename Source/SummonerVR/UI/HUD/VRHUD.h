// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VRHUD.generated.h"

class UVRUserWidget;
UCLASS()
class SUMMONERVR_API AVRHUD : public AHUD
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TObjectPtr<UVRUserWidget> OverlayWidget;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(EditAnywhere)
	TSubclassOf<UVRUserWidget> OverlayWidgetClass;
	
};
