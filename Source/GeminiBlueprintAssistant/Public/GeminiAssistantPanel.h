// Public/GeminiAssistantPanel.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "GeminiAPIClient.h" // Include our API client header

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
	/** Callback for when the "Process" button is clicked. */
	FReply OnProcessButtonClicked();

	/** Callback for when the text in the prompt box changes. */
	void OnPromptTextChanged(const FText& InText);

	/** Callback for when the text in the prompt box is committed (e.g., on Enter key). */
	void OnPromptTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Callback for when the Gemini API returns a response. */
	void OnGeminiResponse(FString ResponseContent, bool bSuccess, FString ErrorMessage);

	/** Holds the current prompt text. */
	TSharedPtr<SMultiLineEditableTextBox> PromptTextBox;

	/** Holds the response text. */
	TSharedPtr<STextBlock> ResponseTextBlock;

	/** The current prompt text entered by the user. */
	FText CurrentPromptText;

	/** Instance of our Gemini API Client. */
	TSharedPtr<FGeminiAPIClient> GeminiClient; // Add this line
};