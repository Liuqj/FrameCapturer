#include "FrameCapturerPrivatePCH.h"
#include "FrameRecorder.h"
#include "SceneViewport.h"

FViewportReader::FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize, bool bInReadBack)
{
	AvailableEvent = nullptr;
	ReadbackTexture = nullptr;
	PixelFormat = InPixelFormat;
	bReadBack = bInReadBack;

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

void FViewportReader::Resize(uint32 InWidth, uint32 InHeight)
{
	ReadbackTexture.SafeRelease();

	Width = InWidth;
	Height = InHeight;

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		CreateCaptureFrameTexture,
		int32, InWidth, InWidth,
		int32, InHeight, InHeight,
		FViewportReader*, This, this,
		{
			FRHIResourceCreateInfo CreateInfo;
			if (This->bReadBack)
			{
				This->ReadbackTexture = RHICreateTexture2D(
					InWidth,
					InHeight,
					This->PixelFormat,
					1,
					1,
					TexCreate_CPUReadback,
					CreateInfo
					);
			}
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

void FViewportReader::ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, const TFunction<void(const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList) {
		{ 
			const FIntPoint TargetSize(Width, Height);

			FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
				TargetSize,
				PF_B8G8R8A8,
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false);

			TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
			RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("FrameRecorderResampleTexture"));
			check(ResampleTexturePooledRenderTarget);

			const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

			// DownSample
			{

				SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

				RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
				RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

				const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

				TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
				TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

				FTexture2DRHIRef SourceBackBuffer = RHICmdList.GetViewportBackBuffer(ViewportRHI);

				if (TargetSize.X != SourceBackBuffer->GetSizeX() || TargetSize.Y != SourceBackBuffer->GetSizeY())
				{
					PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceBackBuffer);
				}
				else
				{
					PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceBackBuffer);
				}

				float U = float(CaptureRect.Min.X) / float(SourceBackBuffer->GetSizeX());
				float V = float(CaptureRect.Min.Y) / float(SourceBackBuffer->GetSizeY());
				float SizeU = float(CaptureRect.Max.X) / float(SourceBackBuffer->GetSizeX()) - U;
				float SizeV = float(CaptureRect.Max.Y) / float(SourceBackBuffer->GetSizeY()) - V;

				RendererModule->DrawRectangle(
					RHICmdList,
					0, 0,									// Dest X, Y
					TargetSize.X,							// Dest Width
					TargetSize.Y,							// Dest Height
					U, V,									// Source U, V
					SizeU, SizeV,							// Source USize, VSize
					TargetSize,								// Target buffer size
					FIntPoint(1, 1),						// Source texture size
					*VertexShader,
					EDRF_Default);
			}

			// Resolve
			FIntRect Rect{ 0 , 0, TargetSize.X, TargetSize.Y };
			FTexture2DRHIRef Texture;
			TArray<FColor> ColorDataBuffer;
			if (bReadBack)
			{
				RHICmdList.CopyToResolveTarget(
					DestRenderTarget.TargetableTexture,
					ReadbackTexture,
					false,
					FResolveParams());

				RHICmdList.ReadSurfaceData(ReadbackTexture, Rect, ColorDataBuffer, FReadSurfaceDataFlags());
			}
			else
			{
				const bool bKeepOriginalSurface = false;
				RHICmdList.CopyToResolveTarget(
					DestRenderTarget.TargetableTexture,
					DestRenderTarget.ShaderResourceTexture,
					bKeepOriginalSurface,
					FResolveParams());

					Texture = (FTexture2DRHIRef&)(DestRenderTarget.ShaderResourceTexture);
			}

			Callback(ColorDataBuffer, Texture, Rect.Width(), Rect.Height());

			AvailableEvent->Trigger();
		}
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&)>, InRenderCommand, RenderCommand,
		{
			InRenderCommand(RHICmdList);
		});
}

FFrameCapturer::FFrameCapturer(FSceneViewport* Viewport, FIntPoint DesiredBufferSize, bool bInReadBack, EPixelFormat InPixelFormat, uint32 NumSurfaces)
{
	bReadBack = bInReadBack;

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
		Surfaces.Emplace(InPixelFormat, DesiredBufferSize, bReadBack);
		Surfaces.Last().Surface.SetCaptureRect(CaptureRect);
	}

	//FlushRenderingCommands();
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

	Surfaces[ThisCaptureIndex].Surface.ResolveRenderTarget(*RHIViewport, [=](const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
		OnFrameReady(ThisCaptureIndex, ColorBuffer, Texture, Width, Height);
	});

	CurrentFrameIndex = (CurrentFrameIndex + 1) % Surfaces.Num();
}

void FFrameCapturer::OnFrameReady(int32 BufferIndex, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	const FResolveSurface& Surface = Surfaces[BufferIndex];
	FCapturedFrame ResolvedFrameData(TargetSize, Surface.Marker);
	ResolvedFrameData.ColorBuffer = MoveTemp(ColorBuffer) ;
	ResolvedFrameData.ReadbackTexture = MoveTemp(Texture);
	{
		FScopeLock Lock(&CapturedFramesMutex);
		CapturedFrames.Add(MoveTemp(ResolvedFrameData));
	}
	OutstandingFrameCount.Decrement();
}