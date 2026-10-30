using UnrealBuildTool;

public class GitStatusEditor : ModuleRules
{
    public GitStatusEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "Projects",
            "DirectoryWatcher",
            "UnrealEd",
            "Json"
        });
    }
}

