// Copyright Yuri N K, 2018. All Rights Reserved.
// Support: ykasczc@gmail.com

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 4
	#include "Editor/AnimGraph/Public/AnimGraphNode_Base.h"
#else
	#include "Editor/AnimGraph/Classes/AnimGraphNode_Base.h"
#endif
#include "AnimNode_ApplyLegsBlendSpace.h"
#include "AnimGraphNode_ApplyLegsBlendSpace.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI)
class UAnimGraphNode_ApplyLegsBlendSpace : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_ApplyLegsBlendSpace Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	virtual FString GetNodeCategory() const override;
	//~ End UAnimGraphNode_Base Interface
};
