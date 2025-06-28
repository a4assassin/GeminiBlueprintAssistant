// Private/GeminiAssistantPanel.cpp
#include "GeminiAssistantPanel.h"
#include "SlateOptMacros.h"

// UI related includes (Direct Slate widget headers)
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h" 
#include "Widgets/Input/SButton.h" 
#include "EditorStyleSet.h"

// Core includes for config/file operations
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

// Blueprint Core Classes (These exist as direct headers)
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphSchema.h"

#include "K2Node.h"

#include "BlueprintEditorModule.h"        // This header defines FBlueprintEditorModule AND the IBlueprintEditor interface.

#include "Editor.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditor.h"
#include "SGraphPanel.h"

#include "ScopedTransaction.h"

#include "BlueprintNodePreprocessor.h"

#define LOCTEXT_NAMESPACE "FGeminiBlueprintAssistantModule"


// --- CONSTRUCTOR AND BASIC UI CALLBACKS (Keeping as they were) ---
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void GeminiAssistantPanel::Construct(const FArguments& InArgs)
{
	GeminiClient = MakeShared<FGeminiAPIClient>();
	GeminiClient->OnGeminiResponseReceived.BindRaw(this, &GeminiAssistantPanel::OnGeminiResponse);

	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(10.0f))
				[
					SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0, 5, 0, 10))
						[
							SNew(STextBlock)
								.Text(LOCTEXT("PromptLabel", "Enter your prompt for Gemini:"))
								.Font(FAppStyle::GetFontStyle("BoldFont"))
						]
						+ SVerticalBox::Slot()
						.MaxHeight(200)
						.Padding(FMargin(0, 0, 0, 10))
						[
							SAssignNew(PromptTextBox, SMultiLineEditableTextBox)
								.HintText(LOCTEXT("PromptHint", "e.g., Summarize the selected nodes and add comments."))
								.OnTextChanged(this, &GeminiAssistantPanel::OnPromptTextChanged)
								.OnTextCommitted(this, &GeminiAssistantPanel::OnPromptTextCommitted)
								.AutoWrapText(true)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						.Padding(FMargin(0, 0, 0, 10))
						[
							SNew(SButton)
								.Text(LOCTEXT("ProcessButtonText", "Process with Gemini"))
								.OnClicked(this, &GeminiAssistantPanel::OnProcessButtonClicked)
								.IsEnabled_Lambda([this]() { return !CurrentPromptText.IsEmpty(); })
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(FMargin(0, 10, 0, 0))
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(FMargin(10.0f))
								[
									SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(FMargin(0, 0, 0, 5))
										[
											SNew(STextBlock)
												.Text(LOCTEXT("ResponseLabel", "Gemini Response:"))
												.Font(FAppStyle::GetFontStyle("BoldFont"))
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

	FString APIKey;
	if (!GConfig->GetString(TEXT("GeminiAssistant"), TEXT("APIKey"), APIKey, GEditorPerProjectIni))
	{
		ResponseTextBlock->SetText(LOCTEXT("APIKeyMissing", "Gemini API Key not found in config! Please add it to [GeminiAssistant] section in EditorPerProjectUserSettings.ini."));
		UE_LOG(LogTemp, Error, TEXT("GeminiAPIClient: API Key not found in config."));
		return FReply::Handled();
	}

	UBlueprint* ActiveBlueprint = GetActiveBlueprint();
	if (!ActiveBlueprint)
	{
		ResponseTextBlock->SetText(LOCTEXT("NoBlueprintActive", "No Blueprint editor is currently active. Please open a Blueprint."));
		UE_LOG(LogTemp, Error, TEXT("GeminiBlueprintAssistant: No active Blueprint found."));
		return FReply::Handled();
	}

	TArray<UEdGraphNode*> SelectedNodes = GetSelectedBlueprintNodes(ActiveBlueprint);
	FString NodesData = ExtractNodeDataForGemini(SelectedNodes);

	FString PromptToSend;
	if (NodesData.IsEmpty())
	{
		PromptToSend = FString::Printf(TEXT("Summarize the main purpose of the Blueprint named '%s'. Please respond in this exact format :  DETAILS: [summarise the bluprint in a user-freindly manner with important information.] \nSUMMARY: [concise one-line summary].Blueprint Graph Data : % s\nUser Query : % s"),
			*ActiveBlueprint->GetName(), *NodesData, *CurrentPromptText.ToString());
		ResponseTextBlock->SetText(LOCTEXT("SummarizingAll", "Summarizing entire Blueprint with Gemini..."));
	}
	else
	{
		PromptToSend = FString::Printf(TEXT("Given the following Unreal Engine Blueprint nodes from Blueprint '%s', summarize their collective purpose and, Please respond in this exact format :  DETAILS: [summarise the selected nodes in user-friendly manner] \nSUMMARY: [concise one-line summary]. Blueprint Graph Nodes Data: %s\nUser Query: %s"),
			*ActiveBlueprint->GetName(), *NodesData, *CurrentPromptText.ToString());
		ResponseTextBlock->SetText(LOCTEXT("SummarizingSelected", "Summarizing selected Blueprint nodes with Gemini..."));
	}
	PromptToSend += "\nRespond as if you're writing for a basic text display that cannot render formatting - use only letters, numbers, basic punctuation, and spaces.";
	if (GeminiClient.IsValid())
	{
		GeminiClient->GenerateContent(PromptToSend, APIKey);
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
		Results = ParseLLMResponse(ResponseContent);
		ResponseTextBlock->SetText(FText::FromString(Results.Details));
		UE_LOG(LogTemp, Log, TEXT("Gemini API Response: %s"), *ResponseContent);

		// FOR TESTING: Try adding a comment based on response
		UBlueprint* ActiveBlueprint = GetActiveBlueprint();
		// Ensure there's an active blueprint and at least one graph (UbergraphPages[0] is typically the Event Graph)
		if (ActiveBlueprint && ActiveBlueprint->UbergraphPages.Num() > 0)
		{
			// Add comment to the first graph (usually Event Graph) for simplicity
			AddCommentNodeToBlueprint(ActiveBlueprint, ActiveBlueprint->UbergraphPages[0], Results.Summary);
		}
	}
	else
	{
		ResponseTextBlock->SetText(FText::Format(LOCTEXT("GeminiError", "Gemini API Error: {0}"), FText::FromString(ErrorMessage)));
		UE_LOG(LogTemp, Error, TEXT("Gemini API Error: %s"), *ErrorMessage);
	}
}


// --- BLUEPRINT INTERACTION FUNCTIONS ---

UBlueprint* GeminiAssistantPanel::GetActiveBlueprint() const
{
	// GEditor is a global pointer to the editor engine. It's always valid in editor builds.
	if (GEditor)
	{
		// The UAssetEditorSubsystem manages all currently open asset editors.
		// It's exposed via GEditor and is part of the UnrealEd module.
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			// Get all currently edited assets.
			TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
			for (UObject* Asset : EditedAssets)
			{
				// Try to cast each edited asset to a UBlueprint.
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
				{
					// Found an active Blueprint.
					return Blueprint;
				}
			}
		}
	}
	return nullptr; // No active Blueprint found.
}

TArray<UEdGraphNode*> GeminiAssistantPanel::GetSelectedBlueprintNodes(UBlueprint* InBlueprint) const
{
	TArray<UEdGraphNode*> SelectedNodes;

	if (!InBlueprint)
	{
		return SelectedNodes;
	}

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

	TSharedPtr<IBlueprintEditor> FoundBlueprintEditor;
	
	for (TSharedRef<IBlueprintEditor> BlueprintEditorInstance : BlueprintEditorModule.GetBlueprintEditors())
	{
		if (FBlueprintEditorUtils::FindBlueprintForGraph(BlueprintEditorInstance->GetFocusedGraph()) == InBlueprint)
		{
			FoundBlueprintEditor = BlueprintEditorInstance;
			break; // Found the correct editor.
		}
	}

	if (FoundBlueprintEditor.IsValid())
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor = StaticCastSharedPtr<FBlueprintEditor>(FoundBlueprintEditor);

		if (BlueprintEditor.IsValid())
		{
			FGraphPanelSelectionSet FoundSelectedNodes = BlueprintEditor->GetSelectedNodes();
			for (UObject* Node : FoundSelectedNodes)
			{
				if (UEdGraphNode* CurrentNode = Cast<UEdGraphNode>(Node))
				{
					SelectedNodes.Add(CurrentNode);
				}
			}
		}
	}

	return SelectedNodes;
}

FString GeminiAssistantPanel::ExtractNodeDataForGemini(const TArray<UEdGraphNode*>& InNodes) const
{
	/*
	FString ExtractedData;
	if (InNodes.IsEmpty())
	{
		return ExtractedData;
	}

	ExtractedData += TEXT("[\n");
	for (int32 i = 0; i < InNodes.Num(); ++i)
	{
		UEdGraphNode* Node = InNodes[i];
		// Replace newlines in titles/comments to keep JSON on one line per field, for easier parsing by Gemini.
		ExtractedData += FString::Printf(TEXT("  {\n    \"NodeName\": \"%s\",\n"), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT("\\n")));
		ExtractedData += FString::Printf(TEXT("    \"NodeType\": \"%s\",\n"), *Node->GetClass()->GetName());
		ExtractedData += FString::Printf(TEXT("    \"NodeComment\": \"%s\",\n"), *Node->NodeComment.Replace(TEXT("\n"), TEXT("\\n")));
		ExtractedData += FString::Printf(TEXT("    \"NodePosition\": \"(%d, %d)\",\n"), Node->NodePosX, Node->NodePosY);

		// Extract connected pins
		ExtractedData += TEXT("    \"Pins\": [\n");
		for (int32 j = 0; j < Node->Pins.Num(); ++j)
		{
			UEdGraphPin* Pin = Node->Pins[j];
			ExtractedData += FString::Printf(TEXT("      {\n        \"PinName\": \"%s\",\n"), *Pin->PinName.ToString());
			ExtractedData += FString::Printf(TEXT("        \"Direction\": \"%s\",\n"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			ExtractedData += FString::Printf(TEXT("        \"Category\": \"%s\""), *Pin->PinType.PinCategory.ToString());

			// If the pin is linked, list its connections.
			if (Pin->LinkedTo.Num() > 0)
			{
				ExtractedData += TEXT(",\n        \"LinkedTo\": [");
				for (int32 k = 0; k < Pin->LinkedTo.Num(); ++k)
				{
					UEdGraphPin* LinkedPin = Pin->LinkedTo[k];
					// Use GetOwningNode()->GetNodeTitle for linked node name
					ExtractedData += FString::Printf(TEXT("\"%s.%s\"%s"),
						*LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT("\\n")),
						*LinkedPin->PinName.ToString(),
						(k < Pin->LinkedTo.Num() - 1 ? TEXT(", ") : TEXT(""))
					);
				}
				ExtractedData += TEXT("]\n");
			}
			else
			{
				ExtractedData += TEXT("\n");
			}
			ExtractedData += FString::Printf(TEXT("      }%s\n"), (j < Node->Pins.Num() - 1 ? TEXT(",") : TEXT("")));
		}
		ExtractedData += TEXT("    ]\n"); // End Pins Array
		ExtractedData += FString::Printf(TEXT("  }%s\n"), (i < InNodes.Num() - 1 ? TEXT(",") : TEXT(""))); // End Node Object
	}
	ExtractedData += TEXT("]\n"); // End Nodes Array
	*/
	FBlueprintNodePreprocessor NodePreprocessor;
	TArray<UK2Node*> SelectedBPNodes;
	for (UEdGraphNode* Node : InNodes)
	{
		UK2Node* tempNode = Cast<UK2Node>(Node);
		SelectedBPNodes.Add(tempNode);
	}
	FString NormalisedNodeText = NodePreprocessor.PreprocessNodes(SelectedBPNodes);
	//ResponseTextBlock->SetText(FText::FromString(NormalisedNodeText));
	
	return NormalisedNodeText;
}

void GeminiAssistantPanel::AddCommentNodeToBlueprint(UBlueprint* InBlueprint, UEdGraph* TargetGraph, const FString& CommentText) const
{
	if (!InBlueprint || !TargetGraph || CommentText.IsEmpty())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddGeminiCommentNode", "Add Gemini Generated Comment Node"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
	InBlueprint->Modify();
	TargetGraph->Modify();

	UEdGraphNode_Comment* NewCommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
	NewCommentNode->NodeComment = CommentText;

	NewCommentNode->NodeWidth = FMath::Clamp(CommentText.Len() * 10 + 50, 200, 800);
	NewCommentNode->NodeHeight = FMath::Clamp(CommentText.Len() / 30 * 20 + 80, 100, 400);

	NewCommentNode->NodePosX = 0;
	NewCommentNode->NodePosY = 0;

	NewCommentNode->ErrorType = EMessageSeverity::Info;

	// Add the new node to the graph.
	TargetGraph->AddNode(NewCommentNode);
	NewCommentNode->CreateNewGuid();
	NewCommentNode->PostPlacedNewNode();

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	TSharedPtr<IBlueprintEditor> FoundBlueprintEditor;

	// Find the specific Blueprint editor instance that is editing our InBlueprint.
	for (TSharedRef<IBlueprintEditor> BlueprintEditorInstance : BlueprintEditorModule.GetBlueprintEditors())
	{
		if (FBlueprintEditorUtils::FindBlueprintForGraph(BlueprintEditorInstance->GetFocusedGraph()) == InBlueprint)
		{
			FoundBlueprintEditor = BlueprintEditorInstance;
			break;
		}
	}

	if (FoundBlueprintEditor.IsValid())
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor = StaticCastSharedPtr<FBlueprintEditor>(FoundBlueprintEditor);

		if (BlueprintEditor.IsValid())
		{
			FGraphPanelSelectionSet FoundSelectedNodes = BlueprintEditor->GetSelectedNodes();
			if (FoundSelectedNodes.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("No nodes selected to wrap with comment"));
				return;
			}
			float MinX = FLT_MAX, MinY = FLT_MAX;
			float MaxX = -FLT_MAX, MaxY = -FLT_MAX;

			for (UObject* ANode : FoundSelectedNodes)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(ANode))
				{
					float NodeX = Node->NodePosX;
					float NodeY = Node->NodePosY;

					float NodeWidth = 200.0f;
					float NodeHeight = 100.0f;

					MinX = FMath::Min(MinX, NodeX);
					MinY = FMath::Min(MinY, NodeY);
					MaxX = FMath::Max(MaxX, NodeX + NodeWidth);
					MaxY = FMath::Max(MaxY, NodeY + NodeHeight);
				}
			}
			const float Padding = 50.0f;
			MinX -= Padding;
			MinY -= Padding;
			MaxX += Padding;
			MaxY += Padding;

			NewCommentNode->NodeComment = CommentText;
			NewCommentNode->NodePosX = MinX;
			NewCommentNode->NodePosY = MinY;
			NewCommentNode->NodeWidth = MaxX - MinX;
			NewCommentNode->NodeHeight = MaxY - MinY;
			NewCommentNode->CommentColor = FLinearColor::White;

			for (UObject* Node : FoundSelectedNodes)
			{
				if (UEdGraphNode* CurrentNode = Cast<UEdGraphNode>(Node))
				{
					NewCommentNode->AddNodeUnderComment(CurrentNode);
				}
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(InBlueprint);
	}
}
LLMResponseParts GeminiAssistantPanel::ParseLLMResponse(const FString& FullResponse)
{
	LLMResponseParts Result;

	FString DetailsMarker = TEXT("DETAILS:");
	FString SummaryMarker = TEXT("SUMMARY:");

	int32 DetailsIndex = FullResponse.Find(DetailsMarker);
	int32 SummaryIndex = FullResponse.Find(SummaryMarker);

	if (DetailsIndex != INDEX_NONE && SummaryIndex != INDEX_NONE) {
		// Extract DETAILS (from DETAILS: to SUMMARY:)
		int32 DetailsStart = DetailsIndex + DetailsMarker.Len();
		int32 DetailsLength = SummaryIndex - DetailsStart;
		Result.Details = FullResponse.Mid(DetailsStart, DetailsLength).TrimStartAndEnd();

		// Extract SUMMARY (from SUMMARY: to end)
		int32 SummaryStart = SummaryIndex + SummaryMarker.Len();
		Result.Summary = FullResponse.Mid(SummaryStart).TrimStartAndEnd();
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE