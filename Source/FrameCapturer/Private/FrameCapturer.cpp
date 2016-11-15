#include "FrameCapturerPrivatePCH.h"
#include "FrameCapturer.h"
#include "SceneViewport.h"
#include "UnrealTemplate.h"

FViewportReader::FViewportReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize, bool bInReadBack, const FTexture2DRHIRef& InProvidedRenderTarget)
{
	PixelFormat = InPixelFormat;
	bReadBack = bInReadBack;
	ProvidedRenderTarget = InProvidedRenderTarget;

	TargetWidth = InBufferSize.X;
	TargetHeight = InBufferSize.Y;
}


FViewportReader::~FViewportReader()
{
}

void FViewportReader::ResolveRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& BackBufferRHI, const TFunction<void(FRHICommandListImmediate& RHICmdList, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>& Callback)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	const FIntPoint TargetSize(TargetWidth, TargetHeight);
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

		if (TargetSize.X != BackBufferRHI->GetSizeX() || TargetSize.Y != BackBufferRHI->GetSizeY())
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), BackBufferRHI);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), BackBufferRHI);
		}

		float U = float(CaptureRect.Min.X) / float(BackBufferRHI->GetSizeX());
		float V = float(CaptureRect.Min.Y) / float(BackBufferRHI->GetSizeY());
		float SizeU = float(CaptureRect.Max.X) / float(BackBufferRHI->GetSizeX()) - U;
		float SizeV = float(CaptureRect.Max.Y) / float(BackBufferRHI->GetSizeY()) - V;

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
}

void FViewportReader::SetCaptureRect(FIntRect InCaptureRect)
{
	CaptureRect = InCaptureRect;
}

FFrameCapturer::FFrameCapturer(
class FSceneViewport* Viewport, 
	FIntRect CaptureRect,
	FIntPoint DesiredBufferSize,
	bool bInReadBack,
	const FTexture2DRHIRef& InProvidedRenderTarget,
	EPixelFormat InPixelFormat /*= PF_B8G8R8A8 */)
	: Surface(InPixelFormat, DesiredBufferSize, bInReadBack, InProvidedRenderTarget)
{
	bReadBack = bInReadBack;
	State = EFrameGrabberState::Inactive;

	TargetSize = DesiredBufferSize;

	TSharedPtr<SViewport> ViewportWidget = Viewport->GetViewportWidget().Pin();
	if (ViewportWidget.IsValid())
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
		if (Window.IsValid())
		{
			CaptureWindow = Window;
		}
	}

	Surface.SetCaptureRect(CaptureRect);
}

FFrameCapturer::~FFrameCapturer()
{
	StopCapturingFrames();
}

void FFrameCapturer::PrepareCapturingFrames_MainThread(uint32 InFrameCount, TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&, const FTexture2DRHIRef&, int32, int32)>&& InFrameReady)
{
	if (!ensure(State == EFrameGrabberState::Inactive))
	{
		return;
	}

	RemainFrameCount = InFrameCount;
	FrameReady = InFrameReady;

	{
		void* ViewportResource = FSlateApplication::Get().GetRenderer()->GetViewportResource(*CaptureWindow.Pin());
		if (ViewportResource)
		{
#if PLATFORM_ANDROID
			CacheViewportRHI = (*(FViewportRHIRef*)ViewportResource).GetReference();
			RHISetPendingRequestAndroidBackBuffer(CacheViewportRHI, true);
#endif
		}
	}

	State = EFrameGrabberState::Active;
}

void FFrameCapturer::TryCapturingThisFrames_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& BackBufferRHI)
{
	if (State != EFrameGrabberState::Active)
	{
		return;
	}
	StartCapture_RenderThread(RHICmdList, BackBufferRHI);
}

void FFrameCapturer::StopCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}

	RemainFrameCount = 0;

	State = EFrameGrabberState::Inactive;

#if PLATFORM_ANDROID
	void* ViewportResource = FSlateApplication::Get().GetRenderer()->GetViewportResource(*CaptureWindow.Pin());
	if (ViewportResource)
	{
		RHISetPendingRequestAndroidBackBuffer((*(FViewportRHIRef*)ViewportResource).GetReference(), false);
	}
#endif
}

uint32 FFrameCapturer::GetRemainFrameCount()
{
	return RemainFrameCount;
}

void FFrameCapturer::StartCapture_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef BackBufferRHI)
{
#if PLATFORM_ANDROID
	if (!RHIIsRequestAndroidBackBuffer(CacheViewportRHI))
	{
		return;
	}
#endif
	if (RemainFrameCount == 0)
	{
		return;
	}

	RemainFrameCount--;

	Surface.ResolveRenderTarget_RenderThread(
		RHICmdList,
		BackBufferRHI,
		[=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
		FrameReady(RHICmdList, ColorBuffer, Texture, Width, Height);
	});
}
