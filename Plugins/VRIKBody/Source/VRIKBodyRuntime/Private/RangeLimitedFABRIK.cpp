/*
Copyright (c) Henry Cooney 2017

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

https://github.com/hacoo/rtik
*/

#include "RangeLimitedFABRIK.h"
#include "VRIKBodyPrivatePCH.h"

bool FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
	const TArray<FTransform>& InTransforms,
	const FVector & EffectorTargetLocation,
	TArray<FTransform>& OutTransforms,
	float MaxRootDragDistance,
	float RootDragStiffness,
	float Precision,
	int32 MaxIterations)
{
	OutTransforms.Empty();

	// Number of points in the chain. Number of bones = NumPoints - 1
	int32 NumPoints = InTransforms.Num();

	// Gather bone transforms
	OutTransforms.Reserve(NumPoints);
	for (const FTransform& Transform : InTransforms)
	{
		OutTransforms.Add(Transform);
	}

	if (NumPoints < 2)
	{
		// Need at least one bone to do IK!
		return false;
	}
	
	// Gather bone lengths. BoneLengths contains the length of the bone ENDING at this point,
	// i.e., BoneLengths[i] contains the distance between point i-1 and point i
	TArray<float> BoneLengths;
	float MaximumReach = ComputeBoneLengths(InTransforms, BoneLengths);

	bool bBoneLocationUpdated = false;
	int32 EffectorIndex       = NumPoints - 1;
	
	// Check distance between tip location and effector location
	float Slop = FVector::Dist(OutTransforms[EffectorIndex].GetLocation(), EffectorTargetLocation);
	if (Slop > Precision)
	{
		// Set tip bone at end effector location.
		OutTransforms[EffectorIndex].SetLocation(EffectorTargetLocation);
		
		int32 IterationCount = 0;
		while ((Slop > Precision) && (IterationCount++ < MaxIterations))
		{
			// "Forward Reaching" stage - adjust bones from end effector.
			FABRIKForwardPass(
				InTransforms,
				BoneLengths,
				OutTransforms
			);	

			// Drag the root if enabled
			DragPointTethered(
				InTransforms[0],
				OutTransforms[1],
				BoneLengths[1],
				MaxRootDragDistance,
				RootDragStiffness,
				OutTransforms[0]
			);

			// "Backward Reaching" stage - adjust bones from root.
			FABRIKBackwardPass(
				InTransforms,
				BoneLengths,
				OutTransforms
			);

			Slop = FMath::Abs(BoneLengths[EffectorIndex] - 
				FVector::Dist(OutTransforms[EffectorIndex - 1].GetLocation(), EffectorTargetLocation));
		}

		// Place effector based on how close we got to the target
		FVector EffectorLocation = OutTransforms[EffectorIndex].GetLocation();
		FVector EffectorParentLocation = OutTransforms[EffectorIndex - 1].GetLocation();
		EffectorLocation = EffectorParentLocation + (EffectorLocation - EffectorParentLocation).GetUnsafeNormal() * BoneLengths[EffectorIndex];
		OutTransforms[EffectorIndex].SetLocation(EffectorLocation);
		
		bBoneLocationUpdated = true;
	}
	
	// Update bone rotations
	if (bBoneLocationUpdated)
	{
		for (int32 PointIndex = 0; PointIndex < NumPoints - 1; ++PointIndex)
		{
			if (!FMath::IsNearlyZero(BoneLengths[PointIndex + 1]))
			{
				UpdateParentRotation(OutTransforms[PointIndex], InTransforms[PointIndex],
					OutTransforms[PointIndex + 1], InTransforms[PointIndex + 1]);
			}
		}
	}

	return bBoneLocationUpdated;
}

void FRangeLimitedFABRIK::FABRIKForwardPass(
	const TArray<FTransform>& InTransforms,
	const TArray<float>& BoneLengths,
	TArray<FTransform>& OutTransforms
)
{
	int32 NumPoints     = InTransforms.Num();
	int32 EffectorIndex = NumPoints - 1;

	for (int32 PointIndex = EffectorIndex - 1; PointIndex > 0; --PointIndex)
	{
		FTransform& CurrentPoint = OutTransforms[PointIndex];
		FTransform& ChildPoint = OutTransforms[PointIndex + 1];

		// Move the parent to maintain starting bone lengths
		DragPoint(ChildPoint, BoneLengths[PointIndex + 1], CurrentPoint);
	}
}
	
void FRangeLimitedFABRIK::FABRIKBackwardPass(
	const TArray<FTransform>& InTransforms,
	const TArray<float>& BoneLengths,
	TArray<FTransform>& OutTransforms
	)
{
	int32 NumPoints     = InTransforms.Num();
	int32 EffectorIndex = NumPoints - 1;

	for (int32 PointIndex = 1; PointIndex < EffectorIndex; PointIndex++)
	{
		FTransform& ParentPoint  = OutTransforms[PointIndex - 1];
		FTransform& CurrentPoint = OutTransforms[PointIndex];
		
		// Move the child to maintain starting bone lengths
		DragPoint(ParentPoint, BoneLengths[PointIndex], CurrentPoint);
	}
}

FORCEINLINE void FRangeLimitedFABRIK::DragPoint(
	const FTransform& MaintainDistancePoint,
	float BoneLength,
	FTransform& PointToMove
) 
{
	PointToMove.SetLocation(MaintainDistancePoint.GetLocation() +
		(PointToMove.GetLocation() - MaintainDistancePoint.GetLocation()).GetUnsafeNormal() *
		BoneLength);
}

void FRangeLimitedFABRIK::DragPointTethered(
	const FTransform& StartingTransform,
	const FTransform& MaintainDistancePoint,
	float BoneLength,
	float MaxDragDistance,
	float DragStiffness,
	FTransform& PointToDrag
)
{
	if (MaxDragDistance < KINDA_SMALL_NUMBER || DragStiffness < KINDA_SMALL_NUMBER)
	{
		PointToDrag = StartingTransform;
		return;
	}  
		
	FVector Target;
	if (FMath::IsNearlyZero(BoneLength))
	{
		Target = MaintainDistancePoint.GetLocation();
	}
	else
	{
		Target = MaintainDistancePoint.GetLocation() +
			(PointToDrag.GetLocation() - MaintainDistancePoint.GetLocation()).GetUnsafeNormal() *
			BoneLength;
	}
		
	FVector Displacement = Target - StartingTransform.GetLocation();

	// Root drag stiffness 'pulls' the root back (set to 1.0 to disable)
	Displacement /= DragStiffness;	
	
	// limit root displacement to drag length
	FVector LimitedDisplacement = Displacement.GetClampedToMaxSize(MaxDragDistance);
	PointToDrag.SetLocation(StartingTransform.GetLocation() + LimitedDisplacement);
}

void FRangeLimitedFABRIK::UpdateParentRotation(
	FTransform& NewParentTransform, 
	const FTransform& OldParentTransform,
	const FTransform& NewChildTransform,
	const FTransform& OldChildTransform)
{
	FVector OldDir = (OldChildTransform.GetLocation() - OldParentTransform.GetLocation()).GetUnsafeNormal();
	FVector NewDir = (NewChildTransform.GetLocation() - NewParentTransform.GetLocation()).GetUnsafeNormal();
	
	FVector RotationAxis = FVector::CrossProduct(OldDir, NewDir).GetSafeNormal();
	float RotationAngle  = FMath::Acos(FVector::DotProduct(OldDir, NewDir));
	FQuat DeltaRotation  = FQuat(RotationAxis, RotationAngle);
	
	NewParentTransform.SetRotation(DeltaRotation * OldParentTransform.GetRotation());
	NewParentTransform.NormalizeRotation();
}

float FRangeLimitedFABRIK::ComputeBoneLengths(
	const TArray<FTransform>& InTransforms,
	TArray<float>& OutBoneLengths
)
{
	int32 NumPoints = InTransforms.Num();
	float MaximumReach = 0.0f;
	OutBoneLengths.Empty();
	OutBoneLengths.Reserve(NumPoints);

	// Root always has zero length
	OutBoneLengths.Add(0.0f);

	for (int32 i = 1; i < NumPoints; ++i)
	{
		OutBoneLengths.Add(FVector::Dist(InTransforms[i - 1].GetLocation(),
			InTransforms[i].GetLocation()));
		MaximumReach  += OutBoneLengths[i];
	}
	
	return MaximumReach;
}
