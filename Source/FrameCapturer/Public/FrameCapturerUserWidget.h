#pragma once

#include "FrameRecorder.h"
#include "FrameCapturerUserWidget.generated.h"

UCLASS()
class FRAMECAPTURER_API UFrameCapturerUserWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "")
	void CaptureFrame();

private:
	void CheckCaptureTimer();
	void DestoryFrameCapturer();
	void HiddenWidget();
	void ShowWidget();
	void UpdateImage(const FCapturedFrame& CapturedFrame);
	void FillImageBrush(const FCapturedFrame &CapturedFrame);

public:
	UPROPERTY(meta = (BindWidget))
	class UImage* Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool ManualCapture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 BlurKernel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 DownSampleNum = 1;

	FTimerHandle TimerHandle;
	TUniquePtr<class FFrameCapturer> FrameCapturer;
	static TWeakObjectPtr<UTexture2D> ShareImageTexture2D;
	static bool SingletonFlag;
};
