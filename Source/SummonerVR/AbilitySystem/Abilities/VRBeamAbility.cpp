// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/VRBeamAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/VRBeam.h"
#include "Interaction/CombatInterface.h"
#include "VRGameplayTags.h"


void UVRBeamAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

AActor* UVRBeamAbility::SpawnBeam()
{
	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		FVRGameplayTags::Get().Montage_Attack_RightHand);

	const FRotator SocketRotation = ICombatInterface::Execute_GetCombatSocketRotation(
		GetAvatarActorFromActorInfo(),
		FVRGameplayTags::Get().Montage_Attack_RightHand);

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(SocketRotation.Quaternion());

	AVRBeam* Beam = GetWorld()->SpawnActorDeferred<AVRBeam>(
		BeamClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(GetOwningActorFromActorInfo()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	const UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo());
	FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
	EffectContextHandle.SetAbility(this);
	EffectContextHandle.AddSourceObject(Beam);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectContextHandle);

	const FVRGameplayTags GameplayTags = FVRGameplayTags::Get();

	for (auto Pair : DamageTypes)
	{
		const float ScaleDamage = Pair.Value.GetValueAtLevel(GetAbilityLevel());
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, Pair.Key, ScaleDamage);
	}

	Beam->DamageEffectSpecHandle = SpecHandle;
	Beam->FinishSpawning(SpawnTransform);

	// Get the correct skeletal mesh from the interface
	AActor* OwningActor = GetAvatarActorFromActorInfo();
	if (OwningActor)
	{
		// Use the interface to get the correct mesh
		USkeletalMeshComponent* MeshComponent = ICombatInterface::Execute_GetCombatMesh(OwningActor);
		if (MeshComponent)
		{
			// Attach the beam to the mesh at the socket
			FName SocketName = ICombatInterface::Execute_GetCombatSocketName(
				OwningActor,
				FVRGameplayTags::Get().Montage_Attack_RightHand);

			Beam->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
		}
	}

	return Beam;
}

// /*	APawn* CombatSocketName = GetAvatarActorFromActorInfo().Cast(UVRPawnBase)*/
// 	Beam->AttachToActor(GetAvatarActorFromActorInfo(), FAttachmentTransformRules::KeepRelativeTransform)
//somwhere you have to define to get attached to socket