// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "VRIKFingersSolverSetup.h"
#include "Curves/CurveFloat.h"

UVRIKFingersSolverSetup::UVRIKFingersSolverSetup(const FObjectInitializer& ObjectInitializer)
	: TraceHalfDistance(5.f)
	, TraceChannel(ECollisionChannel::ECC_Visibility)
	, bTraceByObjectType(false)
	, InputMinRotation(0.f)
	, InputMaxRotation(75.f)
	, PosesInterpolationSpeed(12.f)
{
}