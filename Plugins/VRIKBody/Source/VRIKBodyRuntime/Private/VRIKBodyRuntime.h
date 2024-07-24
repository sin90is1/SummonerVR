// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

#pragma once
 
#include "VRIKBodyPrivatePCH.h"
#include "Modules/ModuleManager.h"
 
class FVRIKBodyRuntime : public IModuleInterface
{
public:
	virtual void StartupModule() override {};
	virtual void ShutdownModule() override {};
};