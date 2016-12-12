#pragma once
#include "Resource.h"

class FRenderGraphNode;
DECORATE_CLASS_REF(FRenderGraphNode);

struct FOutputDesc {
	FRenderTargetView RTV;
	FViewportRect Rect;
};

class FRenderGraphNode : public eastl::enable_shared_from_this<FRenderGraphNode> {
public:
	eastl::vector<FRenderGraphNodeRef> Incoming;
	eastl::vector<FRenderGraphNodeWeakRef> Outgoing;

	bool Processed = false;

	virtual bool IsProcessed();
	virtual bool IsReadyToProcess();

	// for process nodes
	virtual void Process(FCommandsStream & CmdStream) {}

	// for data nodes
	virtual FGPUResource * GetResource() { return nullptr; }
	virtual FOutputDesc GetOutputDesc() { return{}; }
};

class FProcessingNode : public FRenderGraphNode {
public:
	eastl::hash_map<u32, u32> Inputs;
	eastl::hash_map<u32, u32> Outputs;

	virtual void LinkInput(u32 Slot, FRenderGraphNodeRefParam Input);
	virtual void LinkOutput(u32 Slot, FRenderGraphNodeRefParam Output);
	virtual FRenderGraphNode * GetInput(u32 Slot);
	virtual FRenderGraphNode * GetOutput(u32 Slot);
};
DECORATE_CLASS_REF(FProcessingNode);

class FResourceNode : public FRenderGraphNode {
public:
	FGPUResourceRef Resource;

	FResourceNode(FGPUResourceRef InRes) : Resource(InRes) {}

	FGPUResource * GetResource() override { return Resource.get(); }

	FOutputDesc GetOutputDesc() override {
		FOutputDesc desc = {};
		desc.RTV = Resource->GetRTV();
		desc.Rect = Resource->GetSizeAsViewportRect();
		return desc;
	}
};
DECORATE_CLASS_REF(FResourceNode);

FRenderGraphNodeRef CreateDataNode(FGPUResourceRefParam Resource);

void ProcessGraph(FCommandsStream & CmdStream, FRenderGraphNode * FinalOutput);