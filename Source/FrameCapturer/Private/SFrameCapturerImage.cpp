#include "FrameCapturerPrivatePCH.h"
#include "SFrameCapturerImage.h"
#include "GaussianBlurShaders.h"
#include "FrameCapturer.h"
#include "StackBlur.h"

void SFrameCapturerImageProxy::UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	if (BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		UpdateImageStackBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
	else
	{
		UpdateImageGaussianBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
}

void SFrameCapturerImageProxy::UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	StackBlur(ColorBuffer, Width, Height, BlurKernel, StackBlurParallelCore);

	FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, Width, Height);
	{
		auto Resource = (FTexture2DResource*)(ImageTexture2D->GetItem()->Resource);

		RHICmdList.UpdateTexture2D(
			Resource->GetTexture2DRHI(),
			0,
			UpdateRegion,
			sizeof(FColor) * Width,
			(uint8*)ColorBuffer.GetData()
			);
	}
}

void SFrameCapturerImageProxy::UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
		FIntPoint(Width, Height),
		PF_B8G8R8A8,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_RenderTargetable,
		false);

	TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
	RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("FrameRecorderGaussianTemp"));
	check(ResampleTexturePooledRenderTarget);

	for (int32 i = 0; i < GaussianBlurIteratorCount; i++)
	{
		auto TargetableTexture = ResampleTexturePooledRenderTarget->GetRenderTargetItem().TargetableTexture;
		DrawGaussianBlur(RHICmdList, Texture, (FTexture2DRHIRef&)TargetableTexture, Width, Height, (BlurKernel >> DownSampleNum) / 3.0);
	}
	SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}

//////////////////////////////////////////////////////////////////////////

class FScreenCaptureElement : public ICustomSlateElement
{
public:
	FScreenCaptureElement(){}
	~FScreenCaptureElement() {}
	void SetFrameCapturer(TSharedPtr<FFrameCapturer> InFrameCapturer);
private:
	virtual void DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer) override;



private:
	TWeakPtr<FFrameCapturer> FrameCapturer;
	SFrameCapturerImageProxy ProxyData;
};


void FScreenCaptureElement::SetFrameCapturer(TSharedPtr<FFrameCapturer> InFrameCapturer)
{
	FrameCapturer = InFrameCapturer;
}

void FScreenCaptureElement::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	FTexture2DRHIRef& RT = *(FTexture2DRHIRef*)InWindowBackBuffer;

	auto FrameCapturerPtr = FrameCapturer.Pin();
	if (FrameCapturerPtr.IsValid())
	{
		FrameCapturerPtr->TryCapturingThisFrames_RenderThread(RHICmdList, RT);
	}

	SetRenderTarget(RHICmdList, RT, FTextureRHIRef());
}

//////////////////////////////////////////////////////////////////////////


SFrameCapturerImage::SFrameCapturerImage()
{
	bCanTick = true;
}

SFrameCapturerImage::~SFrameCapturerImage()
{
	// Test Leak this pointer
}

void SFrameCapturerImage::Construct(const FArguments& InArgs)
{
	BlurKernel = InArgs._BlurKernel;
	DownSampleNum = InArgs._DownSampleNum;
	BlurMode = InArgs._BlurMode;
	StackBlurParallelCore = InArgs._StackBlurParallelCore;
	GaussianBlurIteratorCount = InArgs._GaussianBlurIteratorCount;
}

void SFrameCapturerImage::StartCaptureFrame(UWorld* InWorld)
{
	World = InWorld;
	WillCapture = true;
}

int32 SFrameCapturerImage::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (WillCapture)
	{
		// if world perpetuation
		if (World.IsValid() && World->IsGameWorld())
		{
			if (UGameViewportClient* ViewportClient = World->GetGameViewport())
			{
				FVector2D ViewportSize;

				ViewportClient->GetViewportSize(ViewportSize);

				FIntPoint TargetSize((int32)ViewportSize.X >> DownSampleNum.Get(), (int32)ViewportSize.Y >> DownSampleNum.Get());

				SFrameCapturerImage* This = (SFrameCapturerImage*)this;

				This->FillImageBrush(TargetSize.X, TargetSize.Y);

				bool ReadBack = BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;
				
				FVector2D Size = AllottedGeometry.GetDrawSize();
				FIntRect CaptureRect
				{
					(int32)AllottedGeometry.AbsolutePosition.X,
					(int32)AllottedGeometry.AbsolutePosition.Y,
					(int32)(AllottedGeometry.AbsolutePosition.X + Size.X),
					(int32)(AllottedGeometry.AbsolutePosition.Y + Size.Y)
				};

				FrameCapturer = MakeShareable<FFrameCapturer>(new FFrameCapturer(
					ViewportClient->GetGameViewport(),
					CaptureRect,
					TargetSize,
					ReadBack,
					ReadBack ? nullptr : ImageRenderTarget2D->GetItem()->GameThread_GetRenderTargetResource()->GetRenderTargetTexture(),
					PF_B8G8R8A8
					));

				StaticCastSharedPtr<FScreenCaptureElement>(Drawer)->SetFrameCapturer(FrameCapturer);

				TWeakPtr<SFrameCapturerImage> ClosureWeakThis = SharedThis(This);

				FrameCapturer->PrepareCapturingFrames_MainThread(CaptureFrameCount.Get(),
					[=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
					auto This = ClosureWeakThis.Pin();
					if (This.IsValid())
					{
						SFrameCapturerImageProxy Proxy
						{
							BlurKernel.Get(),
							DownSampleNum.Get(),
						CaptureFrameCount.Get(),
							BlurMode.Get(),
							StackBlurParallelCore.Get(),
							GaussianBlurIteratorCount.Get(),
							ImageTexture2D,
							ImageRenderTarget2D
						};
						Proxy.UpdateImage(RHICmdList, ColorBuffer, Texture, Width, Height);
					}
				}
				);
			}
		}
		WillCapture = false;
	}

	if (FrameCapturer.IsValid())
	{
		FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, Drawer);
	}

	const FSlateBrush* ImageBrush = Image.Get();

	if ((ImageBrush != nullptr) && (ImageBrush->DrawAs != ESlateBrushDrawType::NoDrawType))
	{
		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		if (BlurMode.Get() != EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
		{
			DrawEffects |= ESlateDrawEffect::NoGamma;
		}

		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * ImageBrush->GetTint(InWidgetStyle));

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ImageBrush, MyClippingRect, DrawEffects, FinalColorAndOpacity);
	}
	return LayerId;
}

void SFrameCapturerImage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SImage::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (FrameCapturer.IsValid())
	{
		if (FrameCapturer->GetRemainFrameCount() == 0)
		{
			FrameCapturer.Reset();
		}
	}
}

UWorld* SFrameCapturerImage::GetWorld()
{
	return World.Get();
}

void SFrameCapturerImage::FillImageBrush(int32 Width, int32 Height)
{
	FSlateBrush* NoConstImage = (FSlateBrush*)Image.Get();
	NoConstImage->Tiling = ESlateBrushTileType::NoTile;
	if (BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		ImageTexture2D = FFrameCaptureTexturePool::Get().GetTexture2D(Width, Height);
		NoConstImage->SetResourceObject(ImageTexture2D->GetItem());
	}
	else
	{
		ImageRenderTarget2D = FFrameCaptureTexturePool::Get().GetRenderTarget2D(Width, Height);
		NoConstImage->SetResourceObject(ImageRenderTarget2D->GetItem());
	}
	FlushRenderingCommands();
}

void SFrameCapturerImage::SetBlurKernel(int32 InBlurKernel)
{
	BlurKernel = InBlurKernel;
}

void SFrameCapturerImage::SetDownSampleNum(int32 InDownSampleNum)
{
	DownSampleNum = InDownSampleNum;
}

void SFrameCapturerImage::SetCaptureFrameCount(int32 InCaptureFrameCount)
{
	CaptureFrameCount = InCaptureFrameCount;
}

void SFrameCapturerImage::SetBlurMode(EFrameCapturerUserWidgetBlurMode InMode)
{
	BlurMode = InMode;
}

void SFrameCapturerImage::SetStackBlurParallelCore(int32 InStackBlurParallelCore)
{
	StackBlurParallelCore = InStackBlurParallelCore;
}

void SFrameCapturerImage::SetGaussianBlurIteratorCount(int32 InGaussianBlurIteratorCount)
{
	GaussianBlurIteratorCount = InGaussianBlurIteratorCount;
}

TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> SFrameCapturerImage::Drawer = MakeShared<FScreenCaptureElement, ESPMode::ThreadSafe>();
