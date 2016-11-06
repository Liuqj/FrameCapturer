#pragma once

#include "FrameRecorder.h"
#include "FrameCapturerUserWidget.generated.h"

UENUM()
enum class EFrameCapturerUserWidgetBlurMode : uint8
{
	StackBlur_CPU,
	GaussianBlur_GPU,
}; 

UCLASS()
class FRAMECAPTURER_API UFrameCapturerUserWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	// TODO: ~UFrameCapturerUserWidget trigger this£¿
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "")
	void CaptureFrame();

private:
	void CheckCaptureTimer();
	void DestoryFrameCapturer();
	void HiddenWidget();
	void ShowWidget();
	void UpdateImage(const FCapturedFrame& CapturedFrame);
	void FillImageBrush(const FIntPoint &BufferSize);

public:
	UPROPERTY(meta = (BindWidget))
	class UImage* Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	bool ManualCapture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 BlurKernel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 DownSampleNum = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	EFrameCapturerUserWidgetBlurMode BlurMode = EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 StackBlurParallelCore = 4;

private:
	FTimerHandle TimerHandle;
	TUniquePtr<class FFrameCapturer> FrameCapturer;
	static TWeakObjectPtr<UTexture2D> ShareImageTexture2D;
	static bool SingletonFlag;
};
