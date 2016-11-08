#include "FrameCapturerPrivatePCH.h"
#include "FrameRecorder.h"
#include "SceneViewport.h"

FViewportReader::FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize, bool bInReadBack, const FTexture2DRHIRef& InProvidedRenderTarget)
{
	AvailableEvent = nullptr;
	PixelFormat = InPixelFormat;
	bReadBack = bInReadBack;
	ProvidedRenderTarget = InProvidedRenderTarget;

	Resize(InBufferSize.X, InBufferSize.Y);
}


FViewportReader::~FViewportReader()
{
	if (AvailableEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(AvailableEvent);
	}
}

void FViewportReader::Initialize()
{
	check(!AvailableEvent);
	AvailableEvent = FPlatformProcess::GetSynchEventFromPool();
}

void FViewportReader::Resize(uint32 InWidth, uint32 InHeight)
{
	Width = InWidth;
	Height = InHeight;
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

void FViewportReader::ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, const TFunction<void(FRHICommandListImmediate& RHICmdList, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList) {
		{ 
			const FIntPoint TargetSize(Width, Height);
			FTexture2DRHIRef DownSampleTarget;
			if (!ProvidedRenderTarget.IsValid())
			{
				FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
					TargetSize,
					PF_B8G8R8A8,
					FClearValueBinding::None,
					TexCreate_None,
					TexCreate_RenderTargetable,
					false);

				TRefCountPtr<IPooledRenderTarget> FrameRecorderDownSample;
				RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, FrameRecorderDownSample, TEXT("FrameRecorderDownSample"));
				check(FrameRecorderDownSample);

				const FSceneRenderTargetItem& DestRenderTarget = FrameRecorderDownSample->GetRenderTargetItem();
				DownSampleTarget = (FTexture2DRHIRef&)DestRenderTarget.TargetableTexture;
			}
			else
			{
				DownSampleTarget = ProvidedRenderTarget;
			}

			// DownSample
			{
				SetRenderTarget(RHICmdList, DownSampleTarget, FTextureRHIRef());

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
				RHICmdList.ReadSurfaceData(DownSampleTarget, Rect, ColorDataBuffer, FReadSurfaceDataFlags());
			}

			Callback(RHICmdList, ColorDataBuffer, DownSampleTarget, Rect.Width(), Rect.Height());

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

FFrameCapturer::FFrameCapturer(FSceneViewport* Viewport, FIntPoint DesiredBufferSize, bool bInReadBack, const FTexture2DRHIRef& InProvidedRenderTarget, EPixelFormat InPixelFormat /*= PF_B8G8R8A8*/, uint32 NumSurfaces /*= 2 */)
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
		Surfaces.Emplace(InPixelFormat, DesiredBufferSize, bReadBack, InProvidedRenderTarget);
		Surfaces.Last().Surface.SetCaptureRect(CaptureRect);
	}
}

FFrameCapturer::~FFrameCapturer()
{
	StopCapturingFrames();
}

void FFrameCapturer::StartCapturingFrames(uint32 InFrameCount, TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>&& InFrameReady)
{
	if (!ensure(State == EFrameGrabberState::Inactive))
	{
		return;
	}

	RemainFrameCount = InFrameCount;
	FrameReady = InFrameReady;

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

void FFrameCapturer::StopCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}

	for (FResolveSurface& Surface : Surfaces)
	{
		Surface.Surface.BlockUntilAvailable();
	}

	RemainFrameCount = 0;
	State = EFrameGrabberState::Inactive;

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

uint32 FFrameCapturer::GetRemainFrameCount()
{
	return RemainFrameCount;
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

	if (RemainFrameCount == 0)
	{
		return;
	}

	RemainFrameCount--;

	const int32 ThisCaptureIndex = CurrentFrameIndex;

	FResolveSurface* ThisFrameTarget = &Surfaces[ThisCaptureIndex];
	ThisFrameTarget->Surface.BlockUntilAvailable();
	ThisFrameTarget->Surface.Initialize();

	Surfaces[ThisCaptureIndex].Surface.ResolveRenderTarget(*RHIViewport, [=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
		OnFrameReady_RenderThread(RHICmdList, ThisCaptureIndex, ColorBuffer, Texture, Width, Height);
	});

	CurrentFrameIndex = (CurrentFrameIndex + 1) % Surfaces.Num();
}

void FFrameCapturer::OnFrameReady_RenderThread(FRHICommandListImmediate& RHICmdList, int32 BufferIndex, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	FrameReady(RHICmdList, ColorBuffer, Texture, Width, Height);
}