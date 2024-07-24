// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "AnimGraphNode_VRIKFingersSolver.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_VRIKFingersSolver"

UAnimGraphNode_VRIKFingersSolver::UAnimGraphNode_VRIKFingersSolver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_VRIKFingersSolver::GetNodeTitleColor() const
{
	return FLinearColor(FColor::Orange);
}

FText UAnimGraphNode_VRIKFingersSolver::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_FingersGrip_Tooltip", "Applies Fingers Solver to animation frame");
}

FText UAnimGraphNode_VRIKFingersSolver::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_FingersGrip", "Fingers Solver");
}

FString UAnimGraphNode_VRIKFingersSolver::GetNodeCategory() const
{
	return TEXT("IK");
}

#undef LOCTEXT_NAMESPACE