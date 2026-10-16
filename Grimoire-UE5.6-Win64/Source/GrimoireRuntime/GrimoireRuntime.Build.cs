using UnrealBuildTool;
public class GrimoireRuntime : ModuleRules
{
    public GrimoireRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "UMG", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "AIModule", "NavigationSystem", "InputCore" });
    }
}
