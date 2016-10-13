using UnrealBuildTool;

public class ScreenCapture : ModuleRules
{
    public ScreenCapture(TargetInfo Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
                "ScreenCapture/Public"
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "ScreenCapture/Private",
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
            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
            );
    }
}
