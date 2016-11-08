#include "FrameCapturerPrivatePCH.h"
#include "SFrameCapturerImage.h"
#include "GaussianBlurShaders.h"

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

void SFrameCapturerImage::StartCaptureFrame(UWorld* World)
{
	if (World && World->IsGameWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);

			FIntPoint Size((int32)ViewportSize.X >> DownSampleNum.Get(), (int32)ViewportSize.Y >> DownSampleNum.Get());
			FillImageBrush(Size.X, Size.Y);

			bool ReadBack = BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;
			FrameCapturer = MakeUnique<FFrameCapturer>(
				ViewportClient->GetGameViewport(),
				Size,
				ReadBack,
				ReadBack ? nullptr : ImageRenderTarget2D->GetItem()->GameThread_GetRenderTargetResource()->GetRenderTargetTexture(),
				PF_B8G8R8A8,
				CaptureFrameCount.Get() == 1 ? 1 : 3);

			TWeakPtr<SFrameCapturerImage> ClosureWeakThis = SharedThis(this);
			FrameCapturer->StartCapturingFrames(CaptureFrameCount.Get(),
				[=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
				auto This = ClosureWeakThis.Pin();
				if (This.IsValid())
				{
					This->UpdateImage(RHICmdList, ColorBuffer, Texture, Width, Height);
				}
			}
			);
		}
	}
}

int32 SFrameCapturerImage::GetRemainFrameCount()
{
	if (FrameCapturer.IsValid())
	{
		return FrameCapturer->GetRemainFrameCount();
	}
	return 0;
}

int32 SFrameCapturerImage::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* ImageBrush = Image.Get();

	if ((ImageBrush != nullptr) && (ImageBrush->DrawAs != ESlateBrushDrawType::NoDrawType))
	{
		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		DrawEffects |= ESlateDrawEffect::NoGamma;

		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * ImageBrush->GetTint(InWidgetStyle));

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ImageBrush, MyClippingRect, DrawEffects, FinalColorAndOpacity);
	}
	return LayerId;
}

void SFrameCapturerImage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SImage::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (FrameCapturer.IsValid() && GetRemainFrameCount() == 0)
	{
		DestoryFrameCapturer();
	}
}

void SFrameCapturerImage::DestoryFrameCapturer()
{
	if (FrameCapturer.IsValid())
	{
		FrameCapturer.Reset();
	}
}

void SFrameCapturerImage::UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	if (BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		UpdateImageStackBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
	else if (BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::GaussianBlur_GPU)
	{
		UpdateImageGaussianBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}

	OnCaptured.ExecuteIfBound();
}

void SFrameCapturerImage::UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
{
	StackBlur(ColorBuffer, Width, Height, BlurKernel.Get(), StackBlurParallelCore.Get());

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

void SFrameCapturerImage::UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height)
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

	for (int32 i = 0; i < GaussianBlurIteratorCount.Get(); i++)
	{
		auto TargetableTexture = ResampleTexturePooledRenderTarget->GetRenderTargetItem().TargetableTexture;
		DrawGaussianBlur(RHICmdList, Texture, (FTexture2DRHIRef&)TargetableTexture, Width, Height, (BlurKernel.Get() >> DownSampleNum.Get()) / 3.0);
	}
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

void SFrameCapturerImage::SetOnCaptured(FSimpleDelegate InOnCaptured)
{
	OnCaptured = InOnCaptured;
}