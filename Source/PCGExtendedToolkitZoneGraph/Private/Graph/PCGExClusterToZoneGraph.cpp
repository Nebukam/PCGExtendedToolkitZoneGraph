// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterToZoneGraph.h"

#include "PCGExtendedToolkit.h"
#include "PCGExSubSystem.h"
#include "ZoneShapeComponent.h"
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

	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	if (const UPCGComponent* PCGComponent = InContext->GetComponent())
	{
		if (PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Zone Graph PCG Nodes should not be used in runtime-generated PCG components."));
			return false;
		}
	}

	return true;
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
				NewBatch->VtxFilterFactories = &Context->FilterFactories;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputBatches();
	Context->OutputPointsAndEdges();
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

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

		// This executes on the main thread for safety
		const FString ComponentName = TEXT("PCGZoneGraphComponent");
		const EObjectFlags ObjectFlags = (Processor->GetContext()->GetComponent()->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
		Component = Processor->GetContext()->ManagedObjects->New<UZoneShapeComponent>(InTargetActor, MakeUniqueObjectName(InTargetActor, UZoneShapeComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

		Component->ComponentTags.Reserve(Component->ComponentTags.Num() + Processor->GetContext()->ComponentTags.Num());
		for (const FString& ComponentTag : Processor->GetContext()->ComponentTags) { Component->ComponentTags.Add(FName(ComponentTag)); }
	}

	FZGRoad::FZGRoad(const TSharedPtr<FProcessor>& InProcessor, const TSharedPtr<PCGExCluster::FNodeChain>& InChain, const bool InReverse)
		: FZGBase(InProcessor), Chain(InChain), bIsReversed(InReverse)
	{
	}

	void FZGRoad::Compile(const TSharedPtr<PCGExCluster::FCluster>& Cluster)
	{
		Component->SetShapeType(FZoneShapeType::Spline);
		Component->SetCommonLaneProfile(Processor->GetSettings()->LaneProfile);

		TArray<int32> Nodes;
		const int32 ChainSize = Chain->GetNodes(Cluster, Nodes, bIsReversed);

		TArray<FZoneShapePoint>& MutablePoints = Component->GetMutablePoints();
		PCGEx::InitArray(MutablePoints, ChainSize);

		if (Chain->bIsClosedLoop)
		{
			// Redundant last point
			Nodes.Add(Nodes.Last());
		}

		for (int i = 0; i < ChainSize; i++)
		{
			const PCGExCluster::FNode* Node = Cluster->GetNode(Nodes[i]);
			FVector Position = Cluster->GetPos(Node);
			FVector NextPosition = Position;
			i == ChainSize - 1 ? Position : Cluster->GetPos(Nodes[i + 1]);

			if (i == ChainSize - 1)
			{
				NextPosition = Position + (Position - Cluster->GetPos(Nodes[i - 1]));
			}
			else
			{
				NextPosition = Cluster->GetPos(Nodes[i + 1]);
			}

			FZoneShapePoint ShapePoint = FZoneShapePoint(Position);
			ShapePoint.SetRotationFromForwardAndUp((NextPosition - Position), FVector::UpVector);
			ShapePoint.Type = Processor->GetSettings()->RoadPointType;

			// TODO : Point setup

			MutablePoints[i] = ShapePoint;
		}

		if (!Chain->bIsClosedLoop)
		{
			if (bIsReversed)
			{
				if (!Cluster->GetNode(Nodes[0])->IsLeaf()) { MutablePoints[0].Position += MutablePoints[0].Rotation.RotateVector(FVector::BackwardVector) * StartRadius; }
				if (!Cluster->GetNode(Nodes.Last())->IsLeaf()) { MutablePoints.Last().Position += MutablePoints.Last().Rotation.RotateVector(FVector::ForwardVector) * EndRadius; }
			}
			else
			{
				if (!Cluster->GetNode(Nodes[0])->IsLeaf()) { MutablePoints[0].Position += MutablePoints[0].Rotation.RotateVector(FVector::ForwardVector) * StartRadius; }
				if (!Cluster->GetNode(Nodes.Last())->IsLeaf()) { MutablePoints.Last().Position += MutablePoints.Last().Rotation.RotateVector(FVector::BackwardVector) * EndRadius; }
			}
		}

		Component->UpdateShape();
	}

	FZGPolygon::FZGPolygon(const TSharedPtr<FProcessor>& InProcessor, const PCGExCluster::FNode* InNode)
		: FZGBase(InProcessor), NodeIndex(InNode->Index)
	{
		FromStart.Init(false, InNode->Num());
		Roads.Reserve(InNode->Num());
	}

	void FZGPolygon::Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart)
	{
		FromStart[Roads.Add(InRoad)] = bFromStart;
	}

	void FZGPolygon::Compile(const TSharedPtr<PCGExCluster::FCluster>& Cluster)
	{
		Component->SetShapeType(FZoneShapeType::Polygon);
		Component->SetPolygonRoutingType(Processor->GetSettings()->PolygonRoutingType);
		Component->SetTags(Component->GetTags() | Processor->GetSettings()->AdditionalIntersectionTags);
		Component->SetCommonLaneProfile(Processor->GetSettings()->LaneProfile);

		const PCGExCluster::FNode* Center = Cluster->GetNode(NodeIndex);
		const FVector CenterPosition = Cluster->GetPos(Center);

		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, Roads.Num());
		Order.Sort(
			[&](const int32 A, const int32 B)
			{
				const FVector DirA = Roads[A]->Chain->GetEdgeDir(Cluster, FromStart[A]);
				const FVector DirB = Roads[B]->Chain->GetEdgeDir(Cluster, FromStart[B]);
				return PCGExMath::GetRadiansBetweenVectors(DirA, FVector::ForwardVector) > PCGExMath::GetRadiansBetweenVectors(DirB, FVector::ForwardVector);
			});

		TArray<FZoneShapePoint>& MutablePoints = Component->GetMutablePoints();
		PCGEx::InitArray(MutablePoints, Order.Num());

		// Build polygon

		for (int i = 0; i < Order.Num(); i++)
		{
			const int32 Ri = Order[i];
			const TSharedPtr<FZGRoad> Road = Roads[Ri];

			// TODO : Find proper road intersection location

			const PCGExCluster::FNode* OtherNode = nullptr;

			if (Road->Chain->SingleEdge != -1)
			{
				OtherNode = FromStart[Ri] ? Cluster->GetNode(Road->Chain->Links.Last()) : Cluster->GetNode(Road->Chain->Seed.Node);
			}
			else
			{
				OtherNode = FromStart[Ri] ? Cluster->GetNode(Road->Chain->Links[0]) : Cluster->GetNode(Road->Chain->Links.Last(1));
			}
			const FVector OtherPosition = Cluster->GetPos(OtherNode);
			const FVector RoadDirection = (OtherPosition - CenterPosition).GetSafeNormal();
			double Radius = Processor->GetSettings()->PolygonRadius;

			if (FromStart[Ri]) { Road->StartRadius = Radius; }
			else { Road->EndRadius = Radius; }

			FZoneShapePoint ShapePoint = FZoneShapePoint(CenterPosition + RoadDirection * Radius);
			ShapePoint.SetRotationFromForwardAndUp(RoadDirection * -1, FVector::UpVector);
			ShapePoint.Type = Processor->GetSettings()->PolygonPointType;

			MutablePoints[i] = ShapePoint;
		}

		Component->UpdateShape();
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterToZoneGraph::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade)) { return false; }

		if (VtxFiltersManager)
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FilterBreakpoints)

			FilterBreakpoints->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->BuildChains();
				};

			FilterBreakpoints->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					This->FilterVtxScope(Scope);
				};

			FilterBreakpoints->StartSubLoops(NumNodes, GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize());
		}
		else
		{
			return BuildChains();
		}

		return true;
	}

	bool FProcessor::BuildChains()
	{
		ChainBuilder = MakeShared<PCGExCluster::FNodeChainBuilder>(Cluster.ToSharedRef());
		ChainBuilder->Breakpoints = VtxFilterCache;
		bIsProcessorValid = ChainBuilder->Compile(AsyncManager);

		if (!bIsProcessorValid) { return false; }

		Polygons.Reserve(NumNodes / 2);

		return bIsProcessorValid;
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

			int32 StartNode = -1;
			int32 EndNode = -1;
			bool bReverse = false;

			StartNode = Chain->Seed.Node;
			EndNode = Chain->Links.Last().Node;

			bReverse = DirectionSettings.SortExtrapolation(Cluster.Get(), Chain->Seed.Edge, StartNode, EndNode);


			TSharedPtr<FZGRoad> Road = MakeShared<FZGRoad>(This, Chain, bReverse);
			Roads.Add(Road);

			const PCGExCluster::FNode* Start = Cluster->GetNode(StartNode);
			const PCGExCluster::FNode* End = Cluster->GetNode(EndNode);

			if (Chain->bIsClosedLoop && Start->IsBinary() && End->IsBinary())
			{
				// Roaming closed loop, road only!
				continue;
			}

			if (!Start->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(StartNode);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(This, Start);
					Polygons.Add(NewPolygon);
					Map.Add(StartNode, NewPolygon);
					PolygonPtr = &NewPolygon;
				}
				(*PolygonPtr)->Add(Road, true);
			}

			if (!End->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(EndNode);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(This, End);
					Polygons.Add(NewPolygon);
					Map.Add(EndNode, NewPolygon);
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

		// Dispatch async polygon processing

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
				PCGEX_SCOPE_LOOP(i) { This->Polygons[i]->Compile(This->Cluster); }
			};

		CompileIntersections->StartSubLoops(Polygons.Num(), 32);
	}

	void FProcessor::OnPolygonsCompilationComplete()
	{
		if (Roads.IsEmpty()) { return; }
		StartParallelLoopForRange(Roads.Num(), 32);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			// Compile road, after polygons -- since polygons will feed road their start/end offset
			Roads[Index]->Compile(Cluster);
		}
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

		Context->AddNotifyActor(TargetActor);
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
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
