#include "FrameCapturerPrivatePCH.h"
#include "FrameCapturerUserWidget.h"
#include "FrameRecorder.h"
#include "StackBlur.h"
#include "GaussianBlurShaders.h"

void UFrameCapturerUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SingletonFlag)
	{
		UE_LOG(LogFrameCapturer, Error, TEXT("FrameCapturerUserWidget twice open"));
	}
	SingletonFlag = true;

	IsShow = true;

	HiddenWidget();

	if (!ManualCapture)
	{
		CaptureFrame();
	}
}

void UFrameCapturerUserWidget::NativeDestruct()
{
	Super::NativeDestruct();
	Image->SetBrush(FSlateBrush());
	DestoryFrameCapturer();
	IsShow = false;
	SingletonFlag = false;
}

void UFrameCapturerUserWidget::Tick(float DeltaTime)
{
	if (FrameCapturer.IsValid())
	{
		if (FrameCapturer->GetRemainFrameCount() == 0)
		{
			DestoryFrameCapturer();
		}
	}
}

bool UFrameCapturerUserWidget::IsTickable() const
{
	return IsShow;
}

TStatId UFrameCapturerUserWidget::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFrameCapturerUserWidget, STATGROUP_Tickables);
}

bool UFrameCapturerUserWidget::IsTickableWhenPaused() const
{
	return true;
}

void UFrameCapturerUserWidget::CaptureFrame()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);

			FIntPoint Size((int32)ViewportSize.X >> DownSampleNum, (int32)ViewportSize.Y >> DownSampleNum);
			FillImageBrush(Size.X, Size.Y);

			FrameCapturer = MakeUnique<FFrameCapturer>(ViewportClient->GetGameViewport(), Size, BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU, PF_B8G8R8A8, CaptureFrameCount == 1 ? 1 : 3);
			FrameCapturer->StartCapturingFrames(CaptureFrameCount,
				[this](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height){
				UpdateImage(RHICmdList, ColorBuffer, Texture, Width, Height);
			}
			);
		}
	}
}

void UFrameCapturerUserWidget::HiddenWidget()
{
	UPanelWidget* Parent = GetParent();
	if (Parent)
	{
		Parent->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UFrameCapturerUserWidget::ShowWidget()
{
	UPanelWidget* Parent = GetParent();
	if (Parent)
	{
		Parent->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrameCapturerUserWidget::UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height)
{
	if (BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		UpdateImageStackBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
	else if(BlurMode == EFrameCapturerUserWidgetBlurMode::GaussianBlur_GPU)
	{
		UpdateImageGaussianBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
	ShowWidget();
}

void UFrameCapturerUserWidget::UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height)
{
	StackBlur(ColorBuffer, Width, Height, BlurKernel, StackBlurParallelCore);

	FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, Width, Height);
	{
		auto Resource = (FTexture2DResource*)(ShareImageTexture2D->Resource);
		RHICmdList.UpdateTexture2D(
			Resource->GetTexture2DRHI(),
			0,
			UpdateRegion,
			sizeof(FColor) * Width,
			(uint8*)ColorBuffer.GetData()
			);
	}
}

void UFrameCapturerUserWidget::UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const TRefCountPtr<IPooledRenderTarget>& Texture, int32 Width, int32 Height)
{
	auto Resource = (FTexture2DResource*)(ShareImageTexture2D->Resource);
	auto Texture2DRHI = (FTexture2DRHIRef&)Texture->GetRenderTargetItem().ShaderResourceTexture;

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

	const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

	for (int32 i = 0; i < GaussianBlurIteratorCount; i++)
	{
		DrawGaussianBlur(RHICmdList, Texture2DRHI, (FTexture2DRHIRef&)DestRenderTarget.TargetableTexture, Width, Height, (BlurKernel >> DownSampleNum) / 3.0);
	}

	Resource->TextureRHI = Texture2DRHI;
	//Resource->TextureRHI = (FTexture2DRHIRef&)DestRenderTarget.TargetableTexture;
	RHIUpdateTextureReference(ShareImageTexture2D->TextureReference.TextureReferenceRHI, Resource->TextureRHI);
}

void UFrameCapturerUserWidget::FillImageBrush(int32 Width, int32 Height)
{
	if (!ShareImageTexture2D.IsValid() || 
		ShareImageTexture2D->GetSizeX() != Width || ShareImageTexture2D->GetSizeY() != Height)
	{
		ShareImageTexture2D = UTexture2D::CreateTransient(Width, Height);
		ShareImageTexture2D->bIgnoreStreamingMipBias = true;
		ShareImageTexture2D->SRGB = true;
		ShareImageTexture2D->UpdateResource();
	}

	FSlateBrush Brush;
	Brush.SetResourceObject(ShareImageTexture2D.Get());
	Image->SetBrush(Brush);
}

void UFrameCapturerUserWidget::DestoryFrameCapturer()
{
	if (FrameCapturer.IsValid())
	{
		FrameCapturer.Reset();
	}
}

TWeakObjectPtr<UTexture2D> UFrameCapturerUserWidget::ShareImageTexture2D;

bool UFrameCapturerUserWidget::SingletonFlag = false;
