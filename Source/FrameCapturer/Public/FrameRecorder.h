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
	void Initialize();
	void BlockUntilAvailable();
	void ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, const TFunction<void(FRHICommandListImmediate& RHICmdList, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback);
	void SetCaptureRect(FIntRect InCaptureRect) { CaptureRect = InCaptureRect; }
	void Resize(uint32 Width, uint32 Height);
protected:
	FEvent* AvailableEvent;
	FTexture2DRHIRef ProvidedRenderTarget;
	FIntRect CaptureRect;
	EPixelFormat PixelFormat;
	bool bReadBack;
	uint32 Width;
	uint32 Height;
};

class FFrameCapturer
{
public:
	FFrameCapturer(
	class FSceneViewport* Viewport,
		FIntPoint DesiredBufferSize,
		bool bInReadBack,
		const FTexture2DRHIRef& InProvidedRenderTarget,
		EPixelFormat InPixelFormat = PF_B8G8R8A8,
		uint32 NumSurfaces = 2
		);
	~FFrameCapturer();

public:
	void StartCapturingFrames(uint32 InFrameCount, TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>&& InFrameReady);
	void StopCapturingFrames();
	uint32 GetRemainFrameCount();
protected:
	void OnSlateWindowRendered( SWindow& SlateWindow, void* ViewportRHIPtr );
	void OnFrameReady_RenderThread(FRHICommandListImmediate& RHICmdList, int32 SurfaceIndex, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height);

private:
	FFrameCapturer(const FFrameCapturer&);
	FFrameCapturer& operator=(const FFrameCapturer&);

	TWeakPtr<SWindow> CaptureWindow;
	FDelegateHandle OnWindowRendered;
	mutable FCriticalSection CapturedFramesMutex;

	struct FResolveSurface
	{
		FResolveSurface(
			EPixelFormat InPixelFormat,
			FIntPoint BufferSize,
			bool bInReadBack,
			const FTexture2DRHIRef& InProvidedRenderTarget
			)
			: Surface(InPixelFormat, BufferSize, bInReadBack, InProvidedRenderTarget) {}
		FViewportReader Surface;
	};
	TArray<FResolveSurface> Surfaces;
	int32 CurrentFrameIndex = 0;
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