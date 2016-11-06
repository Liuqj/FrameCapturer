#pragma once

struct FViewportReader
{
	FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize, bool bInReadBack);
	~FViewportReader();
	void Initialize();
	void BlockUntilAvailable();
	void ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, const TFunction<void(FRHICommandListImmediate& RHICmdList, const TArray<FColor>&, const TRefCountPtr<IPooledRenderTarget>&, int32, int32)>& Callback);
	void SetCaptureRect(FIntRect InCaptureRect) { CaptureRect = InCaptureRect; }
	void Resize(uint32 Width, uint32 Height);
protected:
	FEvent* AvailableEvent;
	FIntRect CaptureRect;
	EPixelFormat PixelFormat;
	bool bReadBack;
	uint32 Width;
	uint32 Height;
};

struct IFrameMarker
{
	virtual ~IFrameMarker() {}
};

typedef TSharedPtr<IFrameMarker, ESPMode::ThreadSafe> FFrameMarkerPtr;

struct FCapturedFrame
{
	FCapturedFrame(FIntPoint InBufferSize, FFrameMarkerPtr InMarker) : BufferSize(InBufferSize), Marker(MoveTemp(InMarker)) {}

	FCapturedFrame(FCapturedFrame&& In) : ColorBuffer(MoveTemp(In.ColorBuffer)), BufferSize(In.BufferSize), Marker(MoveTemp(In.Marker)) {}
	FCapturedFrame& operator=(FCapturedFrame&& In) { ColorBuffer = MoveTemp(In.ColorBuffer); BufferSize = In.BufferSize; Marker = MoveTemp(In.Marker); return *this; }

	template<typename T>
	T* GetMarker() { return static_cast<T*>(Marker.Get()); }

	TArray<FColor> ColorBuffer;
	FIntPoint BufferSize;
	FFrameMarkerPtr Marker;
	
private:
	FCapturedFrame(const FCapturedFrame& In);
	FCapturedFrame& operator=(const FCapturedFrame&);
};

class FFrameCapturer
{
public:
	FFrameCapturer(class FSceneViewport* Viewport, FIntPoint DesiredBufferSize, bool bInReadBack, EPixelFormat InPixelFormat = PF_B8G8R8A8, uint32 NumSurfaces = 2);
	~FFrameCapturer();

public:
	void StartCapturingFrames(uint32 InFrameCount, TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const TRefCountPtr<IPooledRenderTarget>&, int32, int32)>&& InFrameReady);
	void StopCapturingFrames();
	uint32 GetRemainFrameCount();
protected:
	void OnSlateWindowRendered( SWindow& SlateWindow, void* ViewportRHIPtr );
	void OnFrameReady_RenderThread(FRHICommandListImmediate& RHICmdList, int32 SurfaceIndex, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height);

private:
	FFrameCapturer(const FFrameCapturer&);
	FFrameCapturer& operator=(const FFrameCapturer&);

	TWeakPtr<SWindow> CaptureWindow;
	FDelegateHandle OnWindowRendered;
	TArray<FCapturedFrame> CapturedFrames;
	mutable FCriticalSection CapturedFramesMutex;

	struct FResolveSurface
	{
		FResolveSurface(EPixelFormat InPixelFormat, FIntPoint BufferSize, bool bInReadBack) : Surface(InPixelFormat, BufferSize, bInReadBack) {}
		FFrameMarkerPtr Marker;
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

	TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const TRefCountPtr<IPooledRenderTarget>&, int32, int32)> FrameReady;
};