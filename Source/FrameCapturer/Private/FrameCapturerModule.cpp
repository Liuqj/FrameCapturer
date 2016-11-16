#include "FrameCapturerPrivatePCH.h"
#include "ShaderFileVisitor.h"
#include "FrameCapturerSettings.h"

DEFINE_LOG_CATEGORY(LogFrameCapturer);

#define LOCTEXT_NAMESPACE "FrameCapturer"

class FRAMECAPTURER_API FFrameCapturerModule : public IModuleInterface
{
public:
	static inline FFrameCapturerModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FFrameCapturerModule >("FrameCapturer");
	}

	virtual void StartupModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "FrameCapturer",
				LOCTEXT("RuntimeSettingsName", "FrameCapturer"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the FrameCapturer plugin"),
				GetMutableDefault<UFrameCapturerSettings>()
				);
		}

		SlateImageDelegateHandle = FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().AddLambda([](SWindow&, void*) {
			SFrameCapturerImage::CacheFrameCapturerSlateCount = SFrameCapturerImage::CurrentFrameCapturerSlateCount;
			SFrameCapturerImage::CurrentFrameCapturerSlateCount = 0;
			
		});


#if ExperimentalGPUBlur
		FString PluginShadersDirectory = FPaths::Combine(*FPaths::EnginePluginsDir(), TEXT("/RealGameLab/FrameCapturer/Shaders"));
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
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().Remove(SlateImageDelegateHandle);
		}

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

	FDelegateHandle SlateImageDelegateHandle;
	FShaderFileVisitor* ShaderFiles;
};
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFrameCapturerModule, FrameCapturer)
