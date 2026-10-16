using UnrealBuildTool;
public class GrimoireEditor : ModuleRules
{
    public GrimoireEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GrimoireRuntime", "Json", "HTTP", "EditorSubsystem" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Slate", "SlateCore", "ToolMenus", "AssetRegistry", "AssetTools", "Projects", "InputCore", "PropertyEditor", "UMG", "UMGEditor", "Kismet", "KismetCompiler" });
    }
}
