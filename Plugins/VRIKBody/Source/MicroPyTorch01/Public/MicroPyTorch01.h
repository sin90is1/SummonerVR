// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include <vector>

#if PLATFORM_WINDOWS
// BEGIN C HEADER
typedef int(*__TSW_LoadScriptModel)(char* FileName);
typedef bool(*__TSW_CheckModel)(int ModelId);
typedef bool(*__TSW_Forward1d)(int ModelId, const std::vector<float> InData, int InDimensions);
typedef bool(*__TSW_ForwardTensor)(int ModelId, const float* InData, const int InDimensions[3], float*& OutData, int* Size1, int* Size2, int* Size3);
typedef bool(*__TSW_ForwardPass_Def)(int ModelId, const float* InData, const int* InDimensions, int nDimensionsCount, float*& OutData, int*& OutDimensions, int* OutDimensionsCount);
typedef bool(*__TSW_Execute_Def)(int ModelId, char* FunctionName, const float* InData, const int* InDimensions, int nDimensionsCount, float*& OutData, int*& OutDimensions, int* OutDimensionsCount);
// END HEADER
#endif

class FMicroPyTorch01Module : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if PLATFORM_WINDOWS
	/** DLL Functions */
	__TSW_LoadScriptModel FuncTSW_LoadScriptModel;
	__TSW_CheckModel FuncTSW_CheckModel;
	__TSW_Forward1d FuncTSW_Forward1d;
	__TSW_ForwardTensor FuncTSW_ForwardTensor;
	__TSW_ForwardPass_Def FuncTSW_ForwardPass_Def;
	__TSW_Execute_Def FuncTSW_Execute_Def;
#endif

	bool bDllLoaded;

protected:
	/** DLL Handle */
	void* WrapperDllHandle;
};
