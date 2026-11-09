#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "BlueprintLayoutToolset.generated.h"

class UEdGraph;
class UEdGraphNode;

/** Editor graph alignment commands, the same as the Shift+W/S/A/D and distribute commands. */
UENUM(BlueprintType)
enum class EBlueprintNodeAlignment : uint8
{
	Top,
	Middle,
	Bottom,
	Left,
	Center,
	Right,
	DistributeHorizontally,
	DistributeVertically,
};

/** Size and position of one node as the graph editor draws it. */
USTRUCT(BlueprintType)
struct FBlueprintNodeBounds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BlueprintLayout")
	TObjectPtr<UEdGraphNode> Node = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "BlueprintLayout")
	float X = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "BlueprintLayout")
	float Y = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "BlueprintLayout")
	float Width = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "BlueprintLayout")
	float Height = 0.f;
};

namespace BlueprintLayout
{
	/**
	 * Box that encloses all given node rectangles (X, Y, Width, Height in graph units),
	 * grown by Padding on every side plus TitleHeight on top for the comment title bar.
	 * Returns X, Y, Width, Height of the comment box.
	 */
	BLUEPRINTLAYOUTTOOLSET_API FBox2D ComputeCommentBox(const TArray<FBox2D>& NodeRects, float Padding, float TitleHeight);
}

/**
 * Blueprint graph layout tools for agents.
 *
 * The straighten, align, distribute and bounds tools use the graph editor widgets, so the graph
 * must be open in a Blueprint editor and drawn at least once after its nodes were created.
 * The tools open the graph if needed. Create nodes in one tool call and straighten them in a
 * later call.
 */
UCLASS(BlueprintType)
class UBlueprintLayoutToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Straightens one wire, exactly like pressing Q in the graph editor: moves NodeToMove vertically
	 * so that its pin PinToAlign is level with the pin SourcePin on SourceNode.
	 * The two pins must be connected. The pin names are the names from get_node_infos.
	 * @return The new Y position of NodeToMove.
	 */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static float StraightenConnection(UEdGraphNode* SourceNode, FName SourcePin, UEdGraphNode* NodeToMove, FName PinToAlign);

	/**
	 * Returns the position and the drawn size of each node, in graph units.
	 * Use it to space nodes exactly instead of estimating widths.
	 */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static TArray<FBlueprintNodeBounds> GetNodeBounds(const TArray<UEdGraphNode*>& Nodes);

	/**
	 * Aligns or distributes the nodes, the same as the editor commands
	 * (Align Top Shift+W, Bottom Shift+S, Left Shift+A, Right Shift+D, Middle, Center,
	 * Distribute Horizontally, Distribute Vertically). All nodes must be in one graph.
	 */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static void AlignNodes(const TArray<UEdGraphNode*>& Nodes, EBlueprintNodeAlignment Alignment);

	/**
	 * Adds a comment box around the given nodes, with Padding graph units of space around them.
	 * Uses the drawn node sizes when the graph is open, otherwise an estimate.
	 * @return The new comment node.
	 */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static UEdGraphNode* AddCommentBox(UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, const FString& Text, FLinearColor Color, int32 Padding = 32);

	/** Moves any node, comment boxes included. Comment boxes move without the nodes inside them. */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static void SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y);

	/**
	 * Deletes nodes of any type, comment boxes included. Does not compile the Blueprint.
	 * @return The number of nodes deleted.
	 */
	UFUNCTION(meta = (AICallable), Category = "BlueprintLayout")
	static int32 DeleteNodes(const TArray<UEdGraphNode*>& Nodes);
};
