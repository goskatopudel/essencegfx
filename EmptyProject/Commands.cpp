#include "Commands.h"
#include "Shader.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include <EASTL/array.h>
#include "d3dx12.h"
#include "VideoMemory.h"
#include "Pipeline.h"
#include "Print.h"

#define WORKLOAD_STATS 1
#define API_STATS 1

namespace API_CPUCost {
const u32	Barrier = 1;
const u32	SetPso = 1;
const u32	SetRoot = 1;
const u32	Draw = 1;
const u32	SetRootParam = 1;
}

commands_stats_t& operator += (commands_stats_t& lhs, commands_stats_t const& rhs) {
	lhs.graphic_pipeline_state_changes += rhs.graphic_pipeline_state_changes;
	lhs.graphic_root_signature_changes += rhs.graphic_root_signature_changes;
	lhs.graphic_root_params_set += rhs.graphic_root_params_set;
	lhs.draw_calls += rhs.draw_calls;
	lhs.compute_pipeline_state_changes += rhs.compute_pipeline_state_changes;
	lhs.compute_root_signature_changes += rhs.compute_root_signature_changes;
	lhs.compute_root_params_set += rhs.compute_root_params_set;
	lhs.dispatches += rhs.dispatches;
	lhs.constants_bytes_uploaded += rhs.constants_bytes_uploaded;
	return lhs;
}

class CommandListPool;

class CommandAllocator {
public:
	enum StateEnum {
		ReadyState,
		RecordingState
	};

	unique_com_ptr<ID3D12CommandAllocator>		D12CommandAllocator;
	StateEnum									State;
	eastl::queue<SyncPoint>						Fences;
	CommandListPool*							Owner;
	EContextLifetime							Lifetime;
	u32											ApproximateWorkload = 0;

	CommandAllocator(CommandListPool* owner, EContextLifetime lifetime);
	void Recycle(SyncPoint fence);
	bool CanReset();
	void Reset();
};

struct GPUFence {
	GPUCommandQueue*	Queue;
	u64					Value;
};

const u32 MAX_PENDING_FENCES = 4096;
eastl::array<GPUFence, MAX_PENDING_FENCES>	FencesPool;
eastl::array<u32, MAX_PENDING_FENCES>		FenceGenerations;
u64											FencesCounter;

const u32 DUMMY_SYNC_POINT = 0xFFFFFFFF;

bool SyncPoint::IsCompleted() {
	if (Index == DUMMY_SYNC_POINT) {
		return true;
	}

	if (FenceGenerations[Index] != Generation) {
		return true;
	}

	if (FencesPool[Index].Queue == nullptr) {
		return false;
	}

	return FencesPool[Index].Value <= FencesPool[Index].Queue->GetCompletedValue();
}

void SyncPoint::WaitForCompletion() {
	if (!IsCompleted()) {
		FencesPool[Index].Queue->WaitForCompletion(FencesPool[Index].Value);
	}
}

void SyncPoint::SetTrigger(GPUCommandQueue* queue) {
	check(FencesPool[Index].Queue == nullptr);
	FencesPool[Index].Queue = queue;
	FencesPool[Index].Value = queue->AdvanceSyncValue();
}

void SyncPoint::SetTrigger(GPUCommandQueue* queue, u64 value) {
	check(FencesPool[Index].Queue == nullptr);
	FencesPool[Index].Queue = queue;
	FencesPool[Index].Value = value;
}

SyncPoint CreateSyncPoint() {
	u32 index = (FencesCounter++) % MAX_PENDING_FENCES;

	if (FencesPool[index].Queue) {
		check(FencesPool[index].Value <= FencesPool[index].Queue->GetCompletedValue());
	}

	FencesPool[index].Queue = nullptr;
	FencesPool[index].Value = 0;
	auto generation = ++FenceGenerations[index];

	return SyncPoint(index, generation);
}

bool SyncPoint::IsSet() {
	return Index == DUMMY_SYNC_POINT || FencesPool[Index].Queue != nullptr;
}

SyncPoint GetDummySyncPoint() {
	return SyncPoint(DUMMY_SYNC_POINT, 0);
}

bool operator == (SyncPoint A, SyncPoint B) {
	return A.Index == B.Index && A.Generation == B.Generation;
}

bool operator != (SyncPoint A, SyncPoint B) {
	return A.Index != B.Index || A.Generation != B.Generation;
}

class GPUCommandList {
public:
	enum StateEnum {
		UnassignedState,
		RecordingState,
		ClosedState,
		ExecutedState
	};

	StateEnum									State;
	unique_com_ptr<ID3D12GraphicsCommandList>	D12CommandList;
	CommandAllocator*							Allocator;
	SyncPoint									SyncPoint;
	CommandListPool*							Owner;
	u32											ApproximateWorkload = 0;

	GPUCommandList(CommandListPool* owner, CommandAllocator* allocator);
	void Close();
	void ReleaseAllocatorAfterRecording();
	void Recycle();
	void Reset(CommandAllocator* allocator);
};

class CommandListPool {
public:
	using AllocatorsPool = eastl::array<eastl::queue<eastl::unique_ptr<CommandAllocator>>, (u32)EContextLifetime::CONTEXT_LIFETIMES_COUNT>;

	D3D12_COMMAND_LIST_TYPE								Type;
	eastl::vector<eastl::unique_ptr<GPUCommandList>>	CommandLists;
	AllocatorsPool										ReadyAllocators;
	AllocatorsPool										PendingAllocators;
	u32													ListsNum = 0;
	u32													AllocatorsNum = 0;

	CommandAllocator* ObtainAllocator(EContextLifetime lifetime) {
		decltype(ReadyAllocators[0])& AllocatorPool = ReadyAllocators[(u32)lifetime];
	
		if (!AllocatorPool.size()) {
			AllocatorPool.emplace_back(new CommandAllocator(this, lifetime));
			AllocatorsNum++;
			PrintFormated(L"Creating CommandAllocator, type: %u, lifetime: %u, (%u)\n", Type, lifetime, AllocatorsNum);
		}

		auto allocator = AllocatorPool.front().release();
		AllocatorPool.pop();
		return allocator;
	}

	void Recycle(GPUCommandList* commandList) {
		CommandLists.emplace_back(commandList);
	}

	void Recycle(CommandAllocator* allocator) {
		if (allocator->Lifetime == EContextLifetime::FRAME) {
			ReadyAllocators[(u32)EContextLifetime::FRAME].emplace_back(allocator);
		}
		else {
			PendingAllocators[(u32)EContextLifetime::ASYNC].emplace_back(allocator);
		}
	}

	GPUCommandList*	ObtainList(EContextLifetime lifetime) {
		if (!CommandLists.size()) {
			ListsNum++;
			PrintFormated(L"Creating CommandList, type: %u, lifetime: %u, (%u)\n", Type, lifetime, ListsNum);
			return new GPUCommandList(this, ObtainAllocator(lifetime));
		}

		CommandLists.back()->Reset(ObtainAllocator(lifetime));
		auto result = CommandLists.back().release();
		CommandLists.pop_back();
		return result;
	}

	void ReturnList(GPUCommandList* list) {
		CommandLists.emplace_back(list);
	}

	void ResetAllocators() {
		for (auto& allocatorsRange : ReadyAllocators) {
			while (!allocatorsRange.empty()) {
				PendingAllocators[(u32)allocatorsRange.front()->Lifetime].emplace_back(std::move(allocatorsRange.front()));
				allocatorsRange.pop();
			}
		}

		for (auto& allocatorsRange : PendingAllocators) {
			while (!allocatorsRange.empty() && allocatorsRange.front()->CanReset()) {
				allocatorsRange.front()->Reset();
				ReadyAllocators[(u32)allocatorsRange.front()->Lifetime].emplace_back(std::move(allocatorsRange.front()));
				allocatorsRange.pop();
			}
		}
	}
};

CommandAllocator::CommandAllocator(CommandListPool* owner, EContextLifetime lifetime) : Owner(owner), Lifetime(lifetime), State(ReadyState) {
	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommandAllocator(Owner->Type, IID_PPV_ARGS(D12CommandAllocator.get_init())));
	VERIFYDX12(D12CommandAllocator->Reset());
}

void CommandAllocator::Recycle(SyncPoint fence) {
	check(State == RecordingState);
	Fences.push(fence);
	State = ReadyState;
	Owner->Recycle(this);
}

bool CommandAllocator::CanReset() {
	while (Fences.size() && Fences.front().IsCompleted()) {
		Fences.pop();
	}
	return Fences.size() == 0;
}

void CommandAllocator::Reset() {
	check(State == ReadyState && Fences.size() == 0);
	VERIFYDX12(D12CommandAllocator->Reset());
}

GPUCommandList::GPUCommandList(CommandListPool* owner, CommandAllocator* allocator) : State(RecordingState), Owner(owner), Allocator(allocator) {
	check(Allocator->State == CommandAllocator::ReadyState);
	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommandList(0, Owner->Type, allocator->D12CommandAllocator.get(), nullptr, IID_PPV_ARGS(D12CommandList.get_init())));
	Allocator->State = CommandAllocator::RecordingState;
	State = RecordingState;
	SyncPoint = CreateSyncPoint();
}

void GPUCommandList::Close() {
	check(State == RecordingState);
	check(Allocator->State == CommandAllocator::RecordingState);
	State = ClosedState;
	VERIFYDX12(D12CommandList->Close());
	ReleaseAllocatorAfterRecording();
}

void GPUCommandList::ReleaseAllocatorAfterRecording() {
	check(State == ClosedState);
	Allocator->ApproximateWorkload = eastl::max(ApproximateWorkload, Allocator->ApproximateWorkload);
	Allocator->Recycle(SyncPoint);
	Allocator = nullptr;
}

void GPUCommandList::Recycle() {
	check(Allocator == nullptr);
	Owner->Recycle(this);
}

void GPUCommandList::Reset(CommandAllocator* allocator) {
	Allocator = allocator;

	check(State == ExecutedState);
	check(Allocator->State == CommandAllocator::ReadyState);
	VERIFYDX12(D12CommandList->Reset(allocator->D12CommandAllocator.get(), nullptr));
	Allocator->State = CommandAllocator::RecordingState;
	State = RecordingState;
	ApproximateWorkload = 0;

	SyncPoint = CreateSyncPoint();
}

GPUCommandQueue::GPUCommandQueue(D3D12Device* device, TypeEnum type) :
	Type(type), Device(device), LastSignaledValue(0), CurrentSyncValue(1), CachedLastCompletedValue(0)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(queueDesc));
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;

	switch (Type) {
	case COMPUTE:
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case COPY:
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	case GRAPHICS:
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	VERIFYDX12(Device->D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(D12CommandQueue.get_init())));
	VERIFYDX12(Device->D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(D12Fence.get_init())));
}

void GPUCommandQueue::WaitForCompletion(u64 syncValue) {
	if (D12Fence->GetCompletedValue() < syncValue) {
		thread_local HANDLE SyncEvent = INVALID_HANDLE_VALUE;
		if (SyncEvent == INVALID_HANDLE_VALUE) {
			SyncEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		}

		VERIFYDX12(D12Fence->SetEventOnCompletion(syncValue, SyncEvent));
		WaitForSingleObject(SyncEvent, INFINITE);
	}
}

void GPUCommandQueue::Wait(SyncPoint fence) {
	check(fence.IsSet());
	VERIFYDX12(D12CommandQueue->Wait(D12Fence.get(), FencesPool[fence.Index].Value));
}

void GPUCommandQueue::Execute(GPUCommandList* list) {
	QueuedCommandLists.push_back(list);
}

void GPUCommandQueue::Flush() {
	if (!QueuedCommandLists.size()) {
		return;
	}
	eastl::vector<ID3D12CommandList*> ExecutionList;
	ExecutionList.reserve((u32)QueuedCommandLists.size());

	for (auto commandList : QueuedCommandLists) {
		check(commandList->State == GPUCommandList::ClosedState);
		ExecutionList.push_back(commandList->D12CommandList.get());
	}

	D12CommandQueue->ExecuteCommandLists((u32)ExecutionList.size(), (ID3D12CommandList**)ExecutionList.data());

	u64 syncValue = AdvanceSyncValue();
	for (auto commandList : QueuedCommandLists) {
		commandList->State = GPUCommandList::ExecutedState;
		commandList->SyncPoint.SetTrigger(this, syncValue);
		commandList->Recycle();
	}

	QueuedCommandLists.clear();
}

void GPUCommandQueue::WaitForCompletion() {
	WaitForCompletion(LastSignaledValue);
}

u64	GPUCommandQueue::GetCurrentSyncValue() const {
	return CurrentSyncValue;
}

u64 GPUCommandQueue::GetCompletedValue() {
	CachedLastCompletedValue = D12Fence->GetCompletedValue();
	return CachedLastCompletedValue;
}

u64 GPUCommandQueue::AdvanceSyncValue() {
	LastSignaledValue = CurrentSyncValue;
	VERIFYDX12(D12CommandQueue->Signal(D12Fence.get(), LastSignaledValue));
	CurrentSyncValue++;
	return LastSignaledValue;
}

SyncPoint GPUCommandQueue::GenerateSyncPoint() {
	SyncPoint result = CreateSyncPoint();
	result.SetTrigger(this);
	return result;
}

eastl::unique_ptr<GPUCommandQueue>	DirectQueue;
eastl::unique_ptr<GPUCommandQueue>	CopyQueue;
eastl::unique_ptr<GPUCommandQueue>	ComputeQueue;

CommandListPool					DirectPool;
CommandListPool					CopyPool;
CommandListPool					ComputePool;

GPUCommandQueue*		GetDirectQueue() {
	if (!DirectQueue.get()) {
		DirectQueue.reset(new GPUCommandQueue(GetPrimaryDevice(), GPUCommandQueue::GRAPHICS));
	}
	return DirectQueue.get();
}

GPUCommandQueue*		GetComputeQueue() {
	if (!CopyQueue.get()) {
		CopyQueue.reset(new GPUCommandQueue(GetPrimaryDevice(), GPUCommandQueue::COMPUTE));
	}
	return CopyQueue.get();
}

GPUCommandQueue*		GetCopyQueue() {
	if (!ComputeQueue.get()) {
		ComputeQueue.reset(new GPUCommandQueue(GetPrimaryDevice(), GPUCommandQueue::COPY));
	}
	return ComputeQueue.get();
}

void FGPUContext::Close() {
	FlushBarriers();
	check(CommandList->State == GPUCommandList::RecordingState);
	CommandList->Close();
}

void FGPUContext::Execute() {
	if (CommandList->State == GPUCommandList::RecordingState) {
		Close();
	}
	check(CommandList->State == GPUCommandList::ClosedState);
	if (BarriersList.size()) {
		for (u32 Index = 0; Index < BarriersList.size(); Index++) {
			if (BarriersList[Index].Resource->FatData->AutomaticBarriers) {
				GetResourceStateRegistry()->SetCurrentState(BarriersList[Index].Resource, BarriersList[Index].Subresource, BarriersList[Index].To);
			}
		}
	}
	Queue->Execute(CommandList);
	CommandList = nullptr;
}

void FGPUContext::ExecuteImmediately() {
	Execute();
	Queue->Flush();
}

SyncPoint FGPUContext::GetCompletionSyncPoint() {
	return CommandList->SyncPoint;
}

u32 HasFlag(EAccessType A, EAccessType B) {
	return (u32)(A & B) > 0 ? 1 : 0;
}

D3D12_RESOURCE_STATES GetAPIResourceState(EAccessType Access) {
	check(Access != EAccessType::UNSPECIFIED);
	D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
	const D3D12_RESOURCE_STATES NULL_FLAG = D3D12_RESOURCE_STATE_COMMON;
	State |= HasFlag(Access, EAccessType::WRITE_DEPTH) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::WRITE_RT) ? D3D12_RESOURCE_STATE_RENDER_TARGET : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::WRITE_UAV) ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::READ_DEPTH) ? D3D12_RESOURCE_STATE_DEPTH_READ : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::READ_NON_PIXEL) ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::READ_PIXEL) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::COPY_DEST) ? D3D12_RESOURCE_STATE_COPY_DEST : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::COPY_SRC) ? D3D12_RESOURCE_STATE_COPY_SOURCE : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::READ_IB) ? D3D12_RESOURCE_STATE_INDEX_BUFFER : NULL_FLAG;
	State |= HasFlag(Access, EAccessType::READ_VB_CB) ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : NULL_FLAG;
	return State;
}

void FGPUContext::Barriers(FResourceBarrier const * Barriers, u32 Num) {
	for (u32 Index = 0; Index < Num; Index++) {
		BarriersList.push_back(Barriers[Index]);
	}
}

void FGPUContext::Barrier(FGPUResource* resource, u32 subresource, EAccessType before, EAccessType after) {
	FResourceBarrier Barrier = {};
	Barrier.Resource = resource;
	Barrier.Subresource = subresource;
	Barrier.From = before;
	Barrier.To = after;
	BarriersList.push_back(Barrier);
}

void FGPUContext::FlushBarriers() {
	if (BarriersList.size() - FlushCounter) {
		eastl::vector<D3D12_RESOURCE_BARRIER> ScratchMem;
		ScratchMem.reserve(BarriersList.size() - FlushCounter);

		for (u32 Index = FlushCounter; Index < BarriersList.size(); Index++) {
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = BarriersList[Index].Resource->D12Resource.get();
			barrier.Transition.StateBefore = GetAPIResourceState(BarriersList[Index].From);
			barrier.Transition.StateAfter = GetAPIResourceState(BarriersList[Index].To);
			barrier.Transition.Subresource = BarriersList[Index].Subresource;

		}
		FlushCounter = (u32)BarriersList.size();
		RawCommandList()->ResourceBarrier((u32)ScratchMem.size(), ScratchMem.data());
	}
}

void FGPUContext::CopyDataToSubresource(FGPUResource* Dst, u32 Subresource, void const * Src, u64 RowPitch, u64 SlicePitch) {
	FlushBarriers();

	D3D12_SUBRESOURCE_DATA SubresourceData;
	SubresourceData.pData = Src;
	SubresourceData.RowPitch = RowPitch;
	SubresourceData.SlicePitch = SlicePitch;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
	u32 numRows;
	u64 rowPitch;
	u64 bytesTotal;
	GetPrimaryDevice()->D12Device->GetCopyableFootprints(&Dst->FatData->Desc, Subresource, 1, 0, &Footprint, &numRows, &rowPitch, &bytesTotal);
	auto uploadBuffer = GetUploadAllocator()->CreateBuffer(bytesTotal, 0);
	u32 reeef = uploadBuffer->GetRefCount();
	D3D12_MEMCPY_DEST dest = { (u8*)uploadBuffer->FatData->CpuPtr + Footprint.Offset, Footprint.Footprint.RowPitch, Footprint.Footprint.RowPitch * numRows };
	MemcpySubresource(&dest, &SubresourceData, rowPitch, numRows, Footprint.Footprint.Depth);

	D3D12_TEXTURE_COPY_LOCATION SrcLocation;
	SrcLocation.pResource = uploadBuffer->D12Resource.get();
	SrcLocation.PlacedFootprint = Footprint;
	SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	D3D12_TEXTURE_COPY_LOCATION DstLocation;
	DstLocation.pResource = Dst->D12Resource.get();
	DstLocation.SubresourceIndex = Subresource;
	DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	RawCommandList()->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);
	uploadBuffer.Release(GetCompletionSyncPoint());
}

void FGPUContext::CopyToBuffer(FGPUResource * Dst, void const* Src, u64 Size) {
	FlushBarriers();

	D3D12_SUBRESOURCE_DATA SubresourceData;
	SubresourceData.pData = Src;
	SubresourceData.RowPitch = Size;
	SubresourceData.SlicePitch = Size;

	auto uploadBuffer = GetUploadAllocator()->CreateBuffer(Size, 0);
	memcpy(uploadBuffer->GetMappedPtr(), Src, Size);

	RawCommandList()->CopyBufferRegion(Dst->D12Resource.get(), 0, uploadBuffer->D12Resource.get(), 0, Size);

	uploadBuffer.Release(GetCompletionSyncPoint());
}

ID3D12GraphicsCommandList* FGPUContext::RawCommandList() const {
	return CommandList->D12CommandList.get();
}

void FGPUContext::Open(EContextType Type, EContextLifetime Lifetime) {
	if (Lifetime == EContextLifetime::DEFAULT) {
		switch (Type) {
		case EContextType::DIRECT:
			Lifetime = EContextLifetime::FRAME;
			break;
		case EContextType::COMPUTE:
		case EContextType::COPY:
			Lifetime = EContextLifetime::ASYNC;
			break;
		}
	}

	if (Type == EContextType::DIRECT) {
		CommandList = DirectPool.ObtainList(Lifetime);
		Queue = GetDirectQueue();
		Device = GetPrimaryDevice()->D12Device.get();

		Reset();

		ID3D12DescriptorHeap * Heaps[] = { GetOnlineDescriptorsAllocator()->D12DescriptorHeap.get() };
		RawCommandList()->SetDescriptorHeaps(_countof(Heaps), Heaps);

		SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		SetScissorRect(CD3DX12_RECT(0, 0, 32768, 32768));
	}
	else if (Type == EContextType::COMPUTE) {
		CommandList = ComputePool.ObtainList(Lifetime);
		Queue = GetComputeQueue();
		Device = GetPrimaryDevice()->D12Device.get();

		Reset();

		ID3D12DescriptorHeap * Heaps[] = { GetOnlineDescriptorsAllocator()->D12DescriptorHeap.get() };
		RawCommandList()->SetDescriptorHeaps(_countof(Heaps), Heaps);
	}
	else if (Type == EContextType::COPY) {
		check(0);
	}
}

void FGPUContext::ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float4 color) {
	CommandList->D12CommandList->ClearRenderTargetView(rtv, &color.x, 0, nullptr);
}

void FGPUContext::ClearDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, u8 stencil) {
	CommandList->D12CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}

void FGPUContext::CopyResource(FGPUResource* dst, FGPUResource* src) {
	FlushBarriers();
	RawCommandList()->CopyResource(dst->D12Resource.get(), src->D12Resource.get());
}

void FGPUContext::SetPSO(FPipelineState const* pso) {
	if (PipelineState != pso) {
		PipelineState = pso;
		PipelineType = pso->Type;
		RawCommandList()->SetPipelineState(GetRawPSO(pso));
	}
}

void FGPUContext::SetRoot(FRootLayout const* rootLayout) {
	if (RootLayout != rootLayout) {
		RootLayout = rootLayout;

		if (RootSignature != RootLayout->RootSignature) {
			RootSignature = RootLayout->RootSignature;
			RawCommandList()->SetGraphicsRootSignature(GetRawRootSignature(rootLayout));

			for (u32 index = 0; index < RootLayout->RootParamsNum; index++) {
				u32 L = RootLayout->RootParams[index].TableLen;
				RootParams[index].TableLen = L;
				RootParams[index].SrcRanges.resize(L);
				RootParams[index].SrcRanges = RootLayout->RootParams[index].NullHandles;
				RootParams[index].SrcRangesNums.resize(L, 1);
				RootParams[index].Dirty = true;
			}

			RootParamsNum = RootLayout->RootParamsNum;
		}
	}
}

void FGPUContext::SetTopology(D3D_PRIMITIVE_TOPOLOGY topology) {
	if (Topology != topology) {
		Topology = topology;
		RawCommandList()->IASetPrimitiveTopology(topology);
	}
}

void FGPUContext::SetDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
	if (DSV != dsv) {
		DirtyRTVs = true;
		UsesDepth = dsv.ptr > 0;
		DSV = dsv;
	}
}

void FGPUContext::SetRenderTarget(u32 index, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
	if (RTVs[index] == rtv) {
		return;
	}

	RTVs[index] = rtv;
	DirtyRTVs = true;
	if (rtv.ptr) {
		NumRenderTargets = eastl::max(NumRenderTargets, index + 1);
	}
	else {
		i32 maxIndex = NumRenderTargets;
		for (i32 i = (i32)NumRenderTargets - 1; i >= 0; --i) {
			if (!RTVs[i].ptr) {
				--NumRenderTargets;
				check(i > 0 || NumRenderTargets == 0);
			}
			else {
				break;
			}
		}
	}
}

void FGPUContext::SetViewport(D3D12_VIEWPORT viewport) {
	if (Viewport != viewport) {
		Viewport = viewport;
		RawCommandList()->RSSetViewports(1, &viewport);
	}
}

bool operator != (D3D12_RECT const& A, D3D12_RECT const& B) {
	return A.bottom != B.bottom || A.left != B.left || A.right != B.right || A.top != B.top;
}

void FGPUContext::SetScissorRect(D3D12_RECT rect) {
	if (ScissorRect != rect) {
		RawCommandList()->RSSetScissorRects(1, &rect);
	}
}

void FGPUContext::Draw(u32 vertexCount, u32 startVertex, u32 instances, u32 startInstance) {
	PreDraw();
	RawCommandList()->DrawInstanced(vertexCount, instances, startVertex, startInstance);
}

void FGPUContext::DrawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex, u32 instances, u32 startInstance) {
	PreDraw();
	RawCommandList()->DrawIndexedInstanced(indexCount, instances, startIndex, baseVertex, startInstance);
}

bool operator != (D3D12_VERTEX_BUFFER_VIEW const& A, D3D12_VERTEX_BUFFER_VIEW const & B) {
	return A.BufferLocation != B.BufferLocation || A.SizeInBytes != B.SizeInBytes || A.StrideInBytes != B.StrideInBytes;
}

bool operator != (D3D12_INDEX_BUFFER_VIEW const& A, D3D12_INDEX_BUFFER_VIEW const & B) {
	return A.BufferLocation != B.BufferLocation || A.SizeInBytes != B.SizeInBytes || A.Format != B.Format;
}

void FGPUContext::SetVB(FBufferLocation BufferView, u32 Stream) {
	D3D12_VERTEX_BUFFER_VIEW VBV;
	VBV.BufferLocation = BufferView.Address;
	VBV.StrideInBytes = BufferView.Stride;
	VBV.SizeInBytes = BufferView.Size;
	if (VBVs[Stream] != VBV) {
		DirtyVBVs = 1;
		VBVs[Stream] = VBV;
		if (VBV.BufferLocation) {
			NumVertexBuffers = eastl::max(NumVertexBuffers, Stream + 1);
		}
		else {
			NumVertexBuffers--;
			for (i32 index = Stream - 1; index >= 0; index--) {
				if (VBVs[index].BufferLocation) {
					break;
				}
				else {
					NumVertexBuffers--;
				}
			}
		}
	}
}

void FGPUContext::SetIB(FBufferLocation BufferView) {
	D3D12_INDEX_BUFFER_VIEW NewIBV;
	NewIBV.BufferLocation = BufferView.Address;
	NewIBV.Format = BufferView.Stride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	NewIBV.SizeInBytes = BufferView.Size;
	if (IBV != NewIBV) {
		IBV = NewIBV;
		RawCommandList()->IASetIndexBuffer(&IBV);
	}
}

void FGPUContext::Dispatch(u32 X, u32 Y, u32 Z) {
	PreDraw();
	RawCommandList()->Dispatch(X, Y, Z);
}

void FGPUContext::Reset() {
	FlushCounter = 0;
	BarriersList.clear();

	RootLayout = nullptr;
	RootSignature = nullptr;
	PipelineState = nullptr;
	OnlineDescriptors = GetOnlineDescriptorsAllocator();

	for (auto & Param : RootParams) {
		Param = {};
	}
	RootParamsNum = 0;

	memset(RTVs, 0, sizeof(RTVs));
	NumRenderTargets = 0;
	DSV = {};
	UsesDepth = false;
	DirtyRTVs = false;
	DirtyVBVs = false;
	Viewport = {};
	ScissorRect = {};
	Topology = {};
	IBV = {};
	memset(VBVs, 0, sizeof(VBVs));
	NumVertexBuffers = 0;
}

void FGPUContext::SetConstantBuffer(FConstantBuffer const * ConstantBuffer, D3D12_CPU_DESCRIPTOR_HANDLE CBV) {
	auto BindIter = RootLayout->ConstantBuffers.find(ConstantBuffer->BindId);
	if (BindIter == RootLayout->ConstantBuffers.end()) {
		return;
	}
	auto bind = BindIter->second.Bind;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= Param.SrcRanges[bind.DescOffset] != CBV;
	Param.SrcRanges[bind.DescOffset] = CBV;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void FGPUContext::SetTexture(FTextureParam const * Texture, D3D12_CPU_DESCRIPTOR_HANDLE View) {
	auto BindIter = RootLayout->Textures.find(Texture->BindId);
	if (BindIter == RootLayout->Textures.end()) {
		return;
	}
	auto bind = RootLayout->Textures.find(Texture->BindId)->second;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= View != Param.SrcRanges[bind.DescOffset];
	Param.SrcRanges[bind.DescOffset] = View;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void FGPUContext::SetRWTexture(FRWTextureParam const * RWTexture, D3D12_CPU_DESCRIPTOR_HANDLE View) {
	auto bind = RootLayout->RWTextures.find(RWTexture->BindId)->second;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= View != Param.SrcRanges[bind.DescOffset];
	Param.SrcRanges[bind.DescOffset] = View;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void FGPUContext::PreDraw() {
	FlushBarriers();

	if (PipelineType == EPipelineType::Graphics) {
		if (DirtyRTVs) {
			RawCommandList()->OMSetRenderTargets(NumRenderTargets, RTVs, false, UsesDepth ? &DSV : nullptr);
			DirtyRTVs = false;
		}

		if (DirtyVBVs) {
			RawCommandList()->IASetVertexBuffers(0, NumVertexBuffers, VBVs);
			DirtyVBVs = false;
		}
	}

	for (u32 index = 0; index < RootParamsNum; ++index) {
		auto &Param = RootParams[index];
		if (Param.Dirty) {
			Param.Dirty = false;
			Param.Descriptors = OnlineDescriptors->FastTemporaryAllocate(Param.TableLen);
			auto destHandle = Param.Descriptors.GetCPUHandle(0);

			Device->CopyDescriptors(1, &destHandle, &Param.Descriptors.DescriptorsNum, Param.Descriptors.DescriptorsNum, Param.SrcRanges.data(), Param.SrcRangesNums.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (PipelineType == EPipelineType::Graphics) {
				RawCommandList()->SetGraphicsRootDescriptorTable(index, Param.Descriptors.GetGPUHandle(0));
			}
			else {
				RawCommandList()->SetComputeRootDescriptorTable(index, Param.Descriptors.GetGPUHandle(0));
			}
		}
	}
}

//#include <intrin.h>
//
//u32 GetThreadId() {
//	// from handmade hero
//	auto TLS = (u8*)__readgsqword(0x30);
//	return *(u32*)(TLS + 0x48);
//}

eastl::queue<SyncPoint>	PendingFrameSyncPoints;
u32						MaxBufferedFrames = 3;
bool					IsFrameSyncPointCreated = false;
SyncPoint				LastFrameSyncPoint;

SyncPoint GetCurrentFrameSyncPoint() {
	// todo: threadsafe
	if (!IsFrameSyncPointCreated) {
		IsFrameSyncPointCreated = true;

		PendingFrameSyncPoints.push(CreateSyncPoint());
	}

	return PendingFrameSyncPoints.back();
}

SyncPoint GetLastFrameSyncPoint() {
	check(LastFrameSyncPoint.IsSet());
	return LastFrameSyncPoint;
}

void EndFrame() {
	DirectPool.ResetAllocators();
	CopyPool.ResetAllocators();
	ComputePool.ResetAllocators();

	SyncPoint FrameEndSync = GetCurrentFrameSyncPoint();
	FrameEndSync.SetTrigger(GetDirectQueue(), GetDirectQueue()->LastSignaledValue);
	LastFrameSyncPoint = FrameEndSync;

	GetOnlineDescriptorsAllocator()->FenceTemporaryAllocations(FrameEndSync);
	GetConstantsAllocator()->FenceFrameAllocations(FrameEndSync);

	GetOnlineDescriptorsAllocator()->Tick();
	GetTexturesAllocator()->Tick();
	GetConstantsAllocator()->Tick();
	GetUploadAllocator()->Tick();
	GetBuffersAllocator()->Tick();

	TickDescriptors(FrameEndSync);

	IsFrameSyncPointCreated = false;
	PendingFrameSyncPoints.push(FrameEndSync);
	while (PendingFrameSyncPoints.size() > MaxBufferedFrames) {
		PendingFrameSyncPoints.front().WaitForCompletion();
		PendingFrameSyncPoints.pop();
	}
}

eastl::unique_ptr<FResourceStateRegistry>	GResourceStateRegistry;

FResourceStateRegistry * GetResourceStateRegistry() {
	if (!GResourceStateRegistry.get()) {
		GResourceStateRegistry.reset(new FResourceStateRegistry());
	}
	return GResourceStateRegistry.get();
}

void ProcessResourceBarriers(
	FGPUResource * Resource,
	eastl::vector<FResourceAccessNode>& InitialNodes,
	eastl::vector<FResourceAccess> const& Requests,
	eastl::hash_map<u32, eastl::vector<FResourceBarrier>> &OutBarriers
	) {
	auto & Nodes = InitialNodes;

	u32 LastAllSubresNode = 0;
	eastl::hash_map<u32, u32> LastSubresourceNode;
	u32 LastComplementaryNode = -1;

	if (Nodes[0].Complementary == 1) {
		LastAllSubresNode = -1;
		LastComplementaryNode = 0;

		for (u32 Index = 1; Index < Nodes.size(); Index++) {
			LastSubresourceNode[Nodes[Index].Subresource] = Index;
		}
	}

	auto SpawnNode = [&](u32 Prev, EAccessType Access, u32 Subres, u32 BatchIndex) {
		FResourceAccessNode NewNode = {};
		NewNode.Access = Access;
		if (IsReadAccess(Access) && IsReadAccess(Nodes[Prev].Access)) {
			NewNode.Access |= Nodes[Prev].Access;
		}
		NewNode.Subresource = Subres;
		NewNode.PrevIndices.push_back(Prev);
		NewNode.BatchIndex = BatchIndex;
		return (u32)Nodes.size() - 1;
	};

	auto AddConnection = [&](u32 Prev, u32 Post) {
		Nodes[Post].PrevIndices.push_back(Prev);
		if (IsReadAccess(Nodes[Post].Access) && IsReadAccess(Nodes[Prev].Access)) {
			Nodes[Post].Access |= Nodes[Prev].Access;
		}
	};

	eastl::queue<u32> HelperQueue;
	auto PropagateRead = [&](u32 Index, EAccessType Access) {
		HelperQueue.empty();
		check(IsReadAccess(Access));
		if (IsReadAccess(Nodes[Index].Access) && !Nodes[Index].Immutable && (Nodes[Index].Access | Access) != Nodes[Index].Access) {
			HelperQueue.push(Index);
		}
		while (!HelperQueue.empty()) {
			u32 Node = HelperQueue.front();
			HelperQueue.pop();
			Nodes[Node].Access |= Access;
			for (u32 i = 0; i < Nodes[Node].PrevIndices.size(); i++) {
				u32 Prev = Nodes[Node].PrevIndices[i];
				if (IsReadAccess(Nodes[Prev].Access) && !Nodes[Prev].Immutable && (Nodes[Prev].Access | Access) != Nodes[Prev].Access) {
					HelperQueue.push(Prev);
				}
			}
		}
	};

	auto NeedNewNode = [&](u32 Prev, EAccessType Access) -> bool {
		bool Differs = Access != Nodes[Prev].Access;
		return (IsExclusiveAccess(Access) || Nodes[Prev].Immutable) && Differs;
	};

	for (auto & Request : Requests) {
		if (Request.Subresource == ALL_SUBRESOURCES) {
			bool ResourceInSameState = LastSubresourceNode.size() == 0 && LastComplementaryNode == -1;
			if (ResourceInSameState) {
				u32 Prev = LastAllSubresNode;

				if (NeedNewNode(Prev, Request.Access)) {
					LastAllSubresNode = SpawnNode(Prev, Request.Access, Request.Subresource, Request.BatchIndex);
				}
				else if (IsReadAccess(Request.Access)) {
					PropagateRead(Prev, Request.Access);
				}
			}
			// !ResourceInSameState
			else {
				// LastComplementaryNode
				LastAllSubresNode = SpawnNode(LastComplementaryNode, Request.Access, Request.Subresource, Request.BatchIndex);
				// foreach LastSubresourceNode
				for (auto PrevPair : LastSubresourceNode) {
					u32 Prev = PrevPair.second;
					u32 Subres = PrevPair.first;

					AddConnection(Prev, LastAllSubresNode);
				}

				LastSubresourceNode.clear();
				LastComplementaryNode = -1;
				PropagateRead(LastAllSubresNode, Request.Access);
			}
		}
		// Request.Subresource != ALL_SUBRESOURCES
		else {
			auto SubresFindIter = LastSubresourceNode.find(Request.Subresource);
			if (SubresFindIter != LastSubresourceNode.end()) {
				if (NeedNewNode(SubresFindIter->second, Request.Access)) {
					LastSubresourceNode[Request.Subresource] = SpawnNode(SubresFindIter->second, Request.Access, Request.Subresource, Request.BatchIndex);
				}
				else if (IsReadAccess(Request.Access)) {
					PropagateRead(SubresFindIter->second, Request.Access);
				}
			}
			else if (LastSubresourceNode.size()) {
				if (NeedNewNode(LastComplementaryNode, Request.Access)) {
					LastSubresourceNode[Request.Subresource] = SpawnNode(LastComplementaryNode, Request.Access, Request.Subresource, Request.BatchIndex);
				}
				else if (IsReadAccess(Request.Access)) {
					PropagateRead(LastComplementaryNode, Request.Access);
				}
			}
			else {
				if (NeedNewNode(LastAllSubresNode, Request.Access)) {
					LastComplementaryNode = SpawnNode(LastAllSubresNode, Nodes[LastAllSubresNode].Access, ALL_SUBRESOURCES, Request.BatchIndex);
					Nodes[LastComplementaryNode].Complementary = 1;
					LastSubresourceNode[Request.Subresource] = SpawnNode(LastAllSubresNode, Request.Access, Request.Subresource, Request.BatchIndex);
				}
				else if (IsReadAccess(Request.Access)) {
					PropagateRead(LastAllSubresNode, Request.Access);
				}
			}
		}
	}

	for (u32 Index = 0; Index < Nodes.size(); Index++) {
		for (u32 Prev : Nodes[Index].PrevIndices) {
			if (Nodes[Prev].Access != Nodes[Index].Access) {
				FResourceBarrier Barrier = {};
				Barrier.Resource = Resource;
				Barrier.Subresource = Nodes[Index].Subresource;
				Barrier.From = Nodes[Prev].Access;
				Barrier.To = Nodes[Index].Access;

				OutBarriers[Nodes[Prev].BatchIndex].push_back(Barrier);
			}
		}
	}
}

void FResourceStateRegistry::SetCurrentState(FGPUResource* Resource, u32 Subresource, EAccessType Access) {
	check(Resource->FatData->AutomaticBarriers);

	FResourceEntry & Entry = Resources[Resource];
	if (Subresource == ALL_SUBRESOURCES) {
		Entry.AllSubresources = Access;
		Entry.Complementary = EAccessType::UNSPECIFIED;
		Entry.Subresources.clear();
	}
	else if (Entry.AllSubresources != EAccessType::UNSPECIFIED) {
		Entry.Complementary = Entry.AllSubresources;
		Entry.Subresources[Subresource] = Access;
		Entry.AllSubresources = EAccessType::UNSPECIFIED;
	}
	else {
		Entry.Complementary = Entry.AllSubresources;
		Entry.Subresources.clear();
	}
}

void	FCommandsStream::SetAccess(FGPUResource * Resource, u32 Subresource, EAccessType Access) {
	check(Resource->FatData->AutomaticBarriers);

	FResourceAccess ResourceAccess;
	ResourceAccess.Subresource = Subresource;
	ResourceAccess.Access = Access;
	ResourceAccess.BatchIndex = -1;

	bool Ignore = ResourceAccessList[Resource].Accesses.size()
		&& ResourceAccessList[Resource].Accesses.back().Subresource == Subresource
		&& ResourceAccessList[Resource].Accesses.back().Access == Access;

	if (!Ignore) {
		ResourceAccessList[Resource].Accesses.push_back(ResourceAccess);
		ProcessList.insert(Resource);
	}
}

void	FCommandsStream::ReserveStreamSize(u64 Size) {
	u8 * NewData = new u8[Size];
	memcpy(NewData, Data.get(), MaxSize);
	Data.reset(NewData);
	MaxSize = Size;
}

void*   FCommandsStream::Reserve(u64 Size) {
	while (Offset + Size > MaxSize) {
		ReserveStreamSize(MaxSize * 2);
	}
	u64 CurrentOffset = Offset;
	Offset += Size;
	return Data.get() + CurrentOffset;
}

void FCommandsStream::Reset() {
	Offset = 0;
}

void FCommandsStream::Close() {
	BatchBarriers();
	ProcessBarriersPreExecution(*GetResourceStateRegistry());
}

void FCommandsStream::ProcessBarriersPreExecution(FResourceStateRegistry & Registry) {
	eastl::vector<FResourceAccessNode>	InitialGraph;
	Barriers.clear();

	for (auto & ResourceAccess : ResourceAccessList) {
		check(Registry.Resources.find(ResourceAccess.first) != Registry.Resources.end());

		InitialGraph.clear();
		check(ResourceAccess.second.BatchedNum == ResourceAccess.second.Accesses.size());

		if (Registry.Resources[ResourceAccess.first].AllSubresources != EAccessType::UNSPECIFIED) {
			FResourceAccessNode Node = {};
			Node.Access = Registry.Resources[ResourceAccess.first].AllSubresources;
			Node.Subresource = ALL_SUBRESOURCES;
			Node.Immutable = 1;
			InitialGraph.push_back(Node);
		}
		else {
			check(Registry.Resources[ResourceAccess.first].Complementary != EAccessType::UNSPECIFIED);
			FResourceAccessNode Node = {};
			Node.Access = Registry.Resources[ResourceAccess.first].Complementary;
			Node.Subresource = ALL_SUBRESOURCES;
			Node.Immutable = 1;
			Node.Complementary = 1;
			InitialGraph.push_back(Node);

			for (auto SubresourceAccess : Registry.Resources[ResourceAccess.first].Subresources) {
				Node.Access = SubresourceAccess.second;
				Node.Subresource = SubresourceAccess.first;
				Node.Immutable = 1;
				Node.Complementary = 0;
				InitialGraph.push_back(Node);
			}
		}

		ProcessResourceBarriers(ResourceAccess.first, InitialGraph, ResourceAccess.second.Accesses, Barriers);
	}
}

void FCommandsStream::BatchBarriers() {
	if (ProcessList.size()) {
		for (FGPUResource* Resource : ProcessList) {
			auto & List = ResourceAccessList[Resource];
			for (u32 Index = List.BatchedNum; Index < List.Accesses.size(); Index++) {
				List.Accesses[Index].BatchIndex = BatchCounter;
			}
			List.BatchedNum = (u32)List.Accesses.size();
		}

		ProcessList.clear();

		auto Data = ReservePacket<FRenderCmdBarriersBatch, FRenderCmdBarriersBatchFunc>();
		Data->This = this;
		Data->BatchIndex = BatchCounter;

		BatchCounter++;
	}
}

void FCommandsStream::ExecuteBatchedBarriers(FGPUContext * Context, u32 BatchIndex) {
	if (Barriers[BatchIndex].size()) {
		Context->Barriers(Barriers[BatchIndex].data(), (u32)Barriers[BatchIndex].size());
		Context->FlushBarriers();
	}
}

void Playback(FGPUContext & Context, FCommandsStream * Stream) {
	u64 Offset = 0;
	while (Offset < Stream->Offset) {
		FRenderCmdHeader * Header = (FRenderCmdHeader*)pointer_add(Stream->Data.get(), Offset);
		Offset += Header->Func(&Context, Header + 1);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE CreateCBVFromData(FConstantBuffer *, void const * Data, u64 Size) {
	auto allocation = GetConstantsAllocator()->Allocate(Size);
	memcpy(allocation.CPUPtr, Data, Size);
	return allocation.CPUHandle;;
}