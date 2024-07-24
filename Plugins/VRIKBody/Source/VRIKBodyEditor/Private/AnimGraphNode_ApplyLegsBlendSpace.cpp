// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#include "AnimGraphNode_ApplyLegsBlendSpace.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_ApplyLegsBlendSpace"

UAnimGraphNode_ApplyLegsBlendSpace::UAnimGraphNode_ApplyLegsBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_ApplyLegsBlendSpace::GetNodeTitleColor() const
{
	return FLinearColor(FColor::Orange);
}

FText UAnimGraphNode_ApplyLegsBlendSpace::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_ApplyLegsBlendSpace_Tooltip", "Correctly blends legs animaiton and keeps upper body transform in world space");
}

FText UAnimGraphNode_ApplyLegsBlendSpace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_ApplyLegsBlendSpace", "Blend Legs To VR Body");
}

FString UAnimGraphNode_ApplyLegsBlendSpace::GetNodeCategory() const
{
	return TEXT("Blend");
}

#undef LOCTEXT_NAMESPACE