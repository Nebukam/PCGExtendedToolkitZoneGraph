// Copyright 2025 Timothé Lapetite

#pragma once

#include "CoreMinimal.h"
#include "PCGExLegacyModuleInterface.h"

class FPCGExElementsZoneGraphModule final : public IPCGExLegacyModuleInterface
{
	PCGEX_MODULE_BODY
	
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
