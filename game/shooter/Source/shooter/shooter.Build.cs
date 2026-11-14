using UnrealBuildTool;

public class shooter : ModuleRules
{
    public shooter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "AnimationCore", "AnimGraphRuntime", "Json" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore" });
        PublicIncludePaths.Add(ModuleDirectory);
    }
}
