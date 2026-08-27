#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldInteractionPromptWidget.generated.h"

UCLASS(Abstract)
class CASINO_SIMULATOR_API UWorldInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "World Interaction", meta = (DisplayName = "Set Prompt Text"))
	void BP_SetPromptText(const FText& PromptText);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Interaction", meta = (DisplayName = "Set Primary Prompt Visible"))
	void BP_SetPrimaryPromptVisible(bool bVisible);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Interaction", meta = (DisplayName = "Set Exit Prompt Visible"))
	void BP_SetExitPromptVisible(bool bVisible);
};
