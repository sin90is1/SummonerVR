// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#include "VRIKBodyMath.h"
#include "VRIKBodyPrivatePCH.h"
#include "Kismet/KismetMathLibrary.h"

// ------------------------------------------------------------------------------------------------------------------------------

inline float AngleDelta(float Angle1, float Angle2);

// ------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------

// correlating three (x-y-z or yaw-pitch-roll) sets
void VRIKBodyMath::IKB_CorrelateArraysExt(const float a[][CORR_POINTS_NUM], const float b[][CORR_POINTS_NUM], const int32 Num, float &r1, float &r2, float &r3)
{
	float x_[3], y_[3], x2_[3], y2_[3], xy_[3], result[3];

	for (int n = 0; n < 3; n++) {
		x_[n] = 0.0f; y_[n] = 0.0f; x2_[n] = 0.0f; y2_[n] = 0.0f; xy_[n] = 0.0f;
	}

	for (int i = 0; i < Num; i++) {
		for (int n = 0; n < 3; n++) {
			// simple sum
			x_[n] += a[n][i]; y_[n] += b[n][i];
			// squares sum
			x2_[n] += (a[n][i] * a[n][i]); y2_[n] += (b[n][i] * b[n][i]);
			// products sum
			xy_[n] += (a[n][i] * b[n][i]);
		}
	}

	// get expected value
	float divider = (float)Num;
	for (int n = 0; n < 3; n++) {
		x2_[n] /= divider;
		y2_[n] /= divider;
		x_[n] /= divider;
		y_[n] /= divider;
		xy_[n] /= divider;
	}

	// calculating r
	float sigma_x2, sigma_y2;
	for (int n = 0; n < 3; n++) {
		sigma_x2 = x2_[n] - x_[n] * x_[n];
		sigma_y2 = y2_[n] - y_[n] * y_[n];
		result[n] = (xy_[n] - x_[n] * y_[n]) / (FMath::Sqrt(sigma_x2 * sigma_y2));
	}

	// result
	r1 = result[0];
	r2 = result[1];
	r3 = result[2];
}

// ------------------------------------------------------------------------------------------------------------------------------

// correlating two sets
void VRIKBodyMath::IKB_CorrelateFloatsExt(const float* a, const float* b, const int32 Num, float &r)
{
	float x_ = 0.0f, y_ = 0.0f, x2_ = 0.0f, y2_ = 0.0f, xy_ = 0.0f, result;

	for (int i = 0; i < Num; i++) {
		// simple sum
		x_ += a[i];
		y_ += b[i];
		// squares sum
		x2_ += (a[i] * a[i]);
		y2_ += (b[i] * b[i]);
		// products sum
		xy_ += (a[i] * b[i]);
	}

	// get expected value
	float divider = (float)Num;
	x2_ /= divider;
	y2_ /= divider;
	x_ /= divider;
	y_ /= divider;
	xy_ /= divider;

	// result
	float sigma_x2, sigma_y2;
	sigma_x2 = x2_ - x_ * x_;
	sigma_y2 = y2_ - y_ * y_;
	result = (xy_ - x_ * y_) / (FMath::Sqrt(sigma_x2 * sigma_y2));

	r = result;
}

// ------------------------------------------------------------------------------------------------------------------------------

// Not in use. For a future automatic head-to-neck calibration. Calculates rotation centre for four points
FVector VRIKBodyMath::IKB_CalcPointRotationCentreExt(const float* locdata[CORR_POINTS_NUM])
{
	FVector lin1, LinDir1, lin2, LinDir2;
	FVector v1, v2;

	// 1. Line/normal equations for two triangles
	int index = 0;

	lin1 = FVector(locdata[0][index + 1], locdata[1][index + 1], locdata[2][index + 1]);
	v1 = lin1 - FVector(locdata[0][index + 0], locdata[1][index + 0], locdata[2][index + 0]);
	v2 = lin1 - FVector(locdata[0][index + 2], locdata[1][index + 2], locdata[2][index + 2]);
	LinDir1 = v1 * v2;
	LinDir1.Normalize();

	index = 1;

	lin2 = FVector(locdata[0][index + 1], locdata[1][index + 1], locdata[2][index + 1]);
	v1 = lin2 - FVector(locdata[0][index + 0], locdata[1][index + 0], locdata[2][index + 0]);
	v2 = lin2 - FVector(locdata[0][index + 2], locdata[1][index + 2], locdata[2][index + 2]);
	LinDir2 = v1 * v2;
	LinDir2.Normalize();

	// 2. Plane W (owns straight line 2 parallel to straight 1).
	FVector WNorm = LinDir1 * LinDir2;
	WNorm.Normalize();

	// 3. Plane W2 crossing straight 2 and perpendicular plane W
	FVector W2Norm = WNorm * LinDir2;

	// straight line 1 = lin1 + LinDir1 * lambda
	float p1, p2, p3;
	p1 = W2Norm.X * lin2.X + W2Norm.Y * lin2.Y + W2Norm.Z * lin2.Z;
	p2 = W2Norm.X * lin1.X + W2Norm.Y * lin1.Y + W2Norm.Z * lin1.Z;
	p3 = W2Norm.X * LinDir1.X + W2Norm.Y * LinDir1.Y + W2Norm.Z * LinDir1.Z;
	float lambda = (p1 - p2) / p3;

	return (lin1 + lambda * LinDir1);
}

// ------------------------------------------------------------------------------------------------------------------------------

// Not in use: for a future advanced shoulders adjustment.
float VRIKBodyMath::IKB_AdjustRibcageYawForShoulder(FVector HandLoc, FVector RibcageLoc, FRotator PlevisRot, const float HandLength, const float ShoulderRadius)
{
	// get Ribcage plane
	// get distance from hand to plane
	// sphere [radius = hand length] projection on a plane
	// circles crossing on the plane

	// up vector of the ribcage rotation plane
	const FVector PlaneUpVector = PlevisRot.Quaternion().GetUpVector();
	// dist from plane to the hand location
	float DistanceToHand = FVector::PointPlaneDist(HandLoc, RibcageLoc, PlaneUpVector);
	// projection of the hand location to the ribcage plane
	FVector HandCircleCentre = FVector::PointPlaneProject(HandLoc, RibcageLoc, PlaneUpVector);

	// i.e. always
	if (DistanceToHand < HandLength) {
		// projected length of the hand
		float HandCircleRadius = FMath::Sqrt(HandLength * HandLength - DistanceToHand * DistanceToHand);
		HandCircleCentre -= RibcageLoc;

		float DistToHandOnPlane = FVector::Dist(RibcageLoc, HandCircleCentre);
		float alpha = FMath::RadiansToDegrees(FMath::Acos((DistToHandOnPlane*DistToHandOnPlane + ShoulderRadius*ShoulderRadius - HandCircleRadius*HandCircleRadius) / (2 * DistToHandOnPlane * ShoulderRadius)));

		return alpha * (1.f - DistanceToHand / HandLength);
	}
	return 0.f;
}

// Euler -180..180 angles delta 
inline float AngleDelta(float Angle1, float Angle2)
{
	if (Angle1 - Angle2 > 180.f) {
		Angle1 -= 360.f;
	}
	else if (Angle2 - Angle1 > 180.f) {
		Angle2 -= 360.f;
	}
	return FMath::Abs(Angle2 - Angle2);
}

// Math Dispersion
void VRIKBodyMath::IKB_CalcDispersionSquared(const float* a, const int32 Num, float &s2)
{
	float a_ = 0.f;
	for (int i = 0; i < Num; i++) {
		a_ += a[i];
	}
	a_ /= (float)Num;

	s2 = 0.f;
	for (int i = 0; i < Num; i++) {
		s2 += (a[i] - a_) * (a[i] - a_);
	}

	s2 /= (float)(Num - 1);
}

// Math Expectation
void VRIKBodyMath::IKB_Average(const float* a, const int32 Num, float &a_)
{
	a_ = 0.f;
	for (int i = 0; i < Num; i++) {
		a_ += a[i];
	}
	a_ /= (float)Num;
}

// Helper function for relative rotation 
void VRIKBodyMath::IKB_AddRelativeRotation(FRotator& RotatorToModify, const FRotator& AddRotator)
{
	const FQuat CurRelRotQuat = RotatorToModify.Quaternion();
	const FQuat NewRelRotQuat = CurRelRotQuat * AddRotator.Quaternion();

	RotatorToModify = NewRelRotQuat.Rotator();
}