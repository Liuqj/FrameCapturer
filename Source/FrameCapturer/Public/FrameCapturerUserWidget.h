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
class FRAMECAPTURER_API UFrameCapturerUserWidget : public UUserWidget, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	// TODO: ~UFrameCapturerUserWidget trigger this£¿
	virtual void NativeDestruct() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	bool IsTickableWhenPaused() const override;


	UFUNCTION(BlueprintCallable, Category = "")
	void CaptureFrame();

private:
	void DestoryFrameCapturer();
	void HiddenWidget();
	void ShowWidget();
	void UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height);
	void UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height);
	void UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height);
	void FillImageBrush(int32 Width, int32 Height);

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
	int32 CaptureFrameCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FrameCapturer")
	EFrameCapturerUserWidgetBlurMode BlurMode = EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 StackBlurParallelCore = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameCapturer")
	int32 GaussianBlurIteratorCount = 2;

private:
	bool IsShow = false;
	TUniquePtr<class FFrameCapturer> FrameCapturer;
	static TWeakObjectPtr<UTexture2D> ShareImageTexture2D;
	static TWeakObjectPtr<UTextureRenderTarget2D> ShareImageRenderTarget2D;
	static bool SingletonFlag;
};
