#include "FrameCapturerPrivatePCH.h" 
#include "ShaderFileVisitor.h"
bool FShaderFileVisitor::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	if (!bIsDirectory)
	{
		ShaderFilePaths.Add(FPaths::GetCleanFilename(FilenameOrDirectory));
	}

	return true;
}

void FShaderFileVisitor::Reset()
{
	ShaderFilePaths.Empty();
}
