#include "RenderGraph.h"

bool FRenderGraphNode::IsProcessed() {
	return Processed;
}

bool FRenderGraphNode::IsReadyToProcess() {
	for (auto & Input : Incoming) {
		if (!Input->IsProcessed()) {
			return false;
		}
	}
	return true;
}

void FProcessingNode::LinkInput(u32 Slot, FRenderGraphNodeRefParam Input) {
	Incoming.push_back(Input);
	Inputs[Slot] = (u32)Incoming.size() - 1;

	Input->Outgoing.push_back(shared_from_this());
}

void FProcessingNode::LinkOutput(u32 Slot, FRenderGraphNodeRefParam Output) {
	Outgoing.push_back(Output);
	Outputs[Slot] = (u32)Outgoing.size() - 1;

	Output->Incoming.push_back(shared_from_this());
}

FRenderGraphNode * FProcessingNode::GetInput(u32 Slot) { 
	return Incoming[Inputs[Slot]].get(); 
}

FRenderGraphNode * FProcessingNode::GetOutput(u32 Slot) { 
	return Outgoing[Inputs[Slot]].lock().get(); 
}



#include <EASTL\hash_map.h>
#include <EASTL\stack.h>
#include <EASTL\deque.h>

void PreprocessGraph(FRenderGraphNode * FinalOutput, eastl::deque<FRenderGraphNode*> & Sources) {
	eastl::hash_map<FRenderGraphNode*, u8> Nodes;
	eastl::stack<FRenderGraphNode*> Stack;

	Stack.push(FinalOutput);
	Nodes[FinalOutput] = 0;

	while (!Stack.empty()) {
		FRenderGraphNode * Node = Stack.top();
		Stack.pop();
		Node->Processed = false;

		auto NodeIter = Nodes.find(Node);

		Nodes[Node] = 1;
		if (Node->Incoming.size() == 0) {
			Sources.push_back(Node);
		}

		for (auto & KV : Node->Incoming) {
			auto ChildIter = Nodes.find(KV.get());
			if (ChildIter == Nodes.end()) {
				Nodes[KV.get()] = 0;
				Stack.push(KV.get());
			}
			else if (ChildIter->second == 1) {
				// cycle
				check(0);
			}
		}
	}
}

void ProcessGraph(FCommandsStream & CmdStream, FRenderGraphNode * FinalOutput) {
	eastl::deque<FRenderGraphNode*> Queue;

	PreprocessGraph(FinalOutput, Queue);

	while (Queue.size()) {
		FRenderGraphNode * Node = Queue.front();
		Queue.pop_front();

		Node->Process(CmdStream);
		Node->Processed = true;

		for (auto & KV : Node->Outgoing) {
			if (KV.lock()->IsReadyToProcess()) {
				Queue.push_back(KV.lock().get());
			}
		}
	}

	check(FinalOutput->Processed);
}

FRenderGraphNodeRef CreateDataNode(FGPUResourceRefParam Resource) {
	return eastl::make_shared<FResourceNode>(Resource);
}