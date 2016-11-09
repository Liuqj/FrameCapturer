#include "FrameCapturerPrivatePCH.h"
#include "FrameCaptureTexturePool.h"

FFrameCapturePooledTexture2DItem::FFrameCapturePooledTexture2DItem(uint32 Width, uint32 Height)
{
	Item = UTexture2D::CreateTransient(Width, Height);
	Item->UpdateResource();
}

uint32 FFrameCapturePooledTexture2DItem::AddRef() const
{
	return ++NumRefs;
}

uint32 FFrameCapturePooledTexture2DItem::Release() const
{
	return --NumRefs;
}

uint32 FFrameCapturePooledTexture2DItem::GetRefCount() const
{
	return NumRefs;
}

bool FFrameCapturePooledTexture2DItem::IsFree() const
{
	return NumRefs == 1;
}

UTexture2D* FFrameCapturePooledTexture2DItem::GetItem()
{
	return Item;
}

FFrameCaptureTexturePool& FFrameCaptureTexturePool::Get()
{
	static FFrameCaptureTexturePool FrameCaptureTexturePool;
	return FrameCaptureTexturePool;
}

TRefCountPtr<FFrameCapturePooledTexture2DItem>& FFrameCaptureTexturePool::GetTexture2D(int32 Width, int32 Height)
{
	for (auto& Texture2DItem : Texture2DPool)
	{
		if (Texture2DItem.IsValid()
			&& Texture2DItem->IsFree()
			&& Texture2DItem->GetItem()->GetSizeX() == Width
			&& Texture2DItem->GetItem()->GetSizeY() == Height)
		{
			return Texture2DItem;
		}
	}
	TRefCountPtr<FFrameCapturePooledTexture2DItem> FrameCapturePooledTexture2DItem(new FFrameCapturePooledTexture2DItem(Width, Height));
	return Texture2DPool[Texture2DPool.Add(FrameCapturePooledTexture2DItem)];
}

TRefCountPtr<FFrameCapturePooledRenderTarget2DItem>& FFrameCaptureTexturePool::GetRenderTarget2D(int32 Width, int32 Height)
{
	for (auto& RenderTarget2DItem : RenderTarget2DPool)
	{
		if (RenderTarget2DItem.IsValid()
			&& RenderTarget2DItem->IsFree()
			&& RenderTarget2DItem->GetItem()->SizeX == Width
			&& RenderTarget2DItem->GetItem()->SizeY == Height)
		{
			return RenderTarget2DItem;
		}
	}
	TRefCountPtr<FFrameCapturePooledRenderTarget2DItem> FrameCapturePooledTexture2DItem(new FFrameCapturePooledRenderTarget2DItem(Width, Height));
	return RenderTarget2DPool[RenderTarget2DPool.Add(FrameCapturePooledTexture2DItem)];
}

void FFrameCaptureTexturePool::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Texture2DItem : Texture2DPool)
	{
		auto Item = Texture2DItem->GetItem();
		Collector.AddReferencedObject(Item);
	}

	for (auto& RT2DItem : RenderTarget2DPool)
	{
		auto Item = RT2DItem->GetItem();
		Collector.AddReferencedObject(Item);
	}
}

FFrameCapturePooledRenderTarget2DItem::FFrameCapturePooledRenderTarget2DItem(uint32 Width, uint32 Height)
{
	Item = NewObject<UTextureRenderTarget2D>();
	Item->SetFlags(RF_Transient);
	Item->InitAutoFormat(Width, Height);
	Item->UpdateResourceImmediate(true);
}

uint32 FFrameCapturePooledRenderTarget2DItem::AddRef() const
{
	return ++NumRefs;
}

uint32 FFrameCapturePooledRenderTarget2DItem::Release() const
{
	return --NumRefs;
}

uint32 FFrameCapturePooledRenderTarget2DItem::GetRefCount() const
{
	return NumRefs;
}

bool FFrameCapturePooledRenderTarget2DItem::IsFree() const
{
	return NumRefs == 1;
}

class UTextureRenderTarget2D* FFrameCapturePooledRenderTarget2DItem::GetItem()
{
	return Item;
}
