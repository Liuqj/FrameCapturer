#include "ScreenCapturePrivatePCH.h"

void UScreenCaptureBlueprintLibrary::ScreenCapture(FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture)
{
	FScreenCaptureModule::Get().ScreenCapture(OnScreenCaptured, InTexture);
}

void UScreenCaptureBlueprintLibrary::CaptureScreenWithStackBlur(float ScreenScale, int32 BlurKernel, FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture) 
{
	FScreenCaptureModule::Get().ScreenCaptureWithStackBlur(ScreenScale, BlurKernel, OnScreenCaptured, InTexture);
}
