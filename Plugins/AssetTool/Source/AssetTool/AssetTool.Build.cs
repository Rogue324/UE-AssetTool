using UnrealBuildTool;

public class AssetTool : ModuleRules
{
    public AssetTool(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetTools",
            "ContentBrowser",
            "CoreUObject",
            "EditorScriptingUtilities",
            "Engine",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd"
        });
    }
}
