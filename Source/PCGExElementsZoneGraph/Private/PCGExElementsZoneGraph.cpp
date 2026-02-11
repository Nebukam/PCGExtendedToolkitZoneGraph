// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsZoneGraph.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#include "PCGExGlobalSettings.h"
#include "PCGExZoneGraphGlobalSettings.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsZoneGraphModule"

void FPCGExElementsZoneGraphModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsZoneGraphModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsZoneGraphModule, PCGExElementsZoneGraph)
