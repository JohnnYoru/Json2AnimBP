using UnrealBuildTool;

public class Json2AnimBPPlugin : ModuleRules
{
    public Json2AnimBPPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "UnrealEd",
            "Json",
            "Kismet",          // FBlueprintEditorUtils
            "AnimGraph",       // UAnimationGraph, UAnimGraphNode_Base
            "BlueprintGraph",  // UEdGraph, FEdGraphUtilities
            "InputCore",
            "ApplicationCore", // FPlatformApplicationMisc (clipboard fallback)
            "GraphEditor",     // SGraphEditor
            "WorkspaceMenuStructure",
        });
    }
}
