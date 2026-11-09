#include "BlueprintLayoutToolset.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "BlueprintEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Layout/SlateRect.h"
#include "ScopedTransaction.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"

#define LOCTEXT_NAMESPACE "BlueprintLayoutToolset"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintLayoutToolset, Log, All);

namespace
{
	/** Height of a comment box title bar, in graph units, for the default font size. */
	constexpr float CommentTitleHeight = 36.f;

	/** Size used for a node when the graph is not open, so no drawn size exists. */
	const FVector2D EstimatedNodeSize(200.0, 100.0);

	/** Reports an error to the calling agent as a tool error. */
	void Fail(const FString& Message)
	{
		UE_LOG(LogBlueprintLayoutToolset, Error, TEXT("%s"), *Message);
		if (FFrame* Frame = FFrame::GetThreadLocalTopStackFrame())
		{
			const FBlueprintExceptionInfo Info(EBlueprintExceptionType::UserRaisedError, FText::FromString(Message));
			FBlueprintCoreDelegates::ThrowScriptException(Frame->Object, *Frame, Info);
		}
	}

	/** The graph editor widget that shows Graph. Opens the graph in its Blueprint editor when bOpen is true. */
	TSharedPtr<SGraphEditor> GetGraphEditor(UEdGraph* Graph, bool bOpen)
	{
		if (!Graph)
		{
			return nullptr;
		}

		TSharedPtr<SGraphEditor> Editor = SGraphEditor::FindGraphEditorForGraph(Graph);
		if (!Editor.IsValid() && bOpen)
		{
			if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
			{
				if (TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(Blueprint, true))
				{
					Editor = BlueprintEditor->OpenGraphAndBringToFront(Graph, false);
				}
			}
		}
		return Editor;
	}

	/** Finds a pin by its internal name, then by its display name. */
	UEdGraphPin* FindPinByName(UEdGraphNode* Node, FName PinName)
	{
		if (UEdGraphPin* Pin = Node->FindPin(PinName))
		{
			return Pin;
		}
		const FString Wanted = PinName.ToString();
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->GetDisplayName().ToString().Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	FString DescribePins(const UEdGraphNode* Node)
	{
		TArray<FString> Names;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && !Pin->bHidden)
			{
				Names.Add(Pin->PinName.ToString());
			}
		}
		return FString::Join(Names, TEXT(", "));
	}

	/** Drawn rectangle of a node, or false when the node has no widget yet. */
	bool GetDrawnRect(const TSharedPtr<SGraphEditor>& Editor, const UEdGraphNode* Node, FBox2D& OutRect)
	{
		FSlateRect Rect;
		if (Editor.IsValid() && Editor->GetBoundsForNode(Node, Rect, 0.f))
		{
			OutRect = FBox2D(FVector2D(Rect.Left, Rect.Top), FVector2D(Rect.Right, Rect.Bottom));
			return true;
		}
		return false;
	}

	void MarkModified(UEdGraph* Graph)
	{
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

	/** Checks that all nodes are valid and in one graph. Returns that graph, or null after reporting the error. */
	UEdGraph* GetSharedGraph(const TArray<UEdGraphNode*>& Nodes)
	{
		UEdGraph* Graph = nullptr;
		for (const UEdGraphNode* Node : Nodes)
		{
			if (!Node || !Node->GetGraph())
			{
				Fail(TEXT("A node is null or not in a graph."));
				return nullptr;
			}
			if (Graph && Node->GetGraph() != Graph)
			{
				Fail(TEXT("All nodes must be in the same graph."));
				return nullptr;
			}
			Graph = Node->GetGraph();
		}
		if (!Graph)
		{
			Fail(TEXT("No nodes given."));
		}
		return Graph;
	}
}

FBox2D BlueprintLayout::ComputeCommentBox(const TArray<FBox2D>& NodeRects, float Padding, float TitleHeight)
{
	FBox2D Box(ForceInit);
	for (const FBox2D& Rect : NodeRects)
	{
		Box += Rect;
	}
	if (!Box.bIsValid)
	{
		return Box;
	}
	Box.Min -= FVector2D(Padding, Padding + TitleHeight);
	Box.Max += FVector2D(Padding, Padding);
	return Box;
}

float UBlueprintLayoutToolset::StraightenConnection(UEdGraphNode* SourceNode, FName SourcePin, UEdGraphNode* NodeToMove, FName PinToAlign)
{
	UEdGraph* Graph = GetSharedGraph({ SourceNode, NodeToMove });
	if (!Graph)
	{
		return 0.f;
	}

	UEdGraphPin* Source = FindPinByName(SourceNode, SourcePin);
	if (!Source)
	{
		Fail(FString::Printf(TEXT("Pin '%s' not found on %s. Pins: %s"), *SourcePin.ToString(), *SourceNode->GetName(), *DescribePins(SourceNode)));
		return 0.f;
	}
	UEdGraphPin* Target = FindPinByName(NodeToMove, PinToAlign);
	if (!Target)
	{
		Fail(FString::Printf(TEXT("Pin '%s' not found on %s. Pins: %s"), *PinToAlign.ToString(), *NodeToMove->GetName(), *DescribePins(NodeToMove)));
		return 0.f;
	}
	if (!Source->LinkedTo.Contains(Target))
	{
		Fail(FString::Printf(TEXT("Pin '%s' on %s is not connected to pin '%s' on %s."),
			*SourcePin.ToString(), *SourceNode->GetName(), *PinToAlign.ToString(), *NodeToMove->GetName()));
		return 0.f;
	}

	const TSharedPtr<SGraphEditor> Editor = GetGraphEditor(Graph, true);
	FBox2D Ignored;
	if (!GetDrawnRect(Editor, SourceNode, Ignored) || !GetDrawnRect(Editor, NodeToMove, Ignored))
	{
		Fail(TEXT("The graph editor has not drawn these nodes yet. Call this tool again in a later tool call."));
		return 0.f;
	}

	Editor->StraightenConnections(Source, Target);
	return static_cast<float>(NodeToMove->NodePosY);
}

TArray<FBlueprintNodeBounds> UBlueprintLayoutToolset::GetNodeBounds(const TArray<UEdGraphNode*>& Nodes)
{
	TArray<FBlueprintNodeBounds> Result;
	UEdGraph* Graph = GetSharedGraph(Nodes);
	if (!Graph)
	{
		return Result;
	}

	const TSharedPtr<SGraphEditor> Editor = GetGraphEditor(Graph, true);
	for (UEdGraphNode* Node : Nodes)
	{
		FBox2D Rect;
		if (!GetDrawnRect(Editor, Node, Rect))
		{
			Fail(FString::Printf(TEXT("Node %s has not been drawn yet. Call this tool again in a later tool call."), *Node->GetName()));
			return {};
		}
		FBlueprintNodeBounds& Bounds = Result.AddDefaulted_GetRef();
		Bounds.Node = Node;
		Bounds.X = static_cast<float>(Rect.Min.X);
		Bounds.Y = static_cast<float>(Rect.Min.Y);
		Bounds.Width = static_cast<float>(Rect.GetSize().X);
		Bounds.Height = static_cast<float>(Rect.GetSize().Y);
	}
	return Result;
}

void UBlueprintLayoutToolset::AlignNodes(const TArray<UEdGraphNode*>& Nodes, EBlueprintNodeAlignment Alignment)
{
	UEdGraph* Graph = GetSharedGraph(Nodes);
	if (!Graph)
	{
		return;
	}
	if (Nodes.Num() < 2)
	{
		Fail(TEXT("Give at least two nodes to align."));
		return;
	}

	const TSharedPtr<SGraphEditor> Editor = GetGraphEditor(Graph, true);
	FBox2D Ignored;
	for (const UEdGraphNode* Node : Nodes)
	{
		if (!GetDrawnRect(Editor, Node, Ignored))
		{
			Fail(TEXT("The graph editor has not drawn these nodes yet. Call this tool again in a later tool call."));
			return;
		}
	}

	Editor->ClearSelectionSet();
	for (UEdGraphNode* Node : Nodes)
	{
		Editor->SetNodeSelection(Node, true);
	}

	switch (Alignment)
	{
	case EBlueprintNodeAlignment::Top:                    Editor->OnAlignTop(); break;
	case EBlueprintNodeAlignment::Middle:                 Editor->OnAlignMiddle(); break;
	case EBlueprintNodeAlignment::Bottom:                 Editor->OnAlignBottom(); break;
	case EBlueprintNodeAlignment::Left:                   Editor->OnAlignLeft(); break;
	case EBlueprintNodeAlignment::Center:                 Editor->OnAlignCenter(); break;
	case EBlueprintNodeAlignment::Right:                  Editor->OnAlignRight(); break;
	case EBlueprintNodeAlignment::DistributeHorizontally: Editor->OnDistributeNodesH(); break;
	case EBlueprintNodeAlignment::DistributeVertically:   Editor->OnDistributeNodesV(); break;
	}

	Editor->ClearSelectionSet();
}

UEdGraphNode* UBlueprintLayoutToolset::AddCommentBox(UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, const FString& Text, FLinearColor Color, int32 Padding)
{
	if (!Graph)
	{
		Fail(TEXT("Graph is null."));
		return nullptr;
	}
	if (Nodes.Num() == 0 || GetSharedGraph(Nodes) != Graph)
	{
		Fail(TEXT("Give one or more nodes that are in the given graph."));
		return nullptr;
	}

	const TSharedPtr<SGraphEditor> Editor = GetGraphEditor(Graph, false);
	TArray<FBox2D> Rects;
	for (const UEdGraphNode* Node : Nodes)
	{
		FBox2D Rect;
		if (!GetDrawnRect(Editor, Node, Rect))
		{
			const FVector2D Position(Node->NodePosX, Node->NodePosY);
			Rect = FBox2D(Position, Position + EstimatedNodeSize);
		}
		Rects.Add(Rect);
	}
	const FBox2D Box = BlueprintLayout::ComputeCommentBox(Rects, static_cast<float>(Padding), CommentTitleHeight);

	const FScopedTransaction Transaction(LOCTEXT("AddCommentBox", "Add Comment Box"));
	UEdGraphNode_Comment* Template = NewObject<UEdGraphNode_Comment>();
	UEdGraphNode_Comment* Comment = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(
		Graph, Template, FVector2f(Box.Min.X, Box.Min.Y), false);
	if (!Comment)
	{
		Fail(TEXT("The comment node could not be created."));
		return nullptr;
	}

	Comment->Modify();
	Comment->SetBounds(FSlateRect(Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y));
	Comment->NodeComment = Text;
	Comment->CommentColor = Color;
	MarkModified(Graph);
	return Comment;
}

void UBlueprintLayoutToolset::SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y)
{
	if (!Node || !Node->GetGraph())
	{
		Fail(TEXT("Node is null or not in a graph."));
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetNodePosition", "Move Node"));
	Node->Modify();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	MarkModified(Node->GetGraph());
}

int32 UBlueprintLayoutToolset::DeleteNodes(const TArray<UEdGraphNode*>& Nodes)
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteNodes", "Delete Nodes"));
	int32 Deleted = 0;
	TSet<UEdGraph*> Graphs;
	for (UEdGraphNode* Node : Nodes)
	{
		UEdGraph* Graph = Node ? Node->GetGraph() : nullptr;
		if (!Graph)
		{
			continue;
		}

		Graph->Modify();
		Node->Modify();
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
		else
		{
			Graph->RemoveNode(Node);
		}
		Graphs.Add(Graph);
		++Deleted;
	}
	for (UEdGraph* Graph : Graphs)
	{
		MarkModified(Graph);
	}
	return Deleted;
}

#undef LOCTEXT_NAMESPACE
