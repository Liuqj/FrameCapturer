#pragma once

#include "SImage.h"
#include "BlurModeEnum.h"
#include "FrameCaptureTexturePool.h"

class SFrameCapturerImage : public SImage, public FGCObject
{
	SLATE_BEGIN_ARGS(SFrameCapturerImage)
		: _BlurKernel(3)
		, _DownSampleNum(1)
		, _CaptureFrameCount(1)
		, _BlurMode(EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
		, _StackBlurParallelCore(4)
		, _GaussianBlurIteratorCount(2)
	{}
		SLATE_ATTRIBUTE(int32, BlurKernel)
		SLATE_ATTRIBUTE(int32, DownSampleNum)
		SLATE_ATTRIBUTE(int32, CaptureFrameCount)
		SLATE_ATTRIBUTE(EFrameCapturerUserWidgetBlurMode, BlurMode)
		SLATE_ATTRIBUTE(int32, StackBlurParallelCore)
		SLATE_ATTRIBUTE(int32, GaussianBlurIteratorCount)
	SLATE_END_ARGS()

public:
	SFrameCapturerImage();
	virtual ~SFrameCapturerImage();
	void Construct(const FArguments& InArgs);
	void StartCaptureFrame(UWorld* World);
	UWorld* GetWorld();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	void FillImageBrush(int32 Width, int32 Height) const;
	void ClearImageBrush() const;
	void InitBrushAndFrameCapturer(const FGeometry &AllottedGeometry) const;
	void OnPaintFrameCapter(FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void OnPaintImage(bool bParentEnabled, const FWidgetStyle &InWidgetStyle, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect) const;
public:
	void SetBlurKernel(int32 InBlurKernel);
	void SetDownSampleNum(int32 InDownSampleNum);
	void SetCaptureFrameCount(int32 InCaptureFrameCount);
	void SetBlurMode(EFrameCapturerUserWidgetBlurMode InMode);
	void SetStackBlurParallelCore(int32 InStackBlurParallelCore);
	void SetGaussianBlurIteratorCount(int32 InGaussianBlurIteratorCount);
	void SetOptionalEffectBrush(FSlateBrush* InBrush);
private:
	TAttribute<int32> BlurKernel = 3;
	TAttribute<int32> DownSampleNum = 1;
	TAttribute<int32> CaptureFrameCount = 1;
	TAttribute<EFrameCapturerUserWidgetBlurMode> BlurMode = EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;
	TAttribute<int32> StackBlurParallelCore = 4;
	TAttribute<int32> GaussianBlurIteratorCount = 1;
	FSlateBrush* OptionalEffectBrush = nullptr;
	mutable TRefCountPtr<FFrameCapturePooledTexture2DItem> ImageTexture2D;
	mutable TRefCountPtr<FFrameCapturePooledRenderTarget2DItem> ImageRenderTarget2D;
	TWeakObjectPtr<UWorld> World;

	mutable UMaterialInstanceDynamic* MaterialDynamic = nullptr;

	mutable TSharedPtr<class FFrameCapturer> FrameCapturer;
	mutable bool WantInitialize = false;

public:
	static int32 CacheFrameCapturerSlateCount;
	static int32 CurrentFrameCapturerSlateCount;
};
