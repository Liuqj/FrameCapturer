#pragma once

#include "Engine.h"
#include "GenericPlatformFile.h"

class FShaderFileVisitor : public IPlatformFile::FDirectoryVisitor
{
//FDirectoryVisitor
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;
//FDirectoryVisitor

public:
	void Reset();

	TArray<FString> ShaderFilePaths;
};

