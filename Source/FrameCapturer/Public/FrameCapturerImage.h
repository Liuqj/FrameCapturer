#pragma once

#include "FrameCapturer.h"
#include "SFrameCapturerImage.h"
#include "Components/Image.h"
#include "FrameCapturerImage.generated.h"

UCLASS()
class FRAMECAPTURER_API UFrameCapturerImage : public UImage
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnWidgetRebuilt() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UFUNCTION(BlueprintCallable, Category = "FrameCapturer")
	void CaptureFrame();

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FrameCapturer")
	FSlateBrush OptionalEffectBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	bool ManualCapture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 BlurKernel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 DownSampleNum = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 CaptureFrameCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FrameCapturer")
	EFrameCapturerUserWidgetBlurMode BlurMode = EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 StackBlurParallelCore = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 GaussianBlurIteratorCount = 2;
private:
	TSharedPtr<SFrameCapturerImage> MyFrameCapturerImage;
};
