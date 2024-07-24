// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once

#include "VRIKBody.h"

// Math statistics helper functions

namespace VRIKBodyMath {

	// Correlation coefficients for two sets of vectors
	// a[3][Num], b[3][Num] - vector data sets, r1, r2, r3 - references for a ñorrelation coefficients
	void IKB_CorrelateArraysExt(const float a[][CORR_POINTS_NUM], const float b[][CORR_POINTS_NUM], const int32 Num, float &r1, float &r2, float &r3);

	// Correlation coefficients for two sets of floats
	// a - velocity set[3][Num], index 0..3 - array indexes to compare, r - return value
	void IKB_CorrelateFloatsExt(const float* a, const float* b, const int32 Num, float &r);

	// Calculates approximate rotation centre by four points in spasce
	// Not in use
	FVector IKB_CalcPointRotationCentreExt(const float* locdata[CORR_POINTS_NUM]);

	// Use Inverse kinematics to rotate collarbone
	// Not in use
	float IKB_AdjustRibcageYawForShoulder(FVector HandLoc, FVector RibcageLoc, FRotator RibcageRot, const float HandLength, const float ShoulderRadius);

	// Calculate Squard Dispersion for array a[Num]
	void IKB_CalcDispersionSquared(const float* a, const int32 Num, float &s2);

	// Calculate average Value for array a[Num]
	void IKB_Average(const float* a, const int32 Num, float &a_);

	// Add relative offset AddRotator to existing rotation RotatorToModify
	void IKB_AddRelativeRotation(FRotator& RotatorToModify, const FRotator& AddRotator);

};
