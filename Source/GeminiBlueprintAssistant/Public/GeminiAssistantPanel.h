// Public/GeminiAssistantPanel.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "GeminiAPIClient.h" // Your Gemini API client header

// Forward declarations for necessary Unreal Engine classes/interfaces
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class IBlueprintEditor; // Interface for Blueprint editor instance (exposed via FBlueprintEditorModule.h)
class SGraphEditor;     // The actual graph editor widget (contains selection/view methods)

struct LLMResponseParts {
	FString Details;
	FString Summary;
};
/**
 * Implements the main Gemini Blueprint Assistant panel.
 */
class GEMINIBLUEPRINTASSISTANT_API GeminiAssistantPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(GeminiAssistantPanel) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	// --- UI Callbacks ---
	FReply OnProcessButtonClicked();
	void OnPromptTextChanged(const FText& InText);
	void OnPromptTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	void OnGeminiResponse(FString ResponseContent, bool bSuccess, FString ErrorMessage);

	// --- Blueprint Interaction Functions ---
	UBlueprint* GetActiveBlueprint() const;
	TArray<UEdGraphNode*> GetSelectedBlueprintNodes(UBlueprint* InBlueprint) const;
	TArray<UEdGraphNode*> GetAllNodesFromActiveGraph(UBlueprint* InBlueprint) const;
	FString ExtractNodeDataForGemini(const TArray<UEdGraphNode*>& InNodes) const;
	void AddCommentNodeToBlueprint(UBlueprint* InBlueprint, UEdGraph* TargetGraph, const FString& CommentText) const;
	LLMResponseParts ParseLLMResponse(const FString& FullResponse);

	// --- UI Members ---
	TSharedPtr<SMultiLineEditableTextBox> PromptTextBox;
	TSharedPtr<STextBlock> ResponseTextBlock;
	TSharedPtr<SCheckBox> WriteCommentsCheckBox;
	FText CurrentPromptText;
	LLMResponseParts Results;

	// --- API Client Member ---
	TSharedPtr<FGeminiAPIClient> GeminiClient;
};