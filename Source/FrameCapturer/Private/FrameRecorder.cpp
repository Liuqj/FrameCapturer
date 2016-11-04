#include "FrameCapturerPrivatePCH.h"
#include "FrameRecorder.h"
#include "SceneViewport.h"
#include "ScreenRendering.h"
#include "RenderCore.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"

FViewportReader::FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize)
{
	AvailableEvent = nullptr;
	ReadbackTexture = nullptr;
	PixelFormat = InPixelFormat;

	Resize(InBufferSize.X, InBufferSize.Y);
}

FViewportReader::~FViewportReader()
{
	BlockUntilAvailable();

	ReadbackTexture.SafeRelease();
}

void FViewportReader::Initialize()
{
	check(!AvailableEvent);
	AvailableEvent = FPlatformProcess::GetSynchEventFromPool();
}

void FViewportReader::Resize(uint32 Width, uint32 Height)
{
	ReadbackTexture.SafeRelease();

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		CreateCaptureFrameTexture,
		int32, InWidth, Width,
		int32, InHeight, Height,
		FViewportReader*, This, this,
		{
			FRHIResourceCreateInfo CreateInfo;

			This->ReadbackTexture = RHICreateTexture2D(
				InWidth,
				InHeight,
				This->PixelFormat,
				1,
				1,
				TexCreate_CPUReadback,
				CreateInfo
				);
		});
}

void FViewportReader::BlockUntilAvailable()
{
	if (AvailableEvent)
	{
		AvailableEvent->Wait(~0);

		FPlatformProcess::ReturnSynchEventToPool(AvailableEvent);
		AvailableEvent = nullptr;
	}
}

void FViewportReader::ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, TFunction<void(FColor*, int32, int32)> Callback)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList) {
		FTexture2DRHIRef SourceBackBuffer = RHICmdList.GetViewportBackBuffer(ViewportRHI);
		TArray<FColor> ColorDataBuffer;
		FIntRect Rect{ CaptureRect.Min.X , CaptureRect.Min.Y, CaptureRect.Max.X, CaptureRect.Max.Y };
		RHICmdList.ReadSurfaceData(SourceBackBuffer, Rect, ColorDataBuffer, FReadSurfaceDataFlags());

		Callback((FColor*)ColorDataBuffer.GetData(), Rect.Width(), Rect.Height());

		AvailableEvent->Trigger();
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&)>, InRenderCommand, RenderCommand,
		{
			InRenderCommand(RHICmdList);
		});
}

FFrameCapturer::FFrameCapturer(FSceneViewport* Viewport, FIntPoint DesiredBufferSize, EPixelFormat InPixelFormat, uint32 NumSurfaces)
{
	State = EFrameGrabberState::Inactive;

	TargetSize = DesiredBufferSize;

	CurrentFrameIndex = 0;

	check(NumSurfaces != 0);

	FIntRect CaptureRect(0, 0, Viewport->GetSize().X, Viewport->GetSize().Y);

	TSharedPtr<SViewport> ViewportWidget = Viewport->GetViewportWidget().Pin();
	if (ViewportWidget.IsValid())
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
		if (Window.IsValid())
		{
			CaptureWindow = Window;
			FGeometry InnerWindowGeometry = Window->GetWindowGeometryInWindow();

			FArrangedChildren JustWindow(EVisibility::Visible);
			JustWindow.AddWidget(FArrangedWidget(Window.ToSharedRef(), InnerWindowGeometry));

			FWidgetPath WidgetPath(Window.ToSharedRef(), JustWindow);
			if (WidgetPath.ExtendPathTo(FWidgetMatcher(ViewportWidget.ToSharedRef()), EVisibility::Visible))
			{
				FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(ViewportWidget.ToSharedRef()).Get(FArrangedWidget::NullWidget);

				FVector2D Position = ArrangedWidget.Geometry.AbsolutePosition;
				FVector2D Size = ArrangedWidget.Geometry.GetDrawSize();

				CaptureRect = FIntRect(
					Position.X,
					Position.Y,
					Position.X + Size.X,
					Position.Y + Size.Y);
			}
		}
	}

	Surfaces.Reserve(NumSurfaces);
	for (uint32 Index = 0; Index < NumSurfaces; ++Index)
	{
		Surfaces.Emplace(InPixelFormat, DesiredBufferSize);
		Surfaces.Last().Surface.SetCaptureRect(CaptureRect);
	}

	FlushRenderingCommands();
}

FFrameCapturer::~FFrameCapturer()
{
	if (OnWindowRendered.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().Remove(OnWindowRendered);
	}
}

void FFrameCapturer::StartCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Inactive))
	{
		return;
	}

#if PLATFORM_ANDROID
	void* ViewportResource = FSlateApplication::Get().GetRenderer()->GetViewportResource(*CaptureWindow.Pin());
	if (ViewportResource)
	{
		RHISetPendingRequestAndroidBackBuffer(*((FViewportRHIRef*)ViewportResource), true);
	}
#endif

	State = EFrameGrabberState::Active;
	OnWindowRendered = FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().AddRaw(this, &FFrameCapturer::OnSlateWindowRendered);
}

void FFrameCapturer::CaptureThisFrame(FFrameMarkerPtr Marker)
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}

	OutstandingFrameCount.Increment();

	FScopeLock Lock(&PendingFrameMarkersMutex);
	PendingFrameMarkers.Add(Marker);
}

void FFrameCapturer::StopCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}
#if PLATFORM_ANDROID
	void* ViewportResource = FSlateApplication::Get().GetRenderer()->GetViewportResource(*CaptureWindow.Pin());
	if (ViewportResource)
	{
		RHISetPendingRequestAndroidBackBuffer(*((FViewportRHIRef*)ViewportResource), false);
	}
#endif

	State = EFrameGrabberState::PendingShutdown;
}

void FFrameCapturer::Shutdown()
{
	State = EFrameGrabberState::Inactive;

	for (FResolveSurface& Surface : Surfaces)
	{
		Surface.Surface.BlockUntilAvailable();
	}

	FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().Remove(OnWindowRendered);
	OnWindowRendered = FDelegateHandle();

#if PLATFORM_ANDROID
	void* ViewportResource = FSlateApplication::Get().GetRenderer()->GetViewportResource(*CaptureWindow.Pin());
	if (ViewportResource)
	{
		RHISetPendingRequestAndroidBackBuffer(*((FViewportRHIRef*)ViewportResource), false);
	}
#endif
}

bool FFrameCapturer::HasOutstandingFrames() const
{
	FScopeLock Lock(&CapturedFramesMutex);

	return OutstandingFrameCount.GetValue() || CapturedFrames.Num();
}

TArray<FCapturedFrame> FFrameCapturer::GetCapturedFrames()
{
	TArray<FCapturedFrame> ReturnFrames;

	bool bShouldStop = false;
	{
		FScopeLock Lock(&CapturedFramesMutex);
		Swap(ReturnFrames, CapturedFrames);
		CapturedFrames.Reset();

		if (State == EFrameGrabberState::PendingShutdown)
		{
			bShouldStop = OutstandingFrameCount.GetValue() == 0;
		}
	}

	if (bShouldStop)
	{
		Shutdown();
	}

	return ReturnFrames;
}

void FFrameCapturer::OnSlateWindowRendered(SWindow& SlateWindow, void* ViewportRHIPtr)
{
	TSharedPtr<SWindow> Window = CaptureWindow.Pin();
	if (Window.Get() != &SlateWindow)
	{
		return;
	}

	const FViewportRHIRef* RHIViewport = (const FViewportRHIRef*)ViewportRHIPtr;

#if PLATFORM_ANDROID
	if (!RHIIsRequestAndroidBackBuffer(*RHIViewport))
	{
		return;
	}
#endif

	FFrameMarkerPtr Marker;
	{
		FScopeLock Lock(&PendingFrameMarkersMutex);
		if (!PendingFrameMarkers.Num())
		{
			return;
		}
		Marker = PendingFrameMarkers[0];
		PendingFrameMarkers.RemoveAt(0, 1, false);
	}

	const int32 ThisCaptureIndex = CurrentFrameIndex;

	FResolveSurface* ThisFrameTarget = &Surfaces[ThisCaptureIndex];
	ThisFrameTarget->Surface.BlockUntilAvailable();

	ThisFrameTarget->Surface.Initialize();
	ThisFrameTarget->Marker = Marker;

	Surfaces[ThisCaptureIndex].Surface.ResolveRenderTarget(*RHIViewport, [=](FColor* ColorBuffer, int32 Width, int32 Height) {
		OnFrameReady(ThisCaptureIndex, ColorBuffer, Width, Height);
	});

	CurrentFrameIndex = (CurrentFrameIndex + 1) % Surfaces.Num();
}

void FFrameCapturer::OnFrameReady(int32 BufferIndex, FColor* ColorBuffer, int32 Width, int32 Height)
{
	if (!ensure(ColorBuffer))
	{
		OutstandingFrameCount.Decrement();
		return;
	}

	const FResolveSurface& Surface = Surfaces[BufferIndex];

	FCapturedFrame ResolvedFrameData(TargetSize, Surface.Marker);

	ResolvedFrameData.ColorBuffer.InsertUninitialized(0, TargetSize.X * TargetSize.Y);
	FColor* Dest = &ResolvedFrameData.ColorBuffer[0];

	const int32 MaxWidth = FMath::Min(TargetSize.X, Width);
	for (int32 Row = 0; Row < FMath::Min(Height, TargetSize.Y); ++Row)
	{
		FMemory::Memcpy(Dest, ColorBuffer, sizeof(FColor)*MaxWidth);
		ColorBuffer += Width;
		Dest += MaxWidth;
	}

	{
		FScopeLock Lock(&CapturedFramesMutex);
		CapturedFrames.Add(MoveTemp(ResolvedFrameData));
	}
	OutstandingFrameCount.Decrement();
}