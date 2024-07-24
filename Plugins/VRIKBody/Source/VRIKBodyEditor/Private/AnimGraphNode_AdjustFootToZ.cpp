// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#include "AnimGraphNode_AdjustFootToZ.h"
#include "VRIKBodyEditorPrivatePCH.h"
#include "CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_AdjustFootToZ::UAnimGraphNode_AdjustFootToZ(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FAnimNode_SkeletalControlBase* UAnimGraphNode_AdjustFootToZ::GetNode() const
{
	return &Node;
}

FText UAnimGraphNode_AdjustFootToZ::GetControllerDescription() const
{
	return FText::FromString(TEXT("Adjust Foot to Z"));
}

FText UAnimGraphNode_AdjustFootToZ::GetTooltipText() const
{
	return FText::FromString(TEXT("The Two Bone IK control applies an inverse kinematic (IK) solver to a leg 3-joint chain to adjust foot z to desired ground level."));
}

FText UAnimGraphNode_AdjustFootToZ::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString Result = GetControllerDescription().ToString();
	if (TitleType == ENodeTitleType::FullTitle) {
		Result += TEXT("\nBody IK Solver");
	}
	return FText::FromString(Result);
}

//Title Color!
FLinearColor UAnimGraphNode_AdjustFootToZ::GetNodeTitleColor() const
{
	return FLinearColor::Blue;
}

//Node Category
FString UAnimGraphNode_AdjustFootToZ::GetNodeCategory() const
{
	return FString("VR IK Body");
}

#undef LOCTEXT_NAMESPACE