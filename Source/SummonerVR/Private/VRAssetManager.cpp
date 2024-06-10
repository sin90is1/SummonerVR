// Fill out your copyright notice in the Description page of Project Settings.


#include "VRAssetManager.h"
#include "VRGameplayTags.h"

UVRAssetManager& UVRAssetManager::Get()
{
	check(GEngine);

	UVRAssetManager* VRAssetManager = Cast<UVRAssetManager>(GEngine->AssetManager);
	return *VRAssetManager;
}

void UVRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	FVRGameplayTags::InitializeNativeGameplayTags();
}
