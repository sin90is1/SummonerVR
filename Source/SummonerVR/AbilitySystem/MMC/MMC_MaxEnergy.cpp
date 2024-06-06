// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/MMC/MMC_MaxEnergy.h"

#include "AbilitySystem/AttributeSets/VR_AttributeSetBase.h"
#include "Interaction/CombatInterface.h"


UMMC_MaxEnergy::UMMC_MaxEnergy()
{
	StrDef.AttributeToCapture = UVR_AttributeSetBase::GetIntelligenceAttribute();
	StrDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	StrDef.bSnapshot = false;

	RelevantAttributesToCapture.Add(StrDef);
}


float UMMC_MaxEnergy::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// Gather tags from source and target
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	float Str = 0.f;
	GetCapturedAttributeMagnitude(StrDef, Spec, EvaluationParameters, Str);
	Str = FMath::Max<float>(Str, 0.f);

	ICombatInterface* CombatInterface = Cast<ICombatInterface>(Spec.GetContext().GetSourceObject());
	const int32 PlayerLevel = CombatInterface->GetPlayerLevel();

	return 50.f + 2.5f * Str + 15.f * PlayerLevel;
}
