#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScreenCaptureBlueprintLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCaptureScreenDelegate, UTexture2D*, Texture);
UCLASS()
class SCREENCAPTURE_API UScreenCaptureBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "ScreenCapture")
	static void ScreenCapture(FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ScreenCapture")
	static void	CaptureScreenWithStackBlur(float ScreenScale, int32 BlurKernel, FOnCaptureScreenDelegate OnScreenCaptured, UTexture2D* InTexture = nullptr);
};
