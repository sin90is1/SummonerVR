// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/VRInputConfig.h"

const UInputAction* UVRInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound /*= false*/) const
{
	for (const FVRInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s], on InputConfig [%s]"), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}