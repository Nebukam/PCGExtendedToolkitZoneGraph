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
class PCGEXTENDEDTOOLKIT_API UPCGExZoneGraphGlobalSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Value applied by default to node caching when `Default` is selected -- note that some nodes may stop working as expected when working with cached data.*/
	UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	bool bDefaultCacheNodeOutput = false;

};
