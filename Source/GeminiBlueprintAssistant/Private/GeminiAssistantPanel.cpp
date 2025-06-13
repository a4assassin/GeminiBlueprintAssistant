// Private/GeminiAssistantPanel.cpp
#include "GeminiAssistantPanel.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Misc/ConfigCacheIni.h" // For reading/writing INI files
#include "HAL/FileManager.h" // For file operations
#include "Misc/FileHelper.h" // For reading file contents

#define LOCTEXT_NAMESPACE "FGeminiBlueprintAssistantModule"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void GeminiAssistantPanel::Construct(const FArguments& InArgs)
{
	// Initialize GeminiClient
	GeminiClient = MakeShared<FGeminiAPIClient>();
	// Bind the response delegate
	GeminiClient->OnGeminiResponseReceived.BindRaw(this, &GeminiAssistantPanel::OnGeminiResponse);

	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(10.0f))
				[
					SNew(SVerticalBox)

						// Prompt Input Section
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0, 5, 0, 10))
						[
							SNew(STextBlock)
								.Text(LOCTEXT("PromptLabel", "Enter your prompt for Gemini:"))
								.Font(FEditorStyle::GetFontStyle("BoldFont"))
						]
						+ SVerticalBox::Slot()
						.MaxHeight(200) // Limit height of input box
						.Padding(FMargin(0, 0, 0, 10))
						[
							SAssignNew(PromptTextBox, SMultiLineEditableTextBox)
								.HintText(LOCTEXT("PromptHint", "e.g., Summarize the selected nodes and add comments."))
								.OnTextChanged(this, &GeminiAssistantPanel::OnPromptTextChanged)
								.OnTextCommitted(this, &GeminiAssistantPanel::OnPromptTextCommitted)
								.AutoWrapText(true)
						]

						// Process Button
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						.Padding(FMargin(0, 0, 0, 10))
						[
							SNew(SButton)
								.Text(LOCTEXT("ProcessButtonText", "Process with Gemini"))
								.OnClicked(this, &GeminiAssistantPanel::OnProcessButtonClicked)
								.IsEnabled_Lambda([this]() { return !CurrentPromptText.IsEmpty(); }) // Disable button if prompt is empty
						]

						// Response Display Section
						+ SVerticalBox::Slot()
						.FillHeight(1.0f) // Take remaining space
						.Padding(FMargin(0, 10, 0, 0))
						[
							SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(FMargin(10.0f))
								[
									SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(FMargin(0, 0, 0, 5))
										[
											SNew(STextBlock)
												.Text(LOCTEXT("ResponseLabel", "Gemini Response:"))
												.Font(FEditorStyle::GetFontStyle("BoldFont"))
										]
										+ SVerticalBox::Slot()
										.FillHeight(1.0f)
										[
											SAssignNew(ResponseTextBlock, STextBlock)
												.AutoWrapText(true)
												.Text(LOCTEXT("InitialResponseText", "Response will appear here..."))
										]
								]
						]
				]
		];

	// Initialize with empty text
	CurrentPromptText = FText::GetEmpty();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply GeminiAssistantPanel::OnProcessButtonClicked()
{
	if (CurrentPromptText.IsEmpty())
	{
		ResponseTextBlock->SetText(LOCTEXT("EmptyPromptWarning", "Please enter a prompt."));
		return FReply::Handled();
	}

	ResponseTextBlock->SetText(LOCTEXT("ProcessingRequest", "Sending request to Gemini..."));

	UE_LOG(LogTemp, Log, TEXT("Gemini Blueprint Assistant: Process button clicked. Prompt: %s"), *CurrentPromptText.ToString());

	// --- IMPORTANT: API Key Management ---
	// For testing, you can temporarily hardcode your API key here:
	// FString APIKey = TEXT("YOUR_GEMINI_API_KEY_HERE");
	// BUT FOR ANY SERIOUS USE, read from a config file.

	// Example of reading from a local config file (e.g., Saved/Config/EditorPerProjectUserSettings.ini)
	FString APIKey;
	if (!GConfig->GetString(TEXT("GeminiAssistant"), TEXT("APIKey"), APIKey, GEditorPerProjectIni))
	{
		// If key not found, prompt user to set it
		ResponseTextBlock->SetText(LOCTEXT("APIKeyMissing", "Gemini API Key not found in config! Please add it to [GeminiAssistant] section in EditorPerProjectUserSettings.ini (e.g., [GeminiAssistant]\nAPIKey=YOUR_KEY_HERE) or hardcode for testing."));
		UE_LOG(LogTemp, Error, TEXT("GeminiAPIClient: API Key not found in config."));
		return FReply::Handled();
	}

	// Trigger the Gemini API call
	if (GeminiClient.IsValid())
	{
		GeminiClient->GenerateContent(CurrentPromptText.ToString(), APIKey);
	}
	else
	{
		ResponseTextBlock->SetText(LOCTEXT("ClientError", "Gemini API Client is not initialized."));
		UE_LOG(LogTemp, Error, TEXT("GeminiAPIClient: Client not valid!"));
	}

	return FReply::Handled();
}

void GeminiAssistantPanel::OnPromptTextChanged(const FText& InText)
{
	CurrentPromptText = InText;
}

void GeminiAssistantPanel::OnPromptTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		OnProcessButtonClicked();
	}
}

void GeminiAssistantPanel::OnGeminiResponse(FString ResponseContent, bool bSuccess, FString ErrorMessage)
{
	if (bSuccess)
	{
		ResponseTextBlock->SetText(FText::FromString(ResponseContent));
		UE_LOG(LogTemp, Log, TEXT("Gemini API Response: %s"), *ResponseContent);
	}
	else
	{
		ResponseTextBlock->SetText(FText::Format(LOCTEXT("GeminiError", "Gemini API Error: {0}"), FText::FromString(ErrorMessage)));
		UE_LOG(LogTemp, Error, TEXT("Gemini API Error: %s"), *ErrorMessage);
	}
}

#undef LOCTEXT_NAMESPACE