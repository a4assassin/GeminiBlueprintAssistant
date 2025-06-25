// BlueprintNodePreprocessor.cpp
#include "BlueprintNodePreprocessor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/MemberReference.h"

#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_ForEachElementInEnum.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_CustomEvent.h"

FBlueprintNodePreprocessor::FBlueprintNodePreprocessor()
{
}

FBlueprintNodePreprocessor::~FBlueprintNodePreprocessor()
{
}

FString FBlueprintNodePreprocessor::PreprocessNodes(const TArray<UK2Node*>& Nodes)
{
    TArray<FProcessedNodeData> ProcessedNodes;

    for (UK2Node* Node : Nodes)
    {
        if (Node)
        {
            FProcessedNodeData ProcessedData = ProcessSingleNode(Node);
            ProcessedNodes.Add(ProcessedData);
        }
    }

    return FormatOutput(ProcessedNodes);
}

FProcessedNodeData FBlueprintNodePreprocessor::ProcessSingleNode(UK2Node* Node)
{
    if (!Node)
    {
        return FProcessedNodeData();
    }

    // Try to cast to specific node types
    if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        return ExtractEventNode(EventNode);
    }
    else if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
    {
        return ExtractFunctionCallNode(FunctionNode);
    }
    else if (UK2Node_IfThenElse* BranchNode = Cast<UK2Node_IfThenElse>(Node))
    {
        return ExtractBranchNode(BranchNode);
    }
    else if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(Node))
    {
        return ExtractVariableGetNode(VariableGetNode);
    }
    else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(Node))
    {
        return ExtractVariableSetNode(VariableSetNode);
    }
    else if (UK2Node_ForEachElementInEnum* ForEachNode = Cast<UK2Node_ForEachElementInEnum>(Node))
    {
        return ExtractForEachNode(ForEachNode);
    }
    else if (UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(Node))
    {
        return ExtractSequenceNode(SequenceNode);
    }
    else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
    {
        return ExtractCustomEventNode(CustomEventNode);
    }
    else
    {
        return ExtractGenericNode(Node);
    }
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractEventNode(UK2Node_Event* EventNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("EVENT");

    if (EventNode->EventReference.GetMemberName() != NAME_None)
    {
        Data.DisplayName = EventNode->EventReference.GetMemberName().ToString();
    }
    else
    {
        Data.DisplayName = EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
    }

    Data.Comment = SanitizeString(EventNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(EventNode);
    Data.Connections = ExtractNodeConnections(EventNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractFunctionCallNode(UK2Node_CallFunction* FunctionNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("CALL");

    UFunction* Function = FunctionNode->GetTargetFunction();
    if (Function)
    {
        Data.DisplayName = Function->GetName();

        // Add class name if it's not a global function
        if (Function->GetOuterUClass())
        {
            FString ClassName = Function->GetOuterUClass()->GetName();
            Data.DisplayName = FString::Printf(TEXT("%s.%s"), *ClassName, *Data.DisplayName);
        }
    }
    else
    {
        Data.DisplayName = FunctionNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
    }

    Data.Comment = SanitizeString(FunctionNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(FunctionNode);
    Data.Connections = ExtractNodeConnections(FunctionNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractBranchNode(UK2Node_IfThenElse* BranchNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("BRANCH");
    Data.DisplayName = TEXT("Condition");

    // Try to get condition from connected pin
    UEdGraphPin* ConditionPin = BranchNode->GetConditionPin();
    if (ConditionPin && ConditionPin->LinkedTo.Num() > 0)
    {
        Data.DisplayName = TEXT("Connected Condition");
    }
    else if (ConditionPin)
    {
        FString DefaultValue = GetPinDefaultValue(ConditionPin);
        if (!DefaultValue.IsEmpty())
        {
            Data.DisplayName = DefaultValue;
        }
    }

    Data.Comment = SanitizeString(BranchNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(BranchNode);
    Data.Connections = ExtractNodeConnections(BranchNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractVariableGetNode(UK2Node_VariableGet* VariableNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("GET");

    if (VariableNode->VariableReference.GetMemberName() != NAME_None)
    {
        Data.DisplayName = VariableNode->VariableReference.GetMemberName().ToString();
    }
    else
    {
        Data.DisplayName = VariableNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
    }

    Data.Comment = SanitizeString(VariableNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(VariableNode);
    Data.Connections = ExtractNodeConnections(VariableNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractVariableSetNode(UK2Node_VariableSet* VariableNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("SET");

    if (VariableNode->VariableReference.GetMemberName() != NAME_None)
    {
        Data.DisplayName = VariableNode->VariableReference.GetMemberName().ToString();
    }
    else
    {
        Data.DisplayName = VariableNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
    }

    // Try to get the value being set
    FName VariableName = VariableNode->VariableReference.GetMemberName();
    UEdGraphPin* ValuePin = VariableNode->FindPin(VariableName);

    if (ValuePin)
    {
        FString Value = GetPinDefaultValue(ValuePin);
        if (!Value.IsEmpty())
        {
            Data.DisplayName += FString::Printf(TEXT(" = %s"), *Value);
        }
    }

    Data.Comment = SanitizeString(VariableNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(VariableNode);
    Data.Connections = ExtractNodeConnections(VariableNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractForEachNode(UK2Node_ForEachElementInEnum* ForEachNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("FOREACH");
    Data.DisplayName = TEXT("Array");

    // Try to get array input
    UEdGraphPin* ArrayPin = ForEachNode->FindPin(TEXT("Enum"));
    if (ArrayPin)
    {
        if (ArrayPin->LinkedTo.Num() > 0)
        {
            Data.DisplayName = TEXT("Connected Array");
        }
        else
        {
            FString DefaultValue = GetPinDefaultValue(ArrayPin);
            if (!DefaultValue.IsEmpty())
            {
                Data.DisplayName = DefaultValue;
            }
        }
    }

    Data.Comment = SanitizeString(ForEachNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(ForEachNode);
    Data.Connections = ExtractNodeConnections(ForEachNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractSequenceNode(UK2Node_ExecutionSequence* SequenceNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("SEQUENCE");

    int32 OutputCount = 0;
    for (UEdGraphPin* Pin : SequenceNode->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinName.ToString().StartsWith(TEXT("Then")))
        {
            ++OutputCount;
        }
    }
    Data.DisplayName = FString::Printf(TEXT("%d outputs"), OutputCount);
    Data.Comment = SanitizeString(SequenceNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(SequenceNode);
    Data.Connections = ExtractNodeConnections(SequenceNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractCustomEventNode(UK2Node_CustomEvent* CustomEventNode)
{
    FProcessedNodeData Data;
    Data.NodeType = TEXT("CUSTOM_EVENT");
    Data.DisplayName = CustomEventNode->CustomFunctionName.ToString();
    Data.Comment = SanitizeString(CustomEventNode->NodeComment);
    Data.Parameters = ExtractNodeParameters(CustomEventNode);
    Data.Connections = ExtractNodeConnections(CustomEventNode);

    return Data;
}

FProcessedNodeData FBlueprintNodePreprocessor::ExtractGenericNode(UK2Node* Node)
{
    FProcessedNodeData Data;

    // Get the actual class name for better categorization
    FString ClassName = Node->GetClass()->GetName();

    // Enhanced node type detection based on class patterns
    if (ClassName.Contains(TEXT("Math")) || ClassName.Contains(TEXT("Add")) ||
        ClassName.Contains(TEXT("Multiply")) || ClassName.Contains(TEXT("Subtract")))
    {
        Data.NodeType = TEXT("MATH");
    }
    else if (ClassName.Contains(TEXT("String")) || ClassName.Contains(TEXT("Text")))
    {
        Data.NodeType = TEXT("STRING");
    }
    else if (ClassName.Contains(TEXT("Array")) || ClassName.Contains(TEXT("Set")) || ClassName.Contains(TEXT("Map")))
    {
        Data.NodeType = TEXT("COLLECTION");
    }
    else if (ClassName.Contains(TEXT("Cast")) || ClassName.Contains(TEXT("IsValid")))
    {
        Data.NodeType = TEXT("VALIDATION");
    }
    else if (ClassName.Contains(TEXT("Delay")) || ClassName.Contains(TEXT("Timeline")))
    {
        Data.NodeType = TEXT("TIMING");
    }
    else if (ClassName.Contains(TEXT("Widget")) || ClassName.Contains(TEXT("UI")))
    {
        Data.NodeType = TEXT("UI");
    }
    else if (ClassName.Contains(TEXT("Audio")) || ClassName.Contains(TEXT("Sound")))
    {
        Data.NodeType = TEXT("AUDIO");
    }
    else if (ClassName.Contains(TEXT("Physics")) || ClassName.Contains(TEXT("Collision")))
    {
        Data.NodeType = TEXT("PHYSICS");
    }
    else if (ClassName.Contains(TEXT("AI")) || ClassName.Contains(TEXT("Blackboard")) || ClassName.Contains(TEXT("Behavior")))
    {
        Data.NodeType = TEXT("AI");
    }
    else if (ClassName.Contains(TEXT("Animation")) || ClassName.Contains(TEXT("Montage")))
    {
        Data.NodeType = TEXT("ANIMATION");
    }
    else
    {
        Data.NodeType = TEXT("NODE");
    }

    // Get the best available display name
    FString DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
    if (DisplayName.IsEmpty())
    {
        DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    }
    if (DisplayName.IsEmpty())
    {
        DisplayName = ClassName.Replace(TEXT("K2Node_"), TEXT("")).Replace(TEXT("_"), TEXT(" "));
    }

    Data.DisplayName = DisplayName;
    Data.Comment = SanitizeString(Node->NodeComment);
    Data.Parameters = ExtractNodeParameters(Node);
    Data.Connections = ExtractNodeConnections(Node);

    return Data;
}

TMap<FString, FString> FBlueprintNodePreprocessor::ExtractNodeParameters(UK2Node* Node)
{
    TMap<FString, FString> Parameters;

    if (!Node)
    {
        return Parameters;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input &&
            Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            FString Value = GetPinDefaultValue(Pin);
            FString PinName = Pin->PinName.ToString();

            // Handle connected pins differently
            if (Pin->LinkedTo.Num() > 0)
            {
                // Show that it's connected to another node
                UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    FString ConnectedNodeName = LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
                    Value = FString::Printf(TEXT("Connected(%s)"), *ConnectedNodeName);
                }
                else
                {
                    Value = TEXT("Connected");
                }
            }
            else if (Value.IsEmpty())
            {
                // Try to get type information for empty values
                FString PinType = Pin->PinType.PinCategory.ToString();
                if (PinType == TEXT("bool"))
                {
                    Value = TEXT("false");
                }
                else if (PinType == TEXT("int") || PinType == TEXT("float") || PinType == TEXT("real"))
                {
                    Value = TEXT("0");
                }
                else if (PinType == TEXT("string") || PinType == TEXT("text"))
                {
                    Value = TEXT("\"\"");
                }
                else
                {
                    Value = FString::Printf(TEXT("<%s>"), *PinType);
                }
            }

            if (!Value.IsEmpty())
            {
                Parameters.Add(PinName, Value);
            }
        }
    }

    return Parameters;
}

TArray<FString> FBlueprintNodePreprocessor::ExtractNodeConnections(UK2Node* Node)
{
    TArray<FString> Connections;

    if (!Node)
    {
        return Connections;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
        {
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    FString ConnectionInfo = FString::Printf(TEXT("%s.%s"),
                        *LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                        *LinkedPin->PinName.ToString());
                    Connections.Add(ConnectionInfo);
                }
            }
        }
    }

    return Connections;
}

FString FBlueprintNodePreprocessor::GetPinDefaultValue(UEdGraphPin* Pin)
{
    if (!Pin)
    {
        return FString();
    }

    if (!Pin->DefaultValue.IsEmpty())
    {
        return Pin->DefaultValue;
    }

    if (!Pin->DefaultTextValue.IsEmpty())
    {
        return Pin->DefaultTextValue.ToString();
    }

    if (Pin->DefaultObject)
    {
        return Pin->DefaultObject->GetName();
    }

    return FString();
}

FString FBlueprintNodePreprocessor::FormatOutput(const TArray<FProcessedNodeData>& ProcessedNodes)
{
    FString Output;

    for (int32 i = 0; i < ProcessedNodes.Num(); i++)
    {
        const FProcessedNodeData& NodeData = ProcessedNodes[i];

        FString Line = FString::Printf(TEXT("%d. %s"), i + 1, *NodeData.NodeType);

        if (!NodeData.DisplayName.IsEmpty())
        {
            Line += FString::Printf(TEXT(": %s"), *NodeData.DisplayName);
        }

        // Add parameters if any
        if (NodeData.Parameters.Num() > 0)
        {
            TArray<FString> ParamStrings;
            for (const auto& Param : NodeData.Parameters)
            {
                ParamStrings.Add(FString::Printf(TEXT("%s=%s"), *Param.Key, *Param.Value));
            }
            Line += FString::Printf(TEXT("(%s)"), *FString::Join(ParamStrings, TEXT(", ")));
        }

        // Add comment if exists
        if (!NodeData.Comment.IsEmpty())
        {
            Line += FString::Printf(TEXT(" // %s"), *NodeData.Comment);
        }

        Output += Line;
        if (i < ProcessedNodes.Num() - 1)
        {
            Output += TEXT("\n");
        }
    }

    return Output;
}

FString FBlueprintNodePreprocessor::SanitizeString(const FString& Input)
{
    FString Sanitized = Input;

    // Remove newlines and excessive whitespace
    Sanitized = Sanitized.Replace(TEXT("\n"), TEXT(" "));
    Sanitized = Sanitized.Replace(TEXT("\r"), TEXT(" "));
    Sanitized = Sanitized.Replace(TEXT("\t"), TEXT(" "));

    // Trim whitespace
    Sanitized = Sanitized.TrimStartAndEnd();

    return Sanitized;
}

// Usage example in your plugin:
/*
void YourPluginFunction()
{
    // Get your blueprint nodes (example)
    UBlueprint* Blueprint = GetYourBlueprint();
    TArray<UK2Node*> Nodes;

    // Collect nodes from blueprint graphs
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        for (UEdGraphNode* GraphNode : Graph->Nodes)
        {
            if (UK2Node* K2Node = Cast<UK2Node>(GraphNode))
            {
                Nodes.Add(K2Node);
            }
        }
    }

    // Preprocess the nodes
    FBlueprintNodePreprocessor Preprocessor;
    FString CleanText = Preprocessor.PreprocessNodes(Nodes);

    // Use CleanText for your LLM summarization
    UE_LOG(LogTemp, Warning, TEXT("Preprocessed Blueprint: %s"), *CleanText);
}
*/