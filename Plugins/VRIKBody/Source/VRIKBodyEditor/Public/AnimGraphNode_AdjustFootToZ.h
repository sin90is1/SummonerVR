// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_AdjustFootToZ.h"
#include "AnimGraphNode_AdjustFootToZ.generated.h"

/* The Two Bone IK control applies an inverse kinematic (IK) solver to a leg 3-joint chain to adjust foot z to desired ground level */
UCLASS(MinimalAPI)
class UAnimGraphNode_AdjustFootToZ : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()
	
	// Controlled Aniation node
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_AdjustFootToZ Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	// End of UAnimGraphNode_Base interface

	// UAnimGraphNode_SkeletalControlBase interface
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override;
	// End of UAnimGraphNode_SkeletalControlBase interface

protected:
	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;
	// End of UAnimGraphNode_SkeletalControlBase interface
};
