// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "VRAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class SUMMONERVR_API UVRAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:

	static UVRAssetManager& Get();

protected:

	virtual void StartInitialLoading() override;
};
