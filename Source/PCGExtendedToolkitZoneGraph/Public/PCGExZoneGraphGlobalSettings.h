// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExZoneGraphGlobalSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="PCGEx + ZoneGraph", Description="Configure PCGEx + ZoneGraph settings"))
class PCGEXTENDEDTOOLKITZONEGRAPH_API UPCGExZoneGraphGlobalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**  */
	//UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	//bool bDefaultCacheNodeOutput = false;
};
