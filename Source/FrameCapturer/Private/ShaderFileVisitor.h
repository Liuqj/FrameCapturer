#pragma once

class FShaderFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual ~FShaderFileVisitor() {}
//FDirectoryVisitor
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;
//FDirectoryVisitor

public:
	void Reset();

	TArray<FString> ShaderFilePaths;
};

