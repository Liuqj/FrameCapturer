#pragma once

struct FViewportReader
{
	FViewportReader(
		EPixelFormat InPixelFormat,
		FIntPoint InBufferSize,
		bool bInReadBack,
		const FTexture2DRHIRef& InProvidedRenderTarget
		);

	~FViewportReader();

	void ResolveRenderTarget_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		const FTexture2DRHIRef& BackBufferRHI,
		const TFunction<void(FRHICommandListImmediate& RHICmdList, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback);
	
	void SetCaptureRect(FIntRect InCaptureRect);
protected:
	FTexture2DRHIRef ProvidedRenderTarget;
	FIntRect CaptureRect;
	EPixelFormat PixelFormat;
	bool bReadBack;
	uint32 TargetWidth;
	uint32 TargetHeight;
};

class FFrameCapturer : public TSharedFromThis<FFrameCapturer>
{
public:
	FFrameCapturer(
	class FSceneViewport* Viewport,
		FIntRect CaptureRect,
		FIntPoint DesiredBufferSize,
		bool bInReadBack,
		const FTexture2DRHIRef& InProvidedRenderTarget,
		EPixelFormat InPixelFormat = PF_B8G8R8A8
		);

	~FFrameCapturer();

public:
	void PrepareCapturingFrames_MainThread(uint32 InFrameCount, TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>&& InFrameReady);

	void TryCapturingThisFrames_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& BackBufferRHI);

	uint32 GetRemainFrameCount();
protected:
	void StopCapturingFrames();
	void StartCapture_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef BackBufferRHI);

private:
	FFrameCapturer(const FFrameCapturer&);
	FFrameCapturer& operator=(const FFrameCapturer&);

	TWeakPtr<SWindow> CaptureWindow;
	FRHIViewport* CacheViewportRHI;

	FViewportReader Surface;
	uint32 RemainFrameCount = 0;
	bool bReadBack = true;

	enum class EFrameGrabberState
	{
		Inactive, Active
	};
	EFrameGrabberState State = EFrameGrabberState::Inactive;
	FIntPoint TargetSize;

	TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)> FrameReady;
};