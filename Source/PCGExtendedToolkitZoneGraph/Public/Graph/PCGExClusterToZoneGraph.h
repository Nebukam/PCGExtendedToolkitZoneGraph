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

UENUM(BlueprintType)
enum class EPCGExZGAutoRadiusMode : uint8
{
	Disabled       = 0 UMETA(DisplayName="Disabled"),
	WidestLane     = 1 UMETA(DisplayName="Widest Lane"),
	HalfProfile    = 2 UMETA(DisplayName="Half Profile Width"),
	WidestLaneMin  = 3 UMETA(DisplayName="Widest Lane (Min)"),
	HalfProfileMin = 4 UMETA(DisplayName="Half Profile Width (Min)"),
};

namespace PCGExClusters
{
	class FNodeChain;
}

namespace PCGExMT
{
	class FAsyncToken;
	class FTimeSlicedMainThreadLoop;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="cluster-to-zone-graph"))
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	double PolygonRadius = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonRadius = false;

	/** Per-point polygon radius override. Read from: Points. Attribute type: double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Radius (Attr)", EditCondition="bOverridePolygonRadius"))
	FName PolygonRadiusAttribute = FName("ZG.PolygonRadius");

	/** Auto-compute polygon radius from connected road lane profiles.
	 * Disabled: use user radius only.
	 * WidestLane: radius = widest single lane across connected roads.
	 * HalfProfile: radius = max(total profile width) / 2 across connected roads.
	 * WidestLane (Min) / HalfProfile (Min): use the larger of user radius and computed value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	EPCGExZGAutoRadiusMode AutoRadiusMode = EPCGExZGAutoRadiusMode::Disabled;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	EZoneShapePolygonRoutingType PolygonRoutingType = EZoneShapePolygonRoutingType::Arcs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonRoutingType = false;

	/** Per-point polygon routing override. Read from: Points. Attribute type: int32.
	 * Values: 0=Bezier, 1=Arcs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Polygon Routing (Attr)", EditCondition="bOverridePolygonRoutingType"))
	FName PolygonRoutingTypeAttribute = FName("ZG.PolygonRoutingType");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	FZoneShapePointType PolygonPointType = FZoneShapePointType::LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonPointType = false;

	/** Per-point polygon shape point type override. Read from: Points. Attribute type: int32.
	 * Values: 0=Sharp, 1=Bezier, 2=AutoBezier, 3=LaneProfile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Polygon Point Type (Attr)", EditCondition="bOverridePolygonPointType"))
	FName PolygonPointTypeAttribute = FName("ZG.PolygonPointType");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	FZoneShapePointType RoadPointType = FZoneShapePointType::LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideRoadPointType = false;

	/** Per-edge road shape point type override. Read from: Points. Attribute type: int32.
	 * Values: 0=Sharp, 1=Bezier, 2=AutoBezier, 3=LaneProfile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Road Point Type (Attr)", EditCondition="bOverrideRoadPointType"))
	FName RoadPointTypeAttribute = FName("ZG.RoadPointType");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	FZoneLaneProfileRef LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideLaneProfile = false;

	/** Lane profile override. Read from: Points (polygons), Edges then Points fallback (roads, majority vote). Attribute type: FName. Must match a registered lane profile name in ZoneGraph settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Lane Profile (Attr)", EditCondition="bOverrideLaneProfile"))
	FName LaneProfileAttribute = FName("ZG.LaneProfile");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph")
	FZoneGraphTagMask AdditionalIntersectionTags = FZoneGraphTagMask::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideAdditionalIntersectionTags = false;

	/** Per-point intersection tag override. Read from: Points. Attribute type: int32, interpreted as a ZoneGraph tag bitmask (uint32). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ZoneGraph", meta=(PCG_Overridable, DisplayName="Intersection Tags (Attr)", EditCondition="bOverrideAdditionalIntersectionTags"))
	FName AdditionalIntersectionTagsAttribute = FName("ZG.IntersectionTags");

private:
	friend class FPCGExClusterToZoneGraphElement;
};

struct FPCGExClusterToZoneGraphContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterToZoneGraphElement;

	TArray<FString> ComponentTags;

	TMap<FName, FZoneLaneProfileRef> LaneProfileMap;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterToZoneGraphElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterToZoneGraph)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExClusterToZoneGraph
{
	class FProcessor;

	class FZGBase : public TSharedFromThis<FZGBase>
	{
	protected:
		FProcessor* Processor = nullptr;
		TArray<FZoneShapePoint> PrecomputedPoints;

	public:
		UZoneShapeComponent* Component = nullptr;
		double StartRadius = 0;
		double EndRadius = 0;

		explicit FZGBase(FProcessor* InProcessor);
		void InitComponent(AActor* InTargetActor);
	};

	class FZGRoad : public FZGBase
	{
	public:
		TSharedPtr<PCGExClusters::FNodeChain> Chain;
		bool bIsReversed = false;

		FZoneLaneProfileRef CachedLaneProfile;
		double CachedMaxLaneWidth = 0;
		double CachedTotalProfileWidth = 0;

		explicit FZGRoad(FProcessor* InProcessor, const TSharedPtr<PCGExClusters::FNodeChain>& InChain, const bool InReverse);
		void ResolveLaneProfile(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void Compile();
	};

	class FZGPolygon : public FZGBase
	{
	protected:
		TArray<TSharedPtr<FZGRoad>> Roads;
		TBitArray<> FromStart;

		double CachedRadius = 0;
		TArray<double> CachedRoadRadii;
		EZoneShapePolygonRoutingType CachedRoutingType = EZoneShapePolygonRoutingType::Arcs;
		FZoneShapePointType CachedPointType = FZoneShapePointType::LaneProfile;
		FZoneGraphTagMask CachedAdditionalTags = FZoneGraphTagMask::None;
		FZoneLaneProfileRef CachedLaneProfile;
		TArray<FZoneLaneProfileRef> CachedPointLaneProfiles;

	public:
		int32 NodeIndex = -1;

		explicit FZGPolygon(FProcessor* InProcessor, const PCGExClusters::FNode* InNode);

		void Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart);
		void Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void SyncRadiusToRoads();
		void Compile();
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>
	{
		friend class FBatch;
		friend class FZGRoad;
		friend class FZGPolygon;

	protected:
		FPCGExEdgeDirectionSettings DirectionSettings;

		AActor* TargetActor = nullptr;
		FAttachmentTransformRules CachedAttachmentRules = FAttachmentTransformRules::KeepWorldTransform;

		TWeakPtr<PCGExMT::FAsyncToken> MainThreadToken;

		TSharedPtr<PCGExMT::FTimeSlicedMainThreadLoop> MainCompileLoop;

		TArray<TSharedPtr<PCGExClusters::FNodeChain>> ProcessedChains;

		TArray<TSharedPtr<FZGRoad>> Roads;
		TArray<TSharedPtr<FZGPolygon>> Polygons;

		TSharedPtr<PCGExData::TBuffer<double>> PolygonRadiusBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> PolygonRoutingTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> PolygonPointTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> RoadPointTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> AdditionalIntersectionTagsBuffer;
		TSharedPtr<PCGExData::TBuffer<FName>> EdgeLaneProfileBuffer;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		bool BuildChains();
		virtual void CompleteWork() override;
		void InitComponents();
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void Output() override;

		virtual void Cleanup() override;

		FZoneLaneProfileRef ResolveLaneProfileByName(FName ProfileName) const;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		TSharedPtr<TArray<int8>> Breakpoints;
		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
			DefaultVtxFilterValue = false;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
