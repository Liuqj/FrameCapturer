using UnrealBuildTool;

public class FrameCapturer : ModuleRules
{
    public FrameCapturer(TargetInfo Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
                "FrameCapturer/Public"
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "FrameCapturer/Private",
            }
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "RenderCore",
                "RHI",
                "ShaderCore",
                "SlateCore",
                "Slate",
                "UtilityShaders",
                "UMG",
            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
            );
    }
}
