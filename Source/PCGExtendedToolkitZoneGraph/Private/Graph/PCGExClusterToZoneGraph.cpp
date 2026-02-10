// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterToZoneGraph.h"

#include "PCGComponent.h"
#include "PCGExSubSystem.h"
#include "ZoneShapeComponent.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Clusters/Artifacts/PCGExCachedChain.h"
#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExMT.h"
#include "Helpers/PCGExArrayHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExClusterToZoneGraph"
#define PCGEX_NAMESPACE ClusterToZoneGraph

PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(ClusterToZoneGraph)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ClusterToZoneGraph)

bool FPCGExClusterToZoneGraphElement::Boot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	if (const UPCGComponent* PCGComponent = InContext->GetComponent())
	{
		if (PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Zone Graph PCG Nodes should not be used in runtime-generated PCG components."));
			return false;
		}
	}

	if (Settings->bOverrideLaneProfile)
	{
		if (const UZoneGraphSettings* ZGSettings = GetDefault<UZoneGraphSettings>())
		{
			for (const FZoneLaneProfile& Profile : ZGSettings->GetLaneProfiles())
			{
				Context->LaneProfileMap.Add(Profile.Name, FZoneLaneProfileRef(Profile));
			}
		}
	}

	return true;
}

bool FPCGExClusterToZoneGraphElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterToZoneGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
				NewBatch->VtxFilterFactories = &Context->FilterFactories;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputBatches();
	Context->OutputPointsAndEdges();
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	return Context->TryComplete();
}

namespace PCGExClusterToZoneGraph
{
	FZGBase::FZGBase(FProcessor* InProcessor)
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

	FZGRoad::FZGRoad(FProcessor* InProcessor, const TSharedPtr<PCGExClusters::FNodeChain>& InChain, const bool InReverse)
		: FZGBase(InProcessor), Chain(InChain), bIsReversed(InReverse)
	{
	}

	void FZGRoad::Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();
		const FZoneShapePointType DefaultPointType = S->RoadPointType;

		TArray<int32> Nodes;
		const int32 ChainSize = Chain->GetNodes(Cluster, Nodes, bIsReversed);

		PCGExArrayHelpers::InitArray(PrecomputedPoints, ChainSize);

		if (Chain->bIsClosedLoop) { Nodes.Add(Nodes.Last()); }

		for (int i = 0; i < ChainSize; i++)
		{
			const FVector Position = Cluster->GetPos(Nodes[i]);
			const FVector NextPosition = (i == ChainSize - 1)
				? Position + (Position - Cluster->GetPos(Nodes[i - 1]))
				: Cluster->GetPos(Nodes[i + 1]);

			FZoneShapePoint ShapePoint = FZoneShapePoint(Position);
			ShapePoint.SetRotationFromForwardAndUp((NextPosition - Position), FVector::UpVector);

			if (Processor->RoadPointTypeBuffer)
			{
				const int32 NodePointIndex = Cluster->GetNode(Nodes[i])->PointIndex;
				ShapePoint.Type = static_cast<FZoneShapePointType>(FMath::Clamp(Processor->RoadPointTypeBuffer->Read(NodePointIndex), 0, 3));
			}
			else
			{
				ShapePoint.Type = DefaultPointType;
			}

			PrecomputedPoints[i] = ShapePoint;
		}

		const PCGExClusters::FNode* FirstNode = Cluster->GetNode(Nodes[0]);
		const PCGExClusters::FNode* LastNode = Cluster->GetNode(Nodes.Last());

		if (!Chain->bIsClosedLoop)
		{
			// Re-read radius from actual traversal nodes (may differ from CompleteWork's logical Start/End when reversed)
			if (Processor->PolygonRadiusBuffer)
			{
				if (!FirstNode->IsLeaf()) { StartRadius = Processor->PolygonRadiusBuffer->Read(FirstNode->PointIndex); }
				if (!LastNode->IsLeaf()) { EndRadius = Processor->PolygonRadiusBuffer->Read(LastNode->PointIndex); }
			}

			if (bIsReversed)
			{
				if (!FirstNode->IsLeaf()) { PrecomputedPoints[0].Position += PrecomputedPoints[0].Rotation.RotateVector(FVector::BackwardVector) * StartRadius; }
				if (!LastNode->IsLeaf()) { PrecomputedPoints.Last().Position += PrecomputedPoints.Last().Rotation.RotateVector(FVector::ForwardVector) * EndRadius; }
			}
			else
			{
				if (!FirstNode->IsLeaf()) { PrecomputedPoints[0].Position += PrecomputedPoints[0].Rotation.RotateVector(FVector::ForwardVector) * StartRadius; }
				if (!LastNode->IsLeaf()) { PrecomputedPoints.Last().Position += PrecomputedPoints.Last().Rotation.RotateVector(FVector::BackwardVector) * EndRadius; }
			}
		}

		CachedLaneProfile = Processor->LaneProfileBuffer
			? Processor->ResolveLaneProfile(FirstNode->PointIndex)
			: S->LaneProfile;
	}

	void FZGRoad::Compile()
	{
		Component->SetShapeType(FZoneShapeType::Spline);
		Component->SetCommonLaneProfile(CachedLaneProfile);
		Component->GetMutablePoints() = MoveTemp(PrecomputedPoints);
		Component->UpdateShape();
	}

	FZGPolygon::FZGPolygon(FProcessor* InProcessor, const PCGExClusters::FNode* InNode)
		: FZGBase(InProcessor), NodeIndex(InNode->Index)
	{
		FromStart.Init(false, InNode->Num());
		Roads.Reserve(InNode->Num());
	}

	void FZGPolygon::Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart)
	{
		FromStart[Roads.Add(InRoad)] = bFromStart;
	}

	void FZGPolygon::Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();
		const auto* P = Processor;
		const PCGExClusters::FNode* Center = Cluster->GetNode(NodeIndex);
		const int32 PointIndex = Center->PointIndex;
		const FVector CenterPosition = Cluster->GetPos(Center);

		CachedRadius = P->PolygonRadiusBuffer ? P->PolygonRadiusBuffer->Read(PointIndex) : S->PolygonRadius;
		CachedRoutingType = P->PolygonRoutingTypeBuffer ? static_cast<EZoneShapePolygonRoutingType>(FMath::Clamp(P->PolygonRoutingTypeBuffer->Read(PointIndex), 0, 1)) : S->PolygonRoutingType;
		CachedPointType = P->PolygonPointTypeBuffer ? static_cast<FZoneShapePointType>(FMath::Clamp(P->PolygonPointTypeBuffer->Read(PointIndex), 0, 3)) : S->PolygonPointType;
		CachedAdditionalTags = P->AdditionalIntersectionTagsBuffer ? FZoneGraphTagMask(static_cast<uint32>(P->AdditionalIntersectionTagsBuffer->Read(PointIndex))) : S->AdditionalIntersectionTags;
		CachedLaneProfile = P->LaneProfileBuffer ? P->ResolveLaneProfile(PointIndex) : S->LaneProfile;

		TArray<int32> Order;
		PCGExArrayHelpers::ArrayOfIndices(Order, Roads.Num());
		Order.Sort(
			[&](const int32 A, const int32 B)
			{
				const FVector DirA = Roads[A]->Chain->GetEdgeDir(Cluster, FromStart[A]);
				const FVector DirB = Roads[B]->Chain->GetEdgeDir(Cluster, FromStart[B]);
				return PCGExMath::GetRadiansBetweenVectors(DirA, FVector::ForwardVector) > PCGExMath::GetRadiansBetweenVectors(DirB, FVector::ForwardVector);
			});

		PCGExArrayHelpers::InitArray(PrecomputedPoints, Order.Num());

		for (int i = 0; i < Order.Num(); i++)
		{
			const int32 Ri = Order[i];
			const TSharedPtr<FZGRoad>& Road = Roads[Ri];

			const PCGExClusters::FNode* OtherNode = (Road->Chain->SingleEdge != -1)
				? (FromStart[Ri] ? Cluster->GetNode(Road->Chain->Links.Last()) : Cluster->GetNode(Road->Chain->Seed.Node))
				: (FromStart[Ri] ? Cluster->GetNode(Road->Chain->Links[0]) : Cluster->GetNode(Road->Chain->Links.Last(1)));

			const FVector RoadDirection = (Cluster->GetPos(OtherNode) - CenterPosition).GetSafeNormal();

			FZoneShapePoint ShapePoint = FZoneShapePoint(CenterPosition + RoadDirection * CachedRadius);
			ShapePoint.SetRotationFromForwardAndUp(RoadDirection * -1, FVector::UpVector);
			ShapePoint.Type = CachedPointType;

			PrecomputedPoints[i] = ShapePoint;
		}
	}

	void FZGPolygon::Compile()
	{
		Component->SetShapeType(FZoneShapeType::Polygon);
		Component->SetPolygonRoutingType(CachedRoutingType);
		Component->SetTags(Component->GetTags() | CachedAdditionalTags);
		Component->SetCommonLaneProfile(CachedLaneProfile);
		Component->GetMutablePoints() = MoveTemp(PrecomputedPoints);
		Component->UpdateShape();
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterToZoneGraph::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade)) { return false; }

		if (Settings->bOverridePolygonRadius) { PolygonRadiusBuffer = VtxDataFacade->GetBroadcaster<double>(Settings->PolygonRadiusAttribute); }
		if (Settings->bOverridePolygonRoutingType) { PolygonRoutingTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(Settings->PolygonRoutingTypeAttribute); }
		if (Settings->bOverridePolygonPointType) { PolygonPointTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(Settings->PolygonPointTypeAttribute); }
		if (Settings->bOverrideRoadPointType) { RoadPointTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(Settings->RoadPointTypeAttribute); }
		if (Settings->bOverrideAdditionalIntersectionTags) { AdditionalIntersectionTagsBuffer = VtxDataFacade->GetBroadcaster<int32>(Settings->AdditionalIntersectionTagsAttribute); }
		if (Settings->bOverrideLaneProfile) { LaneProfileBuffer = VtxDataFacade->GetBroadcaster<FName>(Settings->LaneProfileAttribute); }

		if (VtxFiltersManager)
		{
			PCGEX_ASYNC_GROUP_CHKD(TaskManager, FilterBreakpoints)

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
		bIsProcessorValid = PCGExClusters::ChainHelpers::GetOrBuildChains(
			Cluster.ToSharedRef(),
			ProcessedChains,
			VtxFilterCache,
			false);

		if (!bIsProcessorValid) { return false; }

		Polygons.Reserve(NumNodes / 2);

		return bIsProcessorValid;
	}

	void FProcessor::CompleteWork()
	{
		if (ProcessedChains.IsEmpty())
		{
			bIsProcessorValid = false;
			return;
		}

		TMap<int32, TSharedPtr<FZGPolygon>> Map;

		const int32 NumChains = ProcessedChains.Num();
		const double DefaultPolygonRadius = Settings->PolygonRadius;

		Roads.Reserve(NumChains);

		for (int i = 0; i < NumChains; i++)
		{
			const TSharedPtr<PCGExClusters::FNodeChain>& Chain = ProcessedChains[i];
			if (!Chain) { continue; }

			int32 StartNode = Chain->Seed.Node;
			int32 EndNode = Chain->Links.Last().Node;
			const bool bReverse = DirectionSettings.SortExtrapolation(Cluster.Get(), Chain->Seed.Edge, StartNode, EndNode);

			TSharedPtr<FZGRoad> Road = MakeShared<FZGRoad>(this, Chain, bReverse);
			Roads.Add(Road);

			const PCGExClusters::FNode* Start = Cluster->GetNode(StartNode);
			const PCGExClusters::FNode* End = Cluster->GetNode(EndNode);

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
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(this, Start);
					Polygons.Add(NewPolygon);
					Map.Add(StartNode, NewPolygon);
					PolygonPtr = &NewPolygon;
				}
				(*PolygonPtr)->Add(Road, true);
				Road->StartRadius = DefaultPolygonRadius;
			}

			if (!End->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(EndNode);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(this, End);
					Polygons.Add(NewPolygon);
					Map.Add(EndNode, NewPolygon);
					PolygonPtr = &NewPolygon;
				}

				(*PolygonPtr)->Add(Road, false);
				Road->EndRadius = DefaultPolygonRadius;
			}
		}

		// Precompute all geometry off main thread
		for (const TSharedPtr<FZGPolygon>& Polygon : Polygons) { Polygon->Precompute(Cluster); }
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->Precompute(Cluster); }

		MainThreadToken = TaskManager->TryCreateToken(TEXT("ZGMainThreadToken"));

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
		TargetActor = /*Settings->TargetActor.Get() ? Settings->TargetActor.Get() :*/ ExecutionContext->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			bIsProcessorValid = false;
			return;
		}

		const int32 NumPolygons = Polygons.Num();
		const int32 TotalCount = NumPolygons + Roads.Num();

		if (TotalCount == 0) { return; }

		CachedAttachmentRules = Settings->AttachmentRules.GetRules();

		// Single time-sliced loop: polygons first (indices 0..NumPolygons-1), then roads
		// Road radii are pre-assigned in CompleteWork, so no polygon→road dependency
		MainCompileLoop = MakeShared<PCGExMT::FTimeSlicedMainThreadLoop>(TotalCount);
		MainCompileLoop->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE, NumPolygons](const int32 Index, const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			if (Index < NumPolygons)
			{
				This->Polygons[Index]->InitComponent(This->TargetActor);
				This->Context->AttachManagedComponent(This->TargetActor, This->Polygons[Index]->Component, This->CachedAttachmentRules);
				This->Polygons[Index]->Compile();
			}
			else
			{
				const int32 RoadIndex = Index - NumPolygons;
				This->Roads[RoadIndex]->InitComponent(This->TargetActor);
				This->Context->AttachManagedComponent(This->TargetActor, This->Roads[RoadIndex]->Component, This->CachedAttachmentRules);
				This->Roads[RoadIndex]->Compile();
			}
		};

		MainCompileLoop->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->Context->AddNotifyActor(This->TargetActor);
		};

		PCGEX_ASYNC_HANDLE_CHKD_VOID(TaskManager, MainCompileLoop)
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		// No longer used - road compilation moved to main thread via RoadCompileLoop
	}

	void FProcessor::OnRangeProcessingComplete()
	{
	}

	void FProcessor::Output()
	{
		// Component creation, attachment, and notify are handled in InitComponents()
		// which runs on the main thread via RegisterBeginTickAction.
	}

	FZoneLaneProfileRef FProcessor::ResolveLaneProfile(int32 PointIndex) const
	{
		const FName ProfileName = LaneProfileBuffer->Read(PointIndex);
		if (ProfileName.IsNone()) { return Settings->LaneProfile; }
		if (const FZoneLaneProfileRef* Found = Context->LaneProfileMap.Find(ProfileName))
		{
			return *Found;
		}
		return Settings->LaneProfile;
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>::Cleanup();
		TargetActor = nullptr;
		ProcessedChains.Empty();
		Roads.Empty();
		Polygons.Empty();

		PolygonRadiusBuffer.Reset();
		PolygonRoutingTypeBuffer.Reset();
		PolygonPointTypeBuffer.Reset();
		RoadPointTypeBuffer.Reset();
		AdditionalIntersectionTagsBuffer.Reset();
		LaneProfileBuffer.Reset();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		if (Settings->bOverridePolygonRadius) { FacadePreloader.Register<double>(ExecutionContext, Settings->PolygonRadiusAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (Settings->bOverridePolygonRoutingType) { FacadePreloader.Register<int32>(ExecutionContext, Settings->PolygonRoutingTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (Settings->bOverridePolygonPointType) { FacadePreloader.Register<int32>(ExecutionContext, Settings->PolygonPointTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (Settings->bOverrideRoadPointType) { FacadePreloader.Register<int32>(ExecutionContext, Settings->RoadPointTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (Settings->bOverrideAdditionalIntersectionTags) { FacadePreloader.Register<int32>(ExecutionContext, Settings->AdditionalIntersectionTagsAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (Settings->bOverrideLaneProfile) { FacadePreloader.Register<FName>(ExecutionContext, Settings->LaneProfileAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
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
#undef PCGEX_NAMESPACE
