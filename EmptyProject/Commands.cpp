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
	ContextLifetime								Lifetime;
	u32											ApproximateWorkload = 0;

	CommandAllocator(CommandListPool* owner, ContextLifetime lifetime);
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
	using AllocatorsPool = eastl::array<eastl::queue<eastl::unique_ptr<CommandAllocator>>, (u32)ContextLifetime::CONTEXT_LIFETIMES_COUNT>;

	D3D12_COMMAND_LIST_TYPE								Type;
	eastl::vector<eastl::unique_ptr<GPUCommandList>>	CommandLists;
	AllocatorsPool										ReadyAllocators;
	AllocatorsPool										PendingAllocators;
	u32													ListsNum = 0;
	u32													AllocatorsNum = 0;

	CommandAllocator* ObtainAllocator(ContextLifetime lifetime) {
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
		if (allocator->Lifetime == ContextLifetime::FRAME) {
			ReadyAllocators[(u32)ContextLifetime::FRAME].emplace_back(allocator);
		}
		else {
			PendingAllocators[(u32)ContextLifetime::ASYNC].emplace_back(allocator);
		}
	}

	GPUCommandList*	ObtainList(ContextLifetime lifetime) {
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

CommandAllocator::CommandAllocator(CommandListPool* owner, ContextLifetime lifetime) : Owner(owner), Lifetime(lifetime), State(ReadyState) {
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

void GPUContext::Close() {	
	FlushBarriers();
	check(CommandList->State == GPUCommandList::RecordingState);
	CommandList->Close();
}

void GPUContext::Execute() {
	if (CommandList->State == GPUCommandList::RecordingState) {
		Close();
	}
	check(CommandList->State == GPUCommandList::ClosedState);
	Queue->Execute(CommandList);
	CommandList = nullptr;
}

void GPUContext::ExecuteImmediately() {
	Execute();
	Queue->Flush();
}

SyncPoint GPUContext::GetCompletionSyncPoint() {
	return CommandList->SyncPoint;
}

void GPUContext::Barrier(FGPUResource* resource, u32 subresource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource->D12Resource.get();
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = subresource;
	Barriers.push_back(barrier);
}

void GPUContext::FlushBarriers() {
	if (Barriers.size()) {
		GetCL()->ResourceBarrier((u32)Barriers.size(), Barriers.data());
		Barriers.clear();
	}
}

void GPUGraphicsContext::CopyDataToSubresource(FGPUResource* Dst, u32 Subresource, void const * Src, u64 RowPitch, u64 SlicePitch) {
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

	GetCL()->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);
	uploadBuffer.Release(GetCompletionSyncPoint());
}

void GPUGraphicsContext::CopyToBuffer(FGPUResource * Dst, void const* Src, u64 Size) {
	FlushBarriers();

	D3D12_SUBRESOURCE_DATA SubresourceData;
	SubresourceData.pData = Src;
	SubresourceData.RowPitch = Size;
	SubresourceData.SlicePitch = Size;

	auto uploadBuffer = GetUploadAllocator()->CreateBuffer(Size, 0);
	memcpy(uploadBuffer->GetMappedPtr(), Src, Size);

	GetCL()->CopyBufferRegion(Dst->D12Resource.get(), 0, uploadBuffer->D12Resource.get(), 0, Size);

	uploadBuffer.Release(GetCompletionSyncPoint());
}

void GPUComputeContext::Open(ContextLifetime lifetime) {
	Queue = GetComputeQueue();
	CommandList = ComputePool.ObtainList(lifetime);
}

ID3D12GraphicsCommandList* GPUContext::GetCL() const {
	return CommandList->D12CommandList.get();
}

void GPUGraphicsContext::Open(ContextLifetime lifetime) {
	CommandList = DirectPool.ObtainList(lifetime);
	Queue = GetDirectQueue();
	Device = GetPrimaryDevice()->D12Device.get();

	Reset();

	ID3D12DescriptorHeap * Heaps[] = { GetOnlineDescriptorsAllocator()->D12DescriptorHeap.get() };
	GetCL()->SetDescriptorHeaps(_countof(Heaps), Heaps);

	SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SetScissorRect(CD3DX12_RECT(0, 0, 32768, 32768));
}

void GPUGraphicsContext::ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float4 color) {
	CommandList->D12CommandList->ClearRenderTargetView(rtv, &color.x, 0, nullptr);
}

void GPUGraphicsContext::ClearDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, u8 stencil) {
	CommandList->D12CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}

void GPUGraphicsContext::CopyResource(FGPUResource* dst, FGPUResource* src) {
	FlushBarriers();
	GetCL()->CopyResource(dst->D12Resource.get(), src->D12Resource.get());
}

void GPUGraphicsContext::SetPSO(FPipelineState const* pso) {
	if (PipelineState != pso) {
		PipelineState = pso;
		GetCL()->SetPipelineState(GetRawPSO(pso));
	}
}

void GPUGraphicsContext::SetRoot(FGraphicsRootLayout const* rootLayout) {
	if (RootLayout != rootLayout) {
		RootLayout = rootLayout;

		if (RootSignature != RootLayout->RootSignature) {
			RootSignature = RootLayout->RootSignature;
			GetCL()->SetGraphicsRootSignature(GetRawRootSignature(rootLayout));

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

void GPUGraphicsContext::SetTopology(D3D_PRIMITIVE_TOPOLOGY topology) {
	if (Topology != topology) {
		Topology = topology;
		GetCL()->IASetPrimitiveTopology(topology);
	}
}

void GPUGraphicsContext::SetDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
	if (DSV != dsv) {
		DirtyRTVs = true;
		UsesDepth = dsv.ptr > 0;
		DSV = dsv;
	}
}

void GPUGraphicsContext::SetRenderTarget(u32 index, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
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

void GPUGraphicsContext::SetViewport(D3D12_VIEWPORT viewport) {
	if (Viewport != viewport) {
		Viewport = viewport;
		GetCL()->RSSetViewports(1, &viewport);
	}
}

bool operator != (D3D12_RECT const& A, D3D12_RECT const& B) {
	return A.bottom != B.bottom || A.left != B.left || A.right != B.right || A.top != B.top;
}

void GPUGraphicsContext::SetScissorRect(D3D12_RECT rect) {
	if (ScissorRect != rect) {
		GetCL()->RSSetScissorRects(1, &rect);
	}
}

void GPUGraphicsContext::Draw(u32 vertexCount, u32 startVertex, u32 instances, u32 startInstance) {
	PreDraw();
	GetCL()->DrawInstanced(vertexCount, instances, startVertex, startInstance);
}

void GPUGraphicsContext::DrawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex, u32 instances, u32 startInstance) {
	PreDraw();
	GetCL()->DrawIndexedInstanced(indexCount, instances, startIndex, baseVertex, startInstance);
}

bool operator != (D3D12_VERTEX_BUFFER_VIEW const& A, D3D12_VERTEX_BUFFER_VIEW const & B) {
	return A.BufferLocation != B.BufferLocation || A.SizeInBytes != B.SizeInBytes || A.StrideInBytes != B.StrideInBytes;
}

bool operator != (D3D12_INDEX_BUFFER_VIEW const& A, D3D12_INDEX_BUFFER_VIEW const & B) {
	return A.BufferLocation != B.BufferLocation || A.SizeInBytes != B.SizeInBytes || A.Format != B.Format;
}

void GPUGraphicsContext::SetVB(FBufferLocation BufferView, u32 Stream) {
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

void GPUGraphicsContext::SetIB(FBufferLocation BufferView) {
	D3D12_INDEX_BUFFER_VIEW NewIBV;
	NewIBV.BufferLocation = BufferView.Address;
	NewIBV.Format = BufferView.Stride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	NewIBV.SizeInBytes = BufferView.Size;
	if (IBV != NewIBV) {
		IBV = NewIBV;
		GetCL()->IASetIndexBuffer(&IBV);
	}
}

void GPUGraphicsContext::Reset() {
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

void GPUGraphicsContext::SetConstantBuffer(FConstantBufferParam const * ConstantBuffer) {
	auto bind = RootLayout->ConstantBuffers.find(ConstantBuffer->BindId)->second.Bind;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= Param.SrcRanges[bind.DescOffset] != ConstantBuffer->CPUHandle;
	Param.SrcRanges[bind.DescOffset] = ConstantBuffer->CPUHandle;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void GPUGraphicsContext::SetTexture(FShaderParam const * Texture, D3D12_CPU_DESCRIPTOR_HANDLE View) {
	auto bind = RootLayout->Textures.find(Texture->BindId)->second;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= View != Param.SrcRanges[bind.DescOffset];
	Param.SrcRanges[bind.DescOffset] = View;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void GPUGraphicsContext::SetRWTexture(FShaderParam const * RWTexture, D3D12_CPU_DESCRIPTOR_HANDLE View) {
	auto bind = RootLayout->RWTextures.find(RWTexture->BindId)->second;
	auto& Param = RootParams[bind.RootParam];
	Param.Dirty |= View != Param.SrcRanges[bind.DescOffset];
	Param.SrcRanges[bind.DescOffset] = View;
	Param.SrcRangesNums[bind.DescOffset] = 1;
}

void GPUGraphicsContext::PreDraw() {
	FlushBarriers();

	if (DirtyRTVs) {
		GetCL()->OMSetRenderTargets(NumRenderTargets, RTVs, false, UsesDepth ? &DSV : nullptr);
		DirtyRTVs = false;
	}

	if (DirtyVBVs) {
		GetCL()->IASetVertexBuffers(0, NumVertexBuffers, VBVs);
		DirtyVBVs = false;
	}

	for (u32 index = 0; index < RootParamsNum; ++index) {
		auto &Param = RootParams[index];
		if (Param.Dirty) {
			Param.Dirty = false;
			Param.Descriptors = OnlineDescriptors->FastTemporaryAllocate(Param.TableLen);
			auto destHandle = Param.Descriptors.GetCPUHandle(0);

			Device->CopyDescriptors(1, &destHandle, &Param.Descriptors.DescriptorsNum, Param.Descriptors.DescriptorsNum, Param.SrcRanges.data(), Param.SrcRangesNums.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			GetCL()->SetGraphicsRootDescriptorTable(index, Param.Descriptors.GetGPUHandle(0));
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
