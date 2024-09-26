// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "VRBeam.generated.h"


class UBoxComponent;
class UNiagaraSystem;

UCLASS()
class SUMMONERVR_API AVRBeam : public AActor
{
	GENERATED_BODY()
	
public:	

	AVRBeam();

	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

protected:

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION()
	void OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	TMap<FActiveGameplayEffectHandle, UAbilitySystemComponent*> ActiveEffectHandles;

// protected:
// 
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
// 	TSubclassOf<UGameplayEffect> DurationGameplayEffectClass;
// 	
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
// 	TSubclassOf<UGameplayEffect> InfiniteGameplayEffectClass;

private:

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> BoxCol;

// 	UPROPERTY(EditAnywhere)
// 	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> LoopingSound;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;
};
