#include "BlueprintLayoutToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"

namespace BlueprintLayoutTests
{
	UBlueprint* MakeTestBlueprint()
	{
		return FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BP_LayoutToolsetTest")),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}

	UEdGraphNode* AddKnot(UEdGraph* Graph, int32 X, int32 Y)
	{
		FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
		UK2Node_Knot* Knot = Creator.CreateNode();
		Knot->NodePosX = X;
		Knot->NodePosY = Y;
		Creator.Finalize();
		return Knot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintLayoutCommentBoxMathTest,
	"BlueprintLayoutToolset.CommentBox.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintLayoutCommentBoxMathTest::RunTest(const FString& Parameters)
{
	const TArray<FBox2D> Rects = {
		FBox2D(FVector2D(100.0, 50.0), FVector2D(300.0, 150.0)),
		FBox2D(FVector2D(400.0, 0.0), FVector2D(500.0, 80.0)),
	};

	const FBox2D Box = BlueprintLayout::ComputeCommentBox(Rects, 32.f, 40.f);

	TestEqual(TEXT("Left = leftmost node - padding"), static_cast<float>(Box.Min.X), 68.f);
	TestEqual(TEXT("Top = topmost node - padding - title"), static_cast<float>(Box.Min.Y), -72.f);
	TestEqual(TEXT("Right = rightmost node + padding"), static_cast<float>(Box.Max.X), 532.f);
	TestEqual(TEXT("Bottom = lowest node + padding"), static_cast<float>(Box.Max.Y), 182.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintLayoutAddCommentBoxTest,
	"BlueprintLayoutToolset.CommentBox.AddsCommentAroundNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintLayoutAddCommentBoxTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = BlueprintLayoutTests::MakeTestBlueprint();
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!TestNotNull(TEXT("Event graph"), Graph))
	{
		return false;
	}

	UEdGraphNode* A = BlueprintLayoutTests::AddKnot(Graph, 200, 100);
	UEdGraphNode* B = BlueprintLayoutTests::AddKnot(Graph, 600, 300);
	const FLinearColor Color(0.2f, 0.4f, 0.6f, 1.f);

	UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(
		UBlueprintLayoutToolset::AddCommentBox(Graph, { A, B }, TEXT("Build rows"), Color, 32));

	if (!TestNotNull(TEXT("Comment node created"), Comment))
	{
		return false;
	}
	TestTrue(TEXT("Comment is in the graph"), Graph->Nodes.Contains(Comment));
	TestEqual(TEXT("Comment text"), Comment->NodeComment, FString(TEXT("Build rows")));
	TestEqual(TEXT("Comment colour"), Comment->CommentColor, Color);
	TestTrue(TEXT("Left edge is left of the first node"), Comment->NodePosX <= 200 - 32);
	TestTrue(TEXT("Top edge is above the first node"), Comment->NodePosY < 100 - 32);
	TestTrue(TEXT("Right edge is right of the last node"), Comment->NodePosX + Comment->NodeWidth > 600 + 32);
	TestTrue(TEXT("Bottom edge is below the last node"), Comment->NodePosY + Comment->NodeHeight > 300 + 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintLayoutSetNodePositionTest,
	"BlueprintLayoutToolset.Nodes.SetPositionMovesComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintLayoutSetNodePositionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = BlueprintLayoutTests::MakeTestBlueprint();
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	UEdGraphNode* Knot = BlueprintLayoutTests::AddKnot(Graph, 0, 0);
	UEdGraphNode* Comment = UBlueprintLayoutToolset::AddCommentBox(Graph, { Knot }, TEXT("Move me"), FLinearColor::White, 16);
	if (!TestNotNull(TEXT("Comment node created"), Comment))
	{
		return false;
	}

	UBlueprintLayoutToolset::SetNodePosition(Comment, 480, -96);

	TestEqual(TEXT("X"), Comment->NodePosX, 480);
	TestEqual(TEXT("Y"), Comment->NodePosY, -96);
	TestEqual(TEXT("Enclosed node did not move"), Knot->NodePosX, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintLayoutDeleteNodesTest,
	"BlueprintLayoutToolset.Nodes.DeleteAnyNodeType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintLayoutDeleteNodesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = BlueprintLayoutTests::MakeTestBlueprint();
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	UEdGraphNode* Knot = BlueprintLayoutTests::AddKnot(Graph, 0, 0);
	UEdGraphNode* Keep = BlueprintLayoutTests::AddKnot(Graph, 300, 0);
	UEdGraphNode* Comment = UBlueprintLayoutToolset::AddCommentBox(Graph, { Knot }, TEXT("Delete me"), FLinearColor::White, 16);

	const int32 Deleted = UBlueprintLayoutToolset::DeleteNodes({ Knot, Comment });

	TestEqual(TEXT("Deleted count"), Deleted, 2);
	TestFalse(TEXT("Knot removed"), Graph->Nodes.Contains(Knot));
	TestFalse(TEXT("Comment removed"), Graph->Nodes.Contains(Comment));
	TestTrue(TEXT("Other node kept"), Graph->Nodes.Contains(Keep));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
