// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitZoneGraph.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#include "PCGExGlobalSettings.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitZoneGraphModule"

void FPCGExtendedToolkitZoneGraphModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
				"Project", "Plugins", "PCGEx",
				LOCTEXT("PCGExDetailsName", "PCGEx : ZoneGraph"),
				LOCTEXT("PCGExDetailsDescription", "Configure PCG Extended Toolkit : Zone Graph settings"),
				GetMutableDefault<UPCGExGlobalSettings>()
			);
	}
#endif
}

void FPCGExtendedToolkitZoneGraphModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "PCGEx");
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitZoneGraphModule, PCGExtendedToolkitZoneGraph)
