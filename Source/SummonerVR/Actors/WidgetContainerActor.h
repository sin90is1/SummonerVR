// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Widget/VRUserWidget.h"
#include "WidgetContainerActor.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UVRUserWidget;

struct FWidgetControllerParams;

class UWidgetComponent;

UCLASS()
class SUMMONERVR_API AWidgetContainerActor : public AActor
{
	GENERATED_BODY()

public:

	//VR
	AWidgetContainerActor();

	UPROPERTY()
	TObjectPtr<UVRUserWidget> OverlayWidget;

	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	void InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	UFUNCTION(BlueprintCallable, Category = "Widgets")
	void BP_InitOverlay(APlayerController* PC, UAbilitySystemComponent* ASC, UAttributeSet* AS);


UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UVRUserWidget> OverlayWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;

	//for VR
	private:
    // Add this line
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetComponent> ShowedWidget;

	public:
// Add this line
  UFUNCTION(BlueprintCallable, Category = "Widgets")
    void SetOverlayWidget();

};
