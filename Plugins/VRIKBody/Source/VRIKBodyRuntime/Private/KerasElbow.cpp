// Fill out your copyright notice in the Description page of Project Settings.

#include "KerasElbow.h"
#include "Misc/Paths.h"
#include "YnnkVRIKTypes.h"
#include "keras2cpp/model.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Launch/Resources/Version.h"

using keras2cpp::Model;
using keras2cpp::Tensor;

#ifndef __ueversion_compatibility
	#if ENGINE_MAJOR_VERSION < 5
		#ifndef FVector3f
			#define FVector3f FVector
		#endif
		#define FRotator3f FRotator
		#define FTransform3f FTransform
		#define FRotationMatrix44f FRotationMatrix
	#endif
#endif

// Sets default values for this component's properties
UKerasElbow::UKerasElbow()
	: bModelLoaded(false)
	, RequestTimeAccumulated(0.)
	, LastTimeInMs(0.)
	, FramesNumAccumulated(0)
{
	KerasModel = nullptr;
}

UKerasElbow* UKerasElbow::CreateKerasElbowModel(FString ModelFileName, bool bCollectDebugInfo, int32 SmoothNum)
{
	UKerasElbow* NewModel = NewObject<UKerasElbow>(UKerasElbow::StaticClass());
	if (NewModel)
	{
		NewModel->bDebugInfo = bCollectDebugInfo;
		NewModel->Initialize(ModelFileName, SmoothNum);
	}
	return NewModel;
}

void UKerasElbow::Evaluate(const FTransform& RibcageRaw, const FTransform& HandR, const FTransform& HandL, FVector& JointTargetRight, FVector& JointTargetLeft)
{
	if (KerasModel)
	{
		const FTransform3f gRibcage = (FTransform3f)RibcageRaw;
		const FTransform3f gHandR = (FTransform3f)HandR;
		const FTransform3f gHandL = (FTransform3f)HandL;
		FTransform3f RightRelative = gHandR.GetRelativeTransform(gRibcage);
		FTransform3f LeftRelative = gHandL.GetRelativeTransform(gRibcage);

		float ApplyValR_Roll, ApplyValR_Pitch, ApplyValR_Yaw;
		float ApplyValL_Roll, ApplyValL_Pitch, ApplyValL_Yaw;
		YnnkHelpers::GetRotationDeltaInEulerCoordinates(HandR.GetRotation(), RibcageRaw.GetRotation(), ApplyValR_Roll, ApplyValR_Pitch, ApplyValR_Yaw);
		YnnkHelpers::GetRotationDeltaInEulerCoordinates(HandL.GetRotation(), RibcageRaw.GetRotation(), ApplyValL_Roll, ApplyValL_Pitch, ApplyValL_Yaw);

		bool bUpdatePrevVal = true;

		if (FMath::Abs(ApplyValR_Roll - PrevHandR.Roll) > 310.f)
		{
			ApplyValR_Roll += (ApplyValR_Roll < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}
		if (FMath::Abs(ApplyValR_Pitch - PrevHandR.Pitch) > 310.f)
		{
			ApplyValR_Pitch += (ApplyValR_Pitch < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}
		if (FMath::Abs(ApplyValR_Yaw - PrevHandR.Yaw) > 310.f)
		{
			ApplyValR_Yaw += (ApplyValR_Yaw < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}

		if (bUpdatePrevVal)
		{
			// we don't want to normalize
			PrevHandR.Roll = ApplyValR_Roll; PrevHandR.Pitch = ApplyValR_Pitch; PrevHandR.Yaw = ApplyValR_Yaw;
		}

		bUpdatePrevVal = true;

		if (FMath::Abs(ApplyValL_Roll - PrevHandL.Roll) > 310.f)
		{
			ApplyValL_Roll += (ApplyValL_Roll < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}
		if (FMath::Abs(ApplyValL_Pitch - PrevHandL.Pitch) > 310.f)
		{
			ApplyValL_Pitch += (ApplyValL_Pitch < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}
		if (FMath::Abs(ApplyValL_Yaw - PrevHandL.Yaw) > 310.f)
		{
			ApplyValL_Yaw += (ApplyValL_Yaw < 0.f ? 360.f : -360.f);
			bUpdatePrevVal = false;
		}

		if (bUpdatePrevVal)
		{
			// we don't want to normalize
			PrevHandL.Roll = ApplyValL_Roll; PrevHandL.Pitch = ApplyValL_Pitch; PrevHandL.Yaw = ApplyValL_Yaw;
		}

#if ENGINE_MAJOR_VERSION > 4
		Tensor InputDataR(18);
		Tensor InputDataL(18);
#else
		Tensor InputDataR({ (size_t)18 });
		Tensor InputDataL({ (size_t)18 });
#endif

		const float RotVecMul = 10.f;
		const float RotVecMul2 = RotVecMul / 5.f;
		const FVector3f v1 = RightRelative.GetTranslation();
		const FVector3f rf1 = RightRelative.GetRotation().GetForwardVector() * RotVecMul;
		const FVector3f rr1 = RightRelative.GetRotation().GetRightVector() * RotVecMul;

		const FVector3f v2 = LeftRelative.GetTranslation();
		const FVector3f rf2 = LeftRelative.GetRotation().GetForwardVector() * RotVecMul;
		const FVector3f rr2 = LeftRelative.GetRotation().GetRightVector() * RotVecMul;

		double t_before = FPlatformTime::Seconds();

		bool bResult;

		InputDataR.data_ = { v1.X, v1.Y, v1.Z, rf1.X, rf1.Y, rf1.Z, rr1.X, rr1.Y, rr1.Z,
								50.f, -15.f, -8.f, 4.96f * RotVecMul2, 0.63f * RotVecMul2, -0.1f * RotVecMul2, -0.01f * RotVecMul2, 0.82f * RotVecMul2, 4.93f * RotVecMul2 };

		InputDataL.data_ = { 50.f, 15.f, -8.f, 4.78f * RotVecMul2, -1.45f * RotVecMul2, 0.31f * RotVecMul2, 0.43f * RotVecMul2, 0.38f * RotVecMul2, -4.97f * RotVecMul2,
								v2.X, v2.Y, v2.Z, rf2.X, rf2.Y, rf2.Z, rr2.X, rr2.Y, rr2.Z };


		Tensor OutDataR = (*KerasModel)(InputDataR);
		Tensor OutDataL = (*KerasModel)(InputDataL);

		bResult = (OutDataR.dims_.size() == 1 && OutDataR.dims_[0] == 6 && OutDataL.dims_.size() == 1 && OutDataL.dims_[0] == 6);
		if (bResult)
		{
			JointTargetLeft = FVector(OutDataL.data_[0], OutDataL.data_[1], OutDataL.data_[2]);
			JointTargetRight = FVector(OutDataR.data_[3], OutDataR.data_[4], OutDataR.data_[5]);
		}

		if (bDebugInfo)
		{
			double t_after = FPlatformTime::Seconds();

			LastTimeInMs = (t_after - t_before) * 1000.;
			RequestTimeAccumulated += LastTimeInMs;
			FramesNumAccumulated++;
		}

		if (bResult)
		{
			JointTargetLeft.Z -= 17.f;
			JointTargetRight.Z -= 17.f;

			// Apply smoothing
			if (SmoothFrames > 1)
			{
				HistoryRight[SaveIndex] = JointTargetRight;
				HistoryLeft[SaveIndex] = JointTargetLeft;
				SaveIndex = (SaveIndex + 1) % SmoothFrames;

				JointTargetRight = JointTargetLeft = FVector::ZeroVector;
				for (int32 Index = 0; Index < SmoothFrames; Index++)
				{
					JointTargetRight += HistoryRight[Index];
					JointTargetLeft += HistoryLeft[Index];
				}

				JointTargetRight /= SmoothFrames;
				JointTargetLeft /= SmoothFrames;
			}

			JointTargetRight = (FTransform(JointTargetRight) * RibcageRaw).GetTranslation();
			JointTargetLeft = (FTransform(JointTargetLeft) * RibcageRaw).GetTranslation();
		}
		else
		{
			JointTargetRight = JointTargetLeft = FVector::ZeroVector;
		}
	}
}

float UKerasElbow::GetRequestTimeMean() const
{
	return (FramesNumAccumulated > 0)
		? (float)(RequestTimeAccumulated / (double)FramesNumAccumulated)
		: 0.f;
}

float UKerasElbow::GetLastRequestTime() const
{
	return LastTimeInMs;
}

void UKerasElbow::Initialize(FString FileName, int32 SmoothNum)
{
	FString UseFileName = FPaths::FileExists(FileName)
		? FileName
		: FPaths::ProjectDir() / FileName;

	if (FPaths::FileExists(UseFileName))
	{
		std::string szFileName = TCHAR_TO_ANSI(*UseFileName);
		KerasModel = new keras2cpp::Model(Model::load(szFileName));

		SmoothFrames = SmoothNum;
		HistoryRight.Init(FVector::RightVector, SmoothFrames);
		HistoryLeft.Init(FVector::LeftVector, SmoothFrames);
		SaveIndex = 0;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Can't find elbows keras model file (%s)"), *FileName);
		KerasModel = nullptr;
	}
}

void UKerasElbow::BeginDestroy()
{
	Super::BeginDestroy();

	if (KerasModel != nullptr)
	{
		delete KerasModel;
		KerasModel = nullptr;
	}
}