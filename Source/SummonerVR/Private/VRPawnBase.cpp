// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawnBase.h"

#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "AbilitySystem/Components/VR_AbilitySystemComponentBase.h"
#include "VRGameplayTags.h"
#include "Components/CapsuleComponent.h"

// Sets default values
AVRPawnBase::AVRPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UVR_AbilitySystemComponentBase>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UVR_AttributeSetBase>("AttributeSetBase");
}

// Called when the game starts or when spawned
void AVRPawnBase::BeginPlay()
{
	Super::BeginPlay();
	//enemy dose not have super on its
}

UAbilitySystemComponent* AVRPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAnimMontage* AVRPawnBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void AVRPawnBase::Die()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dissolve();
	bDead = true;
}

USkeletalMeshComponent* AVRPawnBase::GetCombatMesh_Implementation() const
{
	return GetMesh();
}

FName AVRPawnBase::GetCombatSocketName_Implementation(const FGameplayTag& MontageTag)
{

	const FVRGameplayTags& GameplayTags = FVRGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_RightHand))
	{
		return RightHandSocketName;
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_LeftHand))
	{
		return LeftHandSocketName;
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_Weapon) && IsValid(WeaponToUse))
	{
		return WeaponTipSocketName;
	}
	return FName();
}

TArray<FTaggedMontage> AVRPawnBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

AActor* AVRPawnBase::GetAvatar_Implementation()
{
	return this;
}

bool AVRPawnBase::IsDead_Implementation() const
{
	return bDead;
}

void AVRPawnBase::InitAbilityActorInfo()
{
}

void AVRPawnBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void AVRPawnBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void AVRPawnBase::AddCharacterAbilities()
{
	UVR_AbilitySystemComponentBase* AuraASC = CastChecked<UVR_AbilitySystemComponentBase>(AbilitySystemComponent);
/*	if (!HasAuthority()) return;*/

	AuraASC->AddCharacterAbilities(StartupAbilities);
}

void AVRPawnBase::Dissolve()
{
	if (!DissolveMaterialInstances.IsEmpty())
	{
		// Get the number of materials on the mesh
		int32 MaterialCount = GetMesh()->GetNumMaterials();

		// Ensure we don't exceed the number of materials on the mesh
		int32 InstanceCount = FMath::Min(MaterialCount, DissolveMaterialInstances.Num());
		TArray<UMaterialInstanceDynamic*> DynamicMaterialInstances;

		for (int32 i = 0; i < InstanceCount; ++i)
		{
			UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstances[i], this);
			GetMesh()->SetMaterial(i, DynamicMatInst);
			DynamicMaterialInstances.Add(DynamicMatInst);
		}

		// Call the Blueprint event with the array of dynamic material instances
		StartDissolveTimeline(DynamicMaterialInstances);
	}
}

void AVRPawnBase::BP_SetWeapon(USkeletalMeshComponent* Weapon)
{
	WeaponToUse = Weapon;
}

FRotator AVRPawnBase::GetCombatSocketRotation_Implementation(const FGameplayTag& MontageTag)
{
	const FVRGameplayTags& GameplayTags = FVRGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_RightHand))
	{
		return GetMesh()->GetSocketRotation(RightHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_LeftHand))
	{
		return GetMesh()->GetSocketRotation(LeftHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_Weapon) && IsValid(WeaponToUse))
	{
		return WeaponToUse->GetSocketRotation(WeaponTipSocketName);
	}
	return FRotator();
}

FVector AVRPawnBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	const FVRGameplayTags& GameplayTags = FVRGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_RightHand))
	{
		return GetMesh()->GetSocketLocation(RightHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_LeftHand))
	{
		return GetMesh()->GetSocketLocation(LeftHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_Weapon) && IsValid(WeaponToUse))
	{
		return WeaponToUse->GetSocketLocation(WeaponTipSocketName);
	}
	return FVector();
}

// TArray<TSubclassOf<UGameplayAbility>> AVRPawnBase::GetStartupAbilities()
// {
// 	return StartupAbilities;
// }

