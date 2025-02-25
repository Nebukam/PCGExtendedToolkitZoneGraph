// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterToZoneGraph.h"

#include "PCGExSubSystem.h"
#include "ZoneShapeComponent.h"

#include "Curve/CurveUtil.h"
#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExClusterToZoneGraph"
#define PCGEX_NAMESPACE ClusterToZoneGraph

PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(ClusterToZoneGraph)

bool FPCGExClusterToZoneGraphElement::Boot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

#if WITH_EDITOR
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	if (InContext->SourceComponent.IsValid())
	{
		if (InContext->SourceComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Zone Graph PCG Nodes should not be used in runtime-generated PCG components."));
			return false;
		}
	}

	return true;
#else
	PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Zone Graph PCG Nodes are only supported in editor."));
	return false;
#endif
}

bool FPCGExClusterToZoneGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterToZoneGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterToZoneGraph::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterToZoneGraph::FBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true; // Not really but we need the step
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputBatches();
	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExClusterToZoneGraph
{
	FZGBase::FZGBase(const TSharedPtr<FProcessor>& InProcessor)
		: Processor(InProcessor)
	{
	}

	void FZGBase::InitComponent(AActor* InTargetActor)
	{
		if (!InTargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, Processor->GetContext(), FTEXT("Invalid target actor."));
			return;
		}

		const FString ComponentName = TEXT("PCGZoneGraphComponent");
		const EObjectFlags ObjectFlags = (Processor->GetContext()->SourceComponent.Get()->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
		Component = Processor->GetContext()->ManagedObjects->New<UZoneShapeComponent>(InTargetActor, MakeUniqueObjectName(InTargetActor, UZoneShapeComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

		Component->ComponentTags.Reserve(Component->ComponentTags.Num() + Processor->GetContext()->ComponentTags.Num());
		for (const FString& ComponentTag : Processor->GetContext()->ComponentTags) { Component->ComponentTags.Add(FName(ComponentTag)); }
	}

	FZGRoad::FZGRoad(const TSharedPtr<FProcessor>& InProcessor, const TSharedPtr<PCGExCluster::FNodeChain>& InChain)
		: FZGBase(InProcessor), Chain(InChain)
	{
	}

	void FZGRoad::Compile(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
	{
		/*
		const TSharedPtr<PCGExCluster::FNodeChain> Chain = ChainBuilder->Chains[Iteration];
		if (!Chain) { return; }

		const int32 ChainSize = Chain->Links.Num() + 1;

		PCGExGraph::FEdge ChainDir = PCGExGraph::FEdge(Chain->Seed.Edge, Cluster->GetNode(Chain->Seed)->PointIndex, Cluster->GetNode(Chain->Links.Last())->PointIndex);
		const bool bReverse = DirectionSettings.SortEndpoints(Cluster.Get(), ChainDir);

		bool bDoReverse = bReverse;


		TArray<FPCGPoint> MutablePoints;
		MutablePoints.SetNumUninitialized(ChainSize);
		// MutablePoints[0] = PathIO->GetInPoint(Cluster->GetNode(Chain->Seed)->PointIndex); // First point in chain

		for (int i = 1; i < ChainSize; i++)
		{
			// MutablePoints[i] = PathIO->GetInPoint(Cluster->GetNode(Chain->Links[i - 1])->PointIndex); // Subsequent points
		}

		if (bDoReverse) { Algo::Reverse(MutablePoints); }

		if (!Chain->bIsClosedLoop)
		{
		}
		else
		{
		}
		*/
	}

	FZGPolygon::FZGPolygon(const TSharedPtr<FProcessor>& InProcessor, const PCGExCluster::FNode* InNode)
		: FZGBase(InProcessor)
	{
		FromStart.Init(false, InNode->Num());
		Chains.Reserve(InNode->Num());
	}

	void FZGPolygon::Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart)
	{
		FromStart[Chains.Add(InRoad)] = bFromStart;
	}

	void FZGPolygon::Compile(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
	{
		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, Chains.Num());
		Order.Sort(
			[&](const int32 A, const int32 B)
			{
				const double VA = PCGExMath::GetRadiansBetweenVectors(InCluster->GetEdgeDir(FromStart[A]), FVector::ForwardVector);
				const double VB = PCGExMath::GetRadiansBetweenVectors(InCluster->GetEdgeDir(FromStart[B]), FVector::ForwardVector);
				return VA > VB;
			});
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterToZoneGraph::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, StaticCastWeakPtr<FBatch>(ParentBatch).Pin()->DirectionSettings, EdgeDataFacade)) { return false; }

		ChainBuilder = MakeShared<PCGExCluster::FNodeChainBuilder>(Cluster.ToSharedRef());
		ChainBuilder->Breakpoints = Breakpoints;
		bIsProcessorValid = ChainBuilder->Compile(AsyncManager);

		if (!bIsProcessorValid) { return false; }

		Polygons.Reserve(NumNodes / 2);

		return true;
	}


	void FProcessor::CompleteWork()
	{
		if (ChainBuilder->Chains.IsEmpty())
		{
			bIsProcessorValid = false;
			return;
		}

		TMap<int32, TSharedPtr<FZGPolygon>> Map;
		TSharedPtr<FProcessor> This = SharedThis(this);

		const int32 NumChains = ChainBuilder->Chains.Num();
		for (int i = 0; i < NumChains; i++)
		{
			TSharedPtr<PCGExCluster::FNodeChain> Chain = ChainBuilder->Chains[i];
			if (!Chain) { continue; }

			if (Chain->SingleEdge != -1) { continue; } // TODO : Handle single-edge chain

			TSharedPtr<FZGRoad> Road = MakeShared<FZGRoad>(This, Chain);
			Roads.Add(Road);

			const PCGExCluster::FNode* Start = Cluster->GetNode(Chain->Seed.Node);
			const PCGExCluster::FNode* End = Cluster->GetNode(Chain->Links.Last().Node);

			const bool bReverse = DirectionSettings.SortExtrapolation(Cluster.Get(), Chain->Seed.Edge, Chain->Seed.Node, Chain->Links.Last().Node);

			if (Chain->bIsClosedLoop && Start->IsBinary() && End->IsBinary()) { continue; } // Roaming closed loop

			if (!Start->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(Start->Index);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(This, Start);
					Polygons.Add(NewPolygon);
					Map.Add(Start->Index, NewPolygon);
					PolygonPtr = &NewPolygon;
				}
				(*PolygonPtr)->Add(Road, true);
			}

			if (!End->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(End->Index);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(This, End);
					Polygons.Add(NewPolygon);
					Map.Add(End->Index, NewPolygon);
					PolygonPtr = &NewPolygon;
				}

				(*PolygonPtr)->Add(Road, false);
			}
		}

		ChainBuilder.Reset();

		MainThreadToken = AsyncManager->TryCreateToken(TEXT("ZGMainThreadToken"));

		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction(
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->InitComponents();
				PCGEX_ASYNC_RELEASE_TOKEN(This->MainThreadToken)
			});
	}

	void FProcessor::InitComponents()
	{
		AActor* TargetActor = /*Settings->TargetActor.Get() ? Settings->TargetActor.Get() :*/ ExecutionContext->GetTargetActor(nullptr);

		// Init components on main thread
		for (const TSharedPtr<FZGPolygon>& Polygon : Polygons) { Polygon->InitComponent(TargetActor); }
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->InitComponent(TargetActor); }

		if (Polygons.IsEmpty())
		{
			OnPolygonsCompilationComplete();
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CompileIntersections)

		CompileIntersections->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnPolygonsCompilationComplete();
			};

		CompileIntersections->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++) { This->Polygons[i]->Compile(This->Cluster); }
			};

		CompileIntersections->StartSubLoops(Polygons.Num(), 32);
	}

	void FProcessor::OnPolygonsCompilationComplete()
	{
		if (Roads.IsEmpty()) { return; }
		StartParallelLoopForRange(Roads.Num(), 32);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		// Compile road, after intersections.
		Roads[Iteration]->Compile(Cluster);
	}

	void FProcessor::OnRangeProcessingComplete()
	{
	}

	void FProcessor::Output()
	{
		if (!this->bIsProcessorValid) { return; }

		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExClusterToZoneGraph::FProcessor::Output);

		AActor* TargetActor = /*Settings->TargetActor.Get() ? Settings->TargetActor.Get() :*/ ExecutionContext->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			return;
		}

		const FAttachmentTransformRules AttachmentRules = Settings->AttachmentRules.GetRules();

		for (const TSharedPtr<FZGPolygon>& Polygon : Polygons) { Context->AttachManagedComponent(TargetActor, Polygon->Component, AttachmentRules); }
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Context->AttachManagedComponent(TargetActor, Road->Component, AttachmentRules); }

		Context->NotifyActors.Add(TargetActor);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>::Cleanup();
		Roads.Empty();
		Polygons.Empty();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(Context, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx are missing the specified Direction attribute."));
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

		const int32 NumPoints = VtxDataFacade->GetNum();
		Breakpoints = MakeShared<TArray<int8>>();
		Breakpoints->Init(false, NumPoints);

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->Breakpoints = Breakpoints;
		return TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
	}
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
