// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExChain.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Transform/PCGExTransform.h"

#include "PCGExClusterToZoneGraph.generated.h"

class UZoneShapeComponent;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExClusterToZoneGraphSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ClusterToZoneGraph, "Cluster to Zone Graph", "Create Zone Graph from clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
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

private:
	friend class FPCGExClusterToZoneGraphElement;
};

struct FPCGExClusterToZoneGraphContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExClusterToZoneGraphElement;
	TArray<TSharedPtr<PCGExCluster::FNodeChain>> Chains;

	TArray<FString> ComponentTags;
	TSet<AActor*> NotifyActors;
};

class FPCGExClusterToZoneGraphElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
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

		explicit FZGBase(const TSharedPtr<FProcessor>& InProcessor);
		void InitComponent(AActor* InTargetActor);
	};

	class FZGRoad : public FZGBase
	{
		TSharedPtr<PCGExCluster::FNodeChain> Chain;

	public:
		explicit FZGRoad(const TSharedPtr<FProcessor>& InProcessor, const TSharedPtr<PCGExCluster::FNodeChain>& InChain);
		void Compile(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
	};

	class FZGPolygon : public FZGBase
	{
	protected:
		TArray<TSharedPtr<FZGRoad>> Chains;
		TBitArray<> FromStart;

	public:
		int32 NodeIndex = -1;
		explicit FZGPolygon(const TSharedPtr<FProcessor>& InProcessor, const PCGExCluster::FNode* InNode);

		void Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart);
		void Compile(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>
	{
		friend class FBatch;

	protected:
		FPCGExEdgeDirectionSettings DirectionSettings;

		TWeakPtr<PCGExMT::FAsyncToken> MainThreadToken;

		TSharedPtr<TArray<int8>> Breakpoints;
		TSharedPtr<TArray<FVector2D>> ProjectedPositions;
		TSharedPtr<PCGExCluster::FNodeChainBuilder> ChainBuilder;

		TArray<TSharedPtr<FZGRoad>> Roads;
		TArray<TSharedPtr<FZGPolygon>> Intersections;

		TArray<UZoneShapeComponent> RoadComponents;
		TArray<UZoneShapeComponent> PolygonsComponents;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
		void BuildZGData();
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
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
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
