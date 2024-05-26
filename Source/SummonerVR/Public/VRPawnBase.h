// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "VRPawnBase.generated.h"


class UAbilitySystemComponent;
class UAttributeSet;

// 
// //for VR HUD
// class AWidgetContainerActor;
//  class UVRUserWidget;

UCLASS()
class SUMMONERVR_API AVRPawnBase : public APawn, public  IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AVRPawnBase();

protected:

	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UAttributeSet* GetUAttributeSet() const { return AttributeSet; }

protected:

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

private:
	virtual void PossessedBy(AController* NewController) override;


//vr
	//UPROPERTY(EditAnywhere, Category = "Widgets")
   // TObjectPtr<UChildActorComponent> WidgetContainerActorComponent;
};
