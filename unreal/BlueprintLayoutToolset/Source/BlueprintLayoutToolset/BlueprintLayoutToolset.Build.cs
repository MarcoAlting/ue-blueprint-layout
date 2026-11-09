using UnrealBuildTool;

public class BlueprintLayoutToolset : ModuleRules
{
	public BlueprintLayoutToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ToolsetRegistry",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"EditorSubsystem",
			"Kismet",
			"BlueprintGraph",
			"GraphEditor",
			"Slate",
			"SlateCore",
		});
	}
}
