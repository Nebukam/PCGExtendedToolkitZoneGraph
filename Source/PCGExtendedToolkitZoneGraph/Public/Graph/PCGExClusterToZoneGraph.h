// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphTypes.h"
#include "ZoneShapeComponent.h"
#include "Core/PCGExClustersProcessor.h"
#include "Details/PCGExAttachmentRules.h"

#include "PCGExClusterToZoneGraph.generated.h"

namespace PCGExClusters
{
	class FNodeChainBuilder;
	class FNodeChain;
}

namespace PCGExMT
{
	class FAsyncToken;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExClusterToZoneGraphSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	UPCGExClusterToZoneGraphSettings()
	{
		if (const UZoneGraphSettings* ZoneGraphSettings = GetDefault<UZoneGraphSettings>())
		{
			if (const FZoneLaneProfile* NewLaneProfile = ZoneGraphSettings->GetDefaultLaneProfile())
			{
				LaneProfile = *NewLaneProfile;
			}
		}
	}
#else
	UPCGExClusterToZoneGraphSettings()
	{
		
	}
#endif

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ClusterToZoneGraph, "Cluster to Zone Graph", "Create Zone Graph from clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterOp; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(FName("Break Conditions"), "Filters used to know which points are 'break' points. Use those if you want to create more polygon shapes.", PCGExFactories::ClusterNodeFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Defines the direction in which points will be ordered to form the final paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedComponentTags = TEXT("PCGExZoneGraph");

	/** Specify a list of functions to be called on the target actor after dynamic mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExAttachmentRules AttachmentRules;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double PolygonRadius = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EZoneShapePolygonRoutingType PolygonRoutingType = EZoneShapePolygonRoutingType::Arcs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FZoneShapePointType PolygonPointType = FZoneShapePointType::LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FZoneShapePointType RoadPointType = FZoneShapePointType::LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections")
	FZoneGraphTagMask AdditionalIntersectionTags = FZoneGraphTagMask::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FZoneLaneProfileRef LaneProfile;

private:
	friend class FPCGExClusterToZoneGraphElement;
};

struct FPCGExClusterToZoneGraphContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterToZoneGraphElement;
	TArray<TSharedPtr<PCGExClusters::FNodeChain>> Chains;

	TArray<FString> ComponentTags;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterToZoneGraphElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterToZoneGraph)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExClusterToZoneGraph
{
	class FProcessor;

	class FZGBase : public TSharedFromThis<FZGBase>
	{
	protected:
		TSharedPtr<FProcessor> Processor;

	public:
		UZoneShapeComponent* Component = nullptr;
		double StartRadius = 0;
		double EndRadius = 0;

		explicit FZGBase(const TSharedPtr<FProcessor>& InProcessor);
		void InitComponent(AActor* InTargetActor);
	};

	class FZGRoad : public FZGBase
	{
	public:
		TSharedPtr<PCGExClusters::FNodeChain> Chain;
		bool bIsReversed = false;

		explicit FZGRoad(const TSharedPtr<FProcessor>& InProcessor, const TSharedPtr<PCGExClusters::FNodeChain>& InChain, const bool InReverse);
		void Compile(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
	};

	class FZGPolygon : public FZGBase
	{
	protected:
		TArray<TSharedPtr<FZGRoad>> Roads;
		TBitArray<> FromStart;

	public:
		int32 NodeIndex = -1;
		explicit FZGPolygon(const TSharedPtr<FProcessor>& InProcessor, const PCGExClusters::FNode* InNode);

		void Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart);
		void Compile(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>
	{
		friend class FBatch;

	protected:
		FPCGExEdgeDirectionSettings DirectionSettings;

		TWeakPtr<PCGExMT::FAsyncToken> MainThreadToken;

		TSharedPtr<PCGExClusters::FNodeChainBuilder> ChainBuilder;

		TArray<TSharedPtr<FZGRoad>> Roads;
		TArray<TSharedPtr<FZGPolygon>> Polygons;

		TArray<UZoneShapeComponent> RoadComponents;
		TArray<UZoneShapeComponent> PolygonsComponents;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		bool BuildChains();
		virtual void CompleteWork() override;
		void InitComponents();
		void OnPolygonsCompilationComplete();
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void Output() override;

		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		TSharedPtr<TArray<int8>> Breakpoints;
		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
			DefaultVtxFilterValue = false;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
