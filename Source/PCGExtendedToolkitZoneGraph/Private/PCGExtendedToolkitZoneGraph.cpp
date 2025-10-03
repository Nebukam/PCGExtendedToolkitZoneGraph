// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitZoneGraph.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#include "PCGExGlobalSettings.h"
#include "PCGExZoneGraphGlobalSettings.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitZoneGraphModule"

void FPCGExtendedToolkitZoneGraphModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FPCGExtendedToolkitZoneGraphModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitZoneGraphModule, PCGExtendedToolkitZoneGraph)
