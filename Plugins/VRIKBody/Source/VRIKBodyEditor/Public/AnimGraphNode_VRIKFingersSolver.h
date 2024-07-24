// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode_VRIKFingersSolver.h"
#include "AnimGraphNode_VRIKFingersSolver.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI)
class UAnimGraphNode_VRIKFingersSolver : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_VRIKFingersSolver Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	virtual FString GetNodeCategory() const override;
	//~ End UAnimGraphNode_Base Interface
};