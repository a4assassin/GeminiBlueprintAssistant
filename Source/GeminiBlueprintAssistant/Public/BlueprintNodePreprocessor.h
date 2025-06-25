// BlueprintNodePreprocessor.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintNodePreprocessor.generated.h"

USTRUCT(BlueprintType)
struct FProcessedNodeData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString NodeType;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    FString Comment;

    UPROPERTY(BlueprintReadOnly)
    TMap<FString, FString> Parameters;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Connections;

    FProcessedNodeData()
    {
        NodeType = TEXT("");
        DisplayName = TEXT("");
        Comment = TEXT("");
    }
};

class GEMINIBLUEPRINTASSISTANT_API FBlueprintNodePreprocessor
{
public:
    FBlueprintNodePreprocessor();
    ~FBlueprintNodePreprocessor();

    // Main preprocessing function
    FString PreprocessNodes(const TArray<UK2Node*>& Nodes);

    // Process single node
    FProcessedNodeData ProcessSingleNode(UK2Node* Node);

private:
    // Node type extractors
    FProcessedNodeData ExtractEventNode(class UK2Node_Event* EventNode);
    FProcessedNodeData ExtractFunctionCallNode(class UK2Node_CallFunction* FunctionNode);
    FProcessedNodeData ExtractBranchNode(class UK2Node_IfThenElse* BranchNode);
    FProcessedNodeData ExtractVariableGetNode(class UK2Node_VariableGet* VariableNode);
    FProcessedNodeData ExtractVariableSetNode(class UK2Node_VariableSet* VariableNode);
    FProcessedNodeData ExtractForEachNode(class UK2Node_ForEachElementInEnum* ForEachNode);
    FProcessedNodeData ExtractSequenceNode(class UK2Node_ExecutionSequence* SequenceNode);
    FProcessedNodeData ExtractCustomEventNode(class UK2Node_CustomEvent* CustomEventNode);
    FProcessedNodeData ExtractGenericNode(class UK2Node* Node);

    // Helper functions
    FString GetNodeTypeString(UK2Node* Node);
    TMap<FString, FString> ExtractNodeParameters(UK2Node* Node);
    TArray<FString> ExtractNodeConnections(UK2Node* Node);
    FString GetPinDefaultValue(UEdGraphPin* Pin);
    FString FormatOutput(const TArray<FProcessedNodeData>& ProcessedNodes);
    FString SanitizeString(const FString& Input);
};