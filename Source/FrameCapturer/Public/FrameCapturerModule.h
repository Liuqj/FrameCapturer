#pragma once

#include "ModuleInterface.h"
#include "FrameCapturerBlueprintLibrary.h"

class FRAMECAPTURER_API FFrameCapturerModule : public IModuleInterface
{
public:
	static inline FFrameCapturerModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FFrameCapturerModule >("FrameCapturer");
	}
};