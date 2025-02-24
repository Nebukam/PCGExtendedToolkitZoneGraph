// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExZoneGraphGlobalSettings.generated.h"

class UPCGPin;

namespace PCGEx
{
	
}

UCLASS(DefaultConfig, config = Editor, defaultconfig)
class PCGEXTENDEDTOOLKITZONEGRAPH_API UPCGExZoneGraphGlobalSettings : public UObject
{
	GENERATED_BODY()

public:
	/**  */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	bool bDefaultCacheNodeOutput = false;

};
