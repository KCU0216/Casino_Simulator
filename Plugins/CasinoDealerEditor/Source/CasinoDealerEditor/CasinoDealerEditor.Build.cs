using UnrealBuildTool;

public class CasinoDealerEditor : ModuleRules
{
    public CasinoDealerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry", "AssetTools",
            "BlueprintGraph", "Kismet", "KismetCompiler", "AnimGraph", "AnimGraphRuntime", "AnimationBlueprintLibrary",
            "IKRig", "IKRigEditor", "Slate", "SlateCore", "Json", "JsonUtilities", "UMG", "UMGEditor",
            "RenderCore", "RHI", "ImageCore", "casino_simulator"
        });
    }
}
