#pragma once

#include "RefCounting.h"

class FFrameCapturePooledTexture2DItem : public IRefCountedObject
{
public:
	FFrameCapturePooledTexture2DItem(uint32 Width, uint32 Height);
	virtual uint32 AddRef() const override;
	virtual uint32 Release() const override;
	virtual uint32 GetRefCount() const override;
	bool IsFree() const;

	class UTexture2D* GetItem();

private:
	UTexture2D* Item;
	mutable int32 NumRefs = 0;
};


class FFrameCapturePooledRenderTarget2DItem : public IRefCountedObject
{
public:
	FFrameCapturePooledRenderTarget2DItem(uint32 Width, uint32 Height);
	virtual uint32 AddRef() const override;
	virtual uint32 Release() const override;
	virtual uint32 GetRefCount() const override;
	bool IsFree() const;

	class UTextureRenderTarget2D* GetItem();

private:
	UTextureRenderTarget2D* Item;
	mutable int32 NumRefs = 0;
};

class FFrameCaptureTexturePool: public FGCObject
{
public:
	static FFrameCaptureTexturePool& Get();
	TRefCountPtr<FFrameCapturePooledTexture2DItem>& GetTexture2D(int32 Width, int32 Height);
	TRefCountPtr<FFrameCapturePooledRenderTarget2DItem>& GetRenderTarget2D(int32 Width, int32 Height);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	TArray<TRefCountPtr<FFrameCapturePooledTexture2DItem>> Texture2DPool;
	TArray<TRefCountPtr<FFrameCapturePooledRenderTarget2DItem>> RenderTarget2DPool;

};