using UnrealBuildTool;

public class shooter : ModuleRules
{
    public shooter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
        PrivateDependencyModuleNames.Add("RenderCore");
        PublicIncludePaths.Add(ModuleDirectory);
    }
}
