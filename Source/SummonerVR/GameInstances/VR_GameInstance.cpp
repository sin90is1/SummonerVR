// Fill out your copyright notice in the Description page of Project Settings.


#include "GameInstances/VR_GameInstance.h"

void UVR_GameInstance::SaveCalibration(float Height, float ArmSpan)
{
	bCalibrated = true;

	PlayerHeight = Height;
	PlayerArmSpan = ArmSpan;
}

void UVR_GameInstance::LoadCalibration(bool& OutCalibrated, float& OutHeight, float& OutArmSpan) const
{
	OutCalibrated = bCalibrated;
	OutHeight = PlayerHeight;
	OutArmSpan = PlayerArmSpan;
}
