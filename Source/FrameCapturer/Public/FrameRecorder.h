#pragma once

struct FViewportReader
{
	FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize, bool bInReadBack);
	~FViewportReader();
	void Initialize();
	void BlockUntilAvailable();
	void ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, const TFunction<void(const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback);
	void SetCaptureRect(FIntRect InCaptureRect) { CaptureRect = InCaptureRect; }
	void Resize(uint32 Width, uint32 Height);
protected:
	FThreadSafeBool bEnabled;
	FEvent* AvailableEvent;
	FTexture2DRHIRef ReadbackTexture;
	FIntRect CaptureRect;
	EPixelFormat PixelFormat;
	bool bIsEnabled;
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

	FCapturedFrame(FCapturedFrame&& In) : ColorBuffer(MoveTemp(In.ColorBuffer)), ReadbackTexture(MoveTemp(In.ReadbackTexture)), BufferSize(In.BufferSize), Marker(MoveTemp(In.Marker)) {}
	FCapturedFrame& operator=(FCapturedFrame&& In) { ColorBuffer = MoveTemp(In.ColorBuffer); ReadbackTexture = (MoveTemp(In.ReadbackTexture)); BufferSize = In.BufferSize; Marker = MoveTemp(In.Marker); return *this; }

	template<typename T>
	T* GetMarker() { return static_cast<T*>(Marker.Get()); }

	TArray<FColor> ColorBuffer;
	FTexture2DRHIRef ReadbackTexture;
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
	void StartCapturingFrames();
	void CaptureThisFrame(FFrameMarkerPtr Marker);
	void StopCapturingFrames();
	void Shutdown();
public:
	bool HasOutstandingFrames() const;
	TArray<FCapturedFrame> GetCapturedFrames();

protected:
	void OnSlateWindowRendered( SWindow& SlateWindow, void* ViewportRHIPtr );
	void OnFrameReady(int32 SurfaceIndex, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height);

private:
	FFrameCapturer(const FFrameCapturer&);
	FFrameCapturer& operator=(const FFrameCapturer&);

	TWeakPtr<SWindow> CaptureWindow;
	FDelegateHandle OnWindowRendered;
	TArray<FCapturedFrame> CapturedFrames;
	mutable FCriticalSection CapturedFramesMutex;

	struct FResolveSurface
	{
		FResolveSurface(EPixelFormat InPixelFormat, FIntPoint BufferSize, bool bReadBack) : Surface(InPixelFormat, BufferSize, bReadBack) {}
		FFrameMarkerPtr Marker;
		FViewportReader Surface;
	};
	TArray<FResolveSurface> Surfaces;
	int32 CurrentFrameIndex;
	int32 SlateRenderIndex;
	FThreadSafeCounter OutstandingFrameCount;
	FCriticalSection PendingFrameMarkersMutex;
	TArray<FFrameMarkerPtr> PendingFrameMarkers;
	bool bReadBack;
	enum class EFrameGrabberState
	{
		Inactive, Active, PendingShutdown
	};
	EFrameGrabberState State;
	FIntPoint TargetSize;
};