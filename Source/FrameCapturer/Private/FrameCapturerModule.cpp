#include "FrameCapturerPrivatePCH.h"
#include "ShaderFileVisitor.h"
DEFINE_LOG_CATEGORY(LogFrameCapturer);

class FRAMECAPTURER_API FFrameCapturerModule : public IModuleInterface
{
public:
	static inline FFrameCapturerModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FFrameCapturerModule >("FrameCapturer");
	}

	virtual void StartupModule() override
	{
#if ExperimentalGPUBlur
		FString PluginShadersDirectory = FPaths::Combine(*FPaths::EnginePluginsDir(), TEXT("/Kingsoft/FrameCapturer/Shaders"));
		FString EngineShadersDirectory = FPaths::Combine(*FPaths::EngineDir(), TEXT("Shaders"));

		ShaderFiles = new FShaderFileVisitor();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.IterateDirectoryRecursively(*PluginShadersDirectory, *ShaderFiles);

		for (int32 ShaderFileIndex = 0; ShaderFileIndex < ShaderFiles->ShaderFilePaths.Num(); ShaderFileIndex++)
		{
			FString CurrentShaderFile = ShaderFiles->ShaderFilePaths[ShaderFileIndex];
			FString GameShaderFullPath = FPaths::Combine(*PluginShadersDirectory, *CurrentShaderFile);
			FString EngineShaderFullPath = FPaths::Combine(*EngineShadersDirectory, *CurrentShaderFile);

			if (PlatformFile.CopyFile(*EngineShaderFullPath, *GameShaderFullPath))
			{
				UE_LOG(LogFrameCapturer, Log, TEXT("Shader file %s copied to %s."), *GameShaderFullPath, *EngineShaderFullPath);
			}
			else
			{
				UE_LOG(LogFrameCapturer, Warning, TEXT("Could not copy %s to %s!"), *GameShaderFullPath, *EngineShaderFullPath);
			}
		}
#endif
	}

	virtual void ShutdownModule() override
	{
#if ExperimentalGPUBlur
		FString EngineShadersDirectory = FPaths::Combine(*FPaths::EngineDir(), TEXT("Shaders"));
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		UE_LOG(LogFrameCapturer, Log, TEXT("Deleting project shaders from Engine/Shaders/"));
		for (int32 ShaderFileIndex = 0; ShaderFileIndex < ShaderFiles->ShaderFilePaths.Num(); ShaderFileIndex++)
		{
			FString EngineShaderFullPath = FPaths::Combine(*EngineShadersDirectory, *ShaderFiles->ShaderFilePaths[ShaderFileIndex]);

			if (PlatformFile.DeleteFile(*EngineShaderFullPath))
			{
				UE_LOG(LogFrameCapturer, Log, TEXT("Shader file %s deleted."), *EngineShaderFullPath);
			}
			else
			{
				UE_LOG(LogFrameCapturer, Warning, TEXT("Could not delete %s!"), *EngineShaderFullPath);
			}
		}

		delete ShaderFiles;
#endif
	}

	FShaderFileVisitor* ShaderFiles;
};

IMPLEMENT_MODULE(FFrameCapturerModule, FrameCapturer)
