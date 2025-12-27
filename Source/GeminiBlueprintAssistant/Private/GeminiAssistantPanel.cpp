// Private/GeminiAssistantPanel.cpp
#include "GeminiAssistantPanel.h"
#include "SlateOptMacros.h"

// UI related includes (Direct Slate widget headers)
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h" 
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h" 
#include "EditorStyleSet.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"


// Core includes for config/file operations
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"

// Blueprint Core Classes (These exist as direct headers)
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphSchema.h"

#include "K2Node.h"

#include "BlueprintEditorModule.h"

#include "Editor.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditor.h"
#include "SGraphPanel.h"

#include "ScopedTransaction.h"

#include "BlueprintNodePreprocessor.h"

#define LOCTEXT_NAMESPACE "FGeminiBlueprintAssistantModule"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void GeminiAssistantPanel::Construct(const FArguments& InArgs)
{
	GeminiClient = MakeShared<FGeminiAPIClient>();
	GeminiClient->OnGeminiResponseReceived.BindRaw(this, &GeminiAssistantPanel::OnGeminiResponse);

	bHasValidApiKey = CheckApiKeyExists();

	ChildSlot
		[
			SNew(SBorder)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
#else
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
#endif
				.Padding(FMargin(10.0f))
				[
					SAssignNew(ContentSwitcher, SWidgetSwitcher)
						+ SWidgetSwitcher::Slot()
						[
							CreateApiKeySetupWidget()
						]
						+ SWidgetSwitcher::Slot()
						[
							CreateMainInterfaceWidget()
						]
				]
		];

	// Set the active widget based on API key existence
	ContentSwitcher->SetActiveWidgetIndex(bHasValidApiKey ? 1 : 0);

	CurrentPromptText = FText::GetEmpty();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply GeminiAssistantPanel::OnProcessButtonClicked()
{
	/*
	if (CurrentPromptText.IsEmpty())
	{
		ResponseTextBlock->SetText(LOCTEXT("EmptyPromptWarning", "Please enter a prompt."));
		return FReply::Handled();
	}
	*/
	CopyButton.Get()->SetVisibility(EVisibility::Collapsed);
	ClearButton.Get()->SetVisibility(EVisibility::Collapsed);
	ProcessButton.Get()->SetVisibility(EVisibility::Hidden);
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

	SelectedNodes = GetSelectedBlueprintNodes(ActiveBlueprint);
	FString NodesData = ExtractNodeDataForGemini(SelectedNodes);
	CachedBlueprint = ActiveBlueprint;
	CachedFocusedGraph = GetFocusedGraph(ActiveBlueprint);
	FString PromptToSend;
	if (SelectedNodes.Num() == 0)
	{
		TArray<UEdGraphNode*> NodesToProcess = GetAllNodesFromActiveGraph(ActiveBlueprint);
		NodesData = ExtractNodeDataForGemini(NodesToProcess);

		if (NodesData.IsEmpty())
		{
			PromptToSend = FString::Printf(TEXT("Summarize the main purpose of the Blueprint named '%s'. The graph appears to be empty or has no processable nodes. Please respond in this exact format :  DETAILS: [summarise the blueprint in a user-friendly manner with available information]."),
				*ActiveBlueprint->GetName(),
				CurrentPromptText.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("\nUser Query: %s"), *CurrentPromptText.ToString()));
		}
		else
		{
			PromptToSend = FString::Printf(TEXT("Given the following Unreal Engine Blueprint graph from Blueprint '%s', summarize the entire graph's purpose and functionality. Please respond in this exact format :  DETAILS: [summarise the nodes in user-friendly manner]\nSUMMARY:[keep empty]. Blueprint Graph Data: %s\n"),
				*ActiveBlueprint->GetName(), *NodesData, 
				CurrentPromptText.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("\nUser Query: %s"), *CurrentPromptText.ToString()));
		}
		ResponseTextBlock->SetText(LOCTEXT("SummarizingEntireGraph", "Summarizing entire Blueprint graph with Gemini..."));
	}
	else
	{
		PromptToSend = FString::Printf(TEXT("Given the following Unreal Engine Blueprint nodes from Blueprint '%s', summarize their collective purpose and Please respond in this exact format :  DETAILS: [summarise the selected nodes in user-friendly manner] \nSUMMARY: [concise one-line summary]. Blueprint Graph Nodes Data: %s\n"),
			*ActiveBlueprint->GetName(), *NodesData, 
			CurrentPromptText.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("\nUser Query: %s"), *CurrentPromptText.ToString()));
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

		UBlueprint* ActiveBlueprint = GetActiveBlueprint();
		// Ensure there's an active blueprint and at least one graph (UbergraphPages[0] is typically the Event Graph)
		if (WriteCommentsCheckBox->IsChecked())
		{
			if (CachedFocusedGraph && CachedBlueprint)
			// Add comment to the first graph (usually Event Graph) for simplicity
				Results.Summary.Len() > 1 ? AddCommentNodeToBlueprint(CachedBlueprint, CachedFocusedGraph, Results.Summary) : (void)0;
		}
	}
	else
	{
		ResponseTextBlock->SetText(FText::Format(LOCTEXT("GeminiError", "Gemini API Error: {0}"), FText::FromString(ErrorMessage)));
		UE_LOG(LogTemp, Error, TEXT("Gemini API Error: %s"), *ErrorMessage);
	}
	ProcessButton.Get()->SetVisibility(EVisibility::Visible);
	CopyButton.Get()->SetVisibility(EVisibility::Visible);
	ClearButton.Get()->SetVisibility(EVisibility::Visible);
	CachedFocusedGraph = nullptr;
	CachedBlueprint = nullptr;
	SelectedNodes.Empty();
}

FReply GeminiAssistantPanel::OnCopyResponseClicked()
{
	if (ResponseTextBlock.IsValid())
	{
		FString ResponseText = ResponseTextBlock->GetText().ToString();
		if (!ResponseText.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*ResponseText);

			// Optional: Show a notification that text was copied
			FNotificationInfo Info(FText::FromString("Response copied to clipboard!"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	return FReply::Handled();
}

FReply GeminiAssistantPanel::OnClearClicked()
{
	if (ResponseTextBlock.IsValid())
	{
		ResponseTextBlock->SetText(FText::FromString(""));
	}
	return FReply::Handled();
}


// --- BLUEPRINT INTERACTION FUNCTIONS ---

UBlueprint* GeminiAssistantPanel::GetActiveBlueprint() const
{
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
					return Blueprint;
				}
			}
		}
	}
	return nullptr; 
}

UEdGraph* GeminiAssistantPanel::GetFocusedGraph(UBlueprint* InBlueprint)
{
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

	UEdGraph* FocusedGraph;
	// Find the specific Blueprint editor instance that is editing our InBlueprint.
	for (TSharedRef<IBlueprintEditor> BlueprintEditorInstance : BlueprintEditorModule.GetBlueprintEditors())
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		FocusedGraph = BlueprintEditorInstance->GetFocusedGraph();
#else
		FocusedGraph = (StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorInstance))->GetFocusedGraph();
#endif	
		if (FocusedGraph && FBlueprintEditorUtils::FindBlueprintForGraph(FocusedGraph) == InBlueprint)
		{
			return FocusedGraph;
		}
	}
	return nullptr;
}

TArray<UEdGraphNode*> GeminiAssistantPanel::GetSelectedBlueprintNodes(UBlueprint* InBlueprint) const
{
	TArray<UEdGraphNode*> FSelectedNodes;

	if (!InBlueprint)
	{
		return FSelectedNodes;
	}

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

	TSharedPtr<IBlueprintEditor> FoundBlueprintEditor;
	
	for (TSharedRef<IBlueprintEditor> BlueprintEditorInstance : BlueprintEditorModule.GetBlueprintEditors())
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		if (FBlueprintEditorUtils::FindBlueprintForGraph(BlueprintEditorInstance->GetFocusedGraph()) == InBlueprint)
#else
		if (FBlueprintEditorUtils::FindBlueprintForGraph((StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorInstance))->GetFocusedGraph()) == InBlueprint)
#endif
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
			for (UObject* Node : FoundSelectedNodes)
			{
				if (UEdGraphNode* CurrentNode = Cast<UEdGraphNode>(Node))
				{
					FSelectedNodes.Add(CurrentNode);
				}
			}
		}
	}

	return FSelectedNodes;
}

TArray<UEdGraphNode*> GeminiAssistantPanel::GetAllNodesFromActiveGraph(UBlueprint* InBlueprint) const
{
	TArray<UEdGraphNode*> AllNodes;

	if (!InBlueprint)
	{
		return AllNodes;
	}

	// Get the currently focused graph from the Blueprint editor
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

	TSharedPtr<IBlueprintEditor> FoundBlueprintEditor;

	for (TSharedRef<IBlueprintEditor> BlueprintEditorInstance : BlueprintEditorModule.GetBlueprintEditors())
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		if (FBlueprintEditorUtils::FindBlueprintForGraph(BlueprintEditorInstance->GetFocusedGraph()) == InBlueprint)
#else
		if (FBlueprintEditorUtils::FindBlueprintForGraph((StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorInstance))->GetFocusedGraph()) == InBlueprint)
#endif
		{
			FoundBlueprintEditor = BlueprintEditorInstance;
			break;
		}
	}

	if (FoundBlueprintEditor.IsValid())
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		UEdGraph* FocusedGraph = FoundBlueprintEditor->GetFocusedGraph();
#else
		UEdGraph* FocusedGraph = (StaticCastSharedPtr<FBlueprintEditor>(FoundBlueprintEditor))->GetFocusedGraph();
#endif
		if (FocusedGraph)
		{
			// Get all nodes from the focused graph
			for (UEdGraphNode* Node : FocusedGraph->Nodes)
			{
				if (Node && IsValid(Node))
				{
					AllNodes.Add(Node);
				}
			}
		}
	}

	return AllNodes;
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		if (FBlueprintEditorUtils::FindBlueprintForGraph(CachedFocusedGraph) == InBlueprint)
#else
		if (FBlueprintEditorUtils::FindBlueprintForGraph((StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorInstance))->GetFocusedGraph()) == InBlueprint)
#endif		
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
			if (SelectedNodes.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("No nodes selected to wrap with comment"));
				return;
			}
			float MinX = FLT_MAX, MinY = FLT_MAX;
			float MaxX = -FLT_MAX, MaxY = -FLT_MAX;

			for (UObject* ANode : SelectedNodes)
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
			const float Padding = 50.f;
			MinX -= Padding;
			MinY -= Padding + 30.f;
			MaxX += Padding;
			MaxY += Padding;

			NewCommentNode->NodeComment = CommentText;
			NewCommentNode->NodePosX = MinX;
			NewCommentNode->NodePosY = MinY;
			NewCommentNode->NodeWidth = MaxX - MinX;
			NewCommentNode->NodeHeight = MaxY - MinY;
			NewCommentNode->CommentColor = FLinearColor::White;

			for (UObject* Node : SelectedNodes)
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

bool GeminiAssistantPanel::CheckApiKeyExists()
{
	// Check if API key exists in EditorPerProjectUserSettings
	FString ApiKey;
	if (GConfig->GetString(TEXT("GeminiAssistant"), TEXT("APIKey"), ApiKey, GEditorPerProjectIni))
	{
		return !ApiKey.IsEmpty();
	}
	return false;
}

void GeminiAssistantPanel::SaveApiKeyToSettings(const FString& ApiKey)
{
	// Save API key to EditorPerProjectUserSettings
	GConfig->SetString(TEXT("GeminiAssistant"), TEXT("APIKey"), *ApiKey, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

FReply GeminiAssistantPanel::OnSubmitApiKeyClicked()
{
	FString ApiKey = ApiKeyTextBox->GetText().ToString();
	if (!ApiKey.IsEmpty())
	{
		SaveApiKeyToSettings(ApiKey);
		bHasValidApiKey = true;

		// Switch to main interface
		ContentSwitcher->SetActiveWidgetIndex(1);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> GeminiAssistantPanel::CreateApiKeySetupWidget()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 0, 0, 10))
		[
			SNew(STextBlock)
				.Text(LOCTEXT("ApiKeySetupTitle", "Gemini API Key Setup"))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
				.Font(FAppStyle::GetFontStyle("BoldFont"))
#else
.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
#endif
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 0, 0, 10))
		[
			SNew(STextBlock)
				.Text(LOCTEXT("ApiKeyInstructions", "Please enter your Gemini API key to continue:"))
				.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 0, 0, 10))
		[
			SAssignNew(ApiKeyTextBox, SEditableTextBox)
				.HintText(LOCTEXT("ApiKeyHint", "Enter your Gemini API key here..."))
				.IsPassword(true) // Hide the API key text
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SAssignNew(SubmitApiKeyButton, SButton)
				.Text(LOCTEXT("SubmitApiKey", "Submit"))
				.OnClicked(this, &GeminiAssistantPanel::OnSubmitApiKeyClicked)
				.IsEnabled_Lambda([this]() {
				return ApiKeyTextBox.IsValid() && !ApiKeyTextBox->GetText().IsEmpty();
					})
		];
}

TSharedRef<SWidget> GeminiAssistantPanel::CreateMainInterfaceWidget()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 0, 0, 5))
		[
			SAssignNew(WriteCommentsCheckBox, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("WriteCommentsLabel", "Write comment for selected nodes."))
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(FMargin(0, 0, 0, 10))
		[
			SAssignNew(ProcessButton, SButton)
				.Text(LOCTEXT("ProcessButtonText", "Process with Gemini"))
				.OnClicked(this, &GeminiAssistantPanel::OnProcessButtonClicked)
				.IsEnabled_Lambda([this]() { return true; /*!CurrentPromptText.IsEmpty();*/ })
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(0, 10, 0, 0))
		[
			SNew(SBorder)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
#else
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
#endif
				.Padding(FMargin(10.0f))
				[
					SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0, 0, 0, 5))
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ResponseLabel", "Gemini Response:"))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
								.Font(FAppStyle::GetFontStyle("BoldFont"))
#else
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
#endif
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(ResponseTextBlock, SMultiLineEditableText)
								.IsReadOnly(true)
								.AutoWrapText(true)
								.Text(LOCTEXT("InitialResponseText", "Response will appear here..."))
						]
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			SAssignNew(CopyButton, SButton)
				.Text(FText::FromString("Copy Response"))
				.OnClicked(this, &GeminiAssistantPanel::OnCopyResponseClicked)
				.ToolTipText(FText::FromString("Copy the Gemini response to clipboard"))
				.Visibility(EVisibility::Collapsed)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			SAssignNew(ClearButton, SButton)
				.Text(FText::FromString("Clear Response"))
				.OnClicked(this, &GeminiAssistantPanel::OnClearClicked)
				.ToolTipText(FText::FromString("Clear the text from panel"))
				.Visibility(EVisibility::Collapsed)
		];
}

#undef LOCTEXT_NAMESPACE