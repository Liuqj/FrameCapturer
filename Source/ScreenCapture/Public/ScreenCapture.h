#pragma once

#include "ModuleInterface.h"
#include "ScreenCaptureBlueprintLibrary.h"

class SCREENCAPTURE_API FScreenCaptureModule : public IModuleInterface, FTickableGameObject
{
public:
	static inline FScreenCaptureModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FScreenCaptureModule >("ScreenCapture");
	}
	void ScreenCaptureWithStackBlur(float ScreenScale, int32 BlurKernel, FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture = nullptr);
	void ScreenCapture(FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture = nullptr);

private:
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	void OnSlateWindowRendered_ScreenCaptureWithStackBlur(SWindow& SlateWindow, void* ViewportRHIPtr, float ScreenScale, FDelegateHandle* DelegateHandle, int32 BlurKernel, FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture, uint64 InCaptureCount);
	void OnSlateWindowRendered_ScreenCapture(SWindow& SlateWindow, void* ViewportRHIPtr, FDelegateHandle* DelegateHandle, FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture, uint64 InCaptureCount);
	void StackBlur(void* InSrc, uint32 Width, uint32 Height, uint32 Radius, int32 MaxCore);

	uint64 CaptureCount = 0;

	TQueue<TFunction<void()>> GameThreadTasks;
};