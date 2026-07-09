using UnrealBuildTool;

public class PivotControlToolLite : ModuleRules
{
	public PivotControlToolLite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"AssetRegistry",
			"MeshDescription",
			"StaticMeshDescription",
			"RawMesh",
			"EditorStyle",
			"EditorSubsystem",
			"InputCore",
			"PhysicsCore",
			"AssetTools",
			"WorkspaceMenuStructure",
			"Projects"
		});
	}
}
