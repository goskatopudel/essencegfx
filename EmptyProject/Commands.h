#pragma once
#include "Essence.h"
#include "Device.h"
#include "MathVector.h"

#include <EASTL/vector.h>
#include <EASTL/array.h>
#include "d3dx12.h"

class FShader;
class FPipelineState;
class FGraphicsRootLayout;
class GPUCommandList;
class GPUCommandQueue;
class GPUGraphicsContext;
class GPUCopyContext;
class GPUComputeContext;

enum class ContextPriority {
	NORMAL,
	HIGH
};

enum class ContextLifetime {
	FRAME,
	ASYNC,
	CONTEXT_LIFETIMES_COUNT
};

class SyncPoint {
public:
	u32	Generation;
	u32 Index;

	SyncPoint() = default;
	SyncPoint(u32 index, u32 generation) : Index(index), Generation(generation) {}
	bool IsCompleted();
	void WaitForCompletion();
	void SetTrigger(GPUCommandQueue*);
	void SetTrigger(GPUCommandQueue*, u64);
	bool IsSet();
};

bool operator == (SyncPoint A, SyncPoint B);
bool operator != (SyncPoint A, SyncPoint B);

SyncPoint GetDummySyncPoint();
SyncPoint GetCurrentFrameSyncPoint();
SyncPoint GetLastFrameSyncPoint();

void EndFrame();

class GPUCommandQueue {
public:
	enum TypeEnum {
		GRAPHICS,
		COMPUTE,
		COPY
	};

	unique_com_ptr<ID3D12CommandQueue>	D12CommandQueue;
	unique_com_ptr<ID3D12Fence>			D12Fence;
	TypeEnum							Type;
	D3D12Device*						Device;
	u64									LastSignaledValue;
	u64									CurrentSyncValue;
	u64									CachedLastCompletedValue;
	eastl::vector<GPUCommandList*>		QueuedCommandLists;

	GPUCommandQueue(D3D12Device* device, TypeEnum type);

	void Wait(SyncPoint fence);
	void Execute(GPUCommandList* list);
	void Flush();

	void WaitForCompletion();
	void WaitForCompletion(u64);
	u64	GetCurrentSyncValue() const;
	u64 GetCompletedValue();
	u64 AdvanceSyncValue();
	SyncPoint GenerateSyncPoint();
	SyncPoint GetCompletionSyncPoint();
};

struct FBufferLocation {
	GPU_VIRTUAL_ADDRESS		Address;
	u32						Size;
	u32						Stride;
};

GPUCommandQueue*		GetDirectQueue();

enum class CommandListStateEnum {
	Unassigned,
	Recording,
	Closed,
	Executed
};

enum class PipelineTypeEnum {
	Unassigned,
	Graphics,
	Compute
};

struct commands_stats_t {
	u32 graphic_pipeline_state_changes;
	u32 graphic_root_signature_changes;
	u32 graphic_root_params_set;
	u32 draw_calls;
	u32 compute_pipeline_state_changes;
	u32 compute_root_signature_changes;
	u32 compute_root_params_set;
	u32 dispatches;
	u64 constants_bytes_uploaded;
};

commands_stats_t& operator += (commands_stats_t& lhs, commands_stats_t const& rhs);

struct frame_stats_t {
	commands_stats_t	command_stats;
	u32					executions_num;
	u32					command_lists_num;
	u32					patchup_command_lists_num;
};

enum class EAccessType {
	INVALID = 0,
	SPLIT_NO_ACCESS = 0x1000,
	READ_PIXEL = 0x1,
	READ_NON_PIXEL = 0x2,
	READ_DEPTH = 0x4,
	WRITE_RT = 0x10,
	WRITE_DEPTH = 0x20,
	WRITE_UAV = 0x40,
	END_READ = 0x100,
	END_WRITE = 0x200
};
DEFINE_ENUM_FLAG_OPERATORS(EAccessType);

class GPUContext {
public:
	GPUCommandList*		CommandList;
	GPUCommandQueue*	Queue;

	void Close();
	void Execute();
	void ExecuteImmediately();
	SyncPoint GetCompletionSyncPoint();

	ID3D12GraphicsCommandList* GetCL() const;

	eastl::vector<D3D12_RESOURCE_BARRIER>	Barriers;
	void Barrier(FGPUResource* resource, u32 subresource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void FlushBarriers();	
};

class FGPUResource;
class FRootSignature;
class FDescriptorAllocator;

struct FShaderParam;
struct FConstantBufferParam;

const u32 MAX_ROOT_PARAMS = 8;

// 16B
struct FDescriptorsAllocation {
	u32						HeapOffset;
	u32						DescriptorsNum;
	FDescriptorAllocator*	Allocator;

	inline bool IsValid() const {
		return Allocator != nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(i32 offset) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(i32 offset) const;

	void Free(SyncPoint sync);
};

struct FBoundRootParam {
	FDescriptorsAllocation						Descriptors;
	eastl::vector<D3D12_CPU_DESCRIPTOR_HANDLE>	SrcRanges;
	eastl::vector<u32>							SrcRangesNums;
	u32											TableLen;
	bool										Dirty;
};

class GPUGraphicsContext : public GPUContext {
public:
	static const u32 MAX_RTVS = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	static const u32 MAX_VBVS = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;

	D3D12_VIEWPORT										Viewport;
	D3D12_RECT											ScissorRect;
	D3D_PRIMITIVE_TOPOLOGY								Topology;
	D3D12_CPU_DESCRIPTOR_HANDLE							RTVs[MAX_RTVS];
	D3D12_CPU_DESCRIPTOR_HANDLE							DSV;
	u32													NumRenderTargets : 4;
	u32													NumVertexBuffers : 5;
	u32													DirtyRTVs : 1;
	u32													UsesDepth : 1;
	u32													DirtyVBVs : 1;
	D3D12_VERTEX_BUFFER_VIEW							VBVs[MAX_VBVS];
	D3D12_INDEX_BUFFER_VIEW								IBV;

	ID3D12Device*										Device;
	FPipelineState const *								PipelineState;
	FGraphicsRootLayout const *							RootLayout;
	FRootSignature const *								RootSignature;
	FDescriptorAllocator*								OnlineDescriptors;
	eastl::array<FBoundRootParam, MAX_ROOT_PARAMS>		RootParams;
	u32													RootParamsNum;

	// Execution 

	void Open(ContextLifetime lifetime = ContextLifetime::FRAME);

	// Utils

	void CopyDataToSubresource(FGPUResource * Dst, u32 Subresource, void const * Src, u64 RowPitch, u64 SlicePitch);
	void CopyToBuffer(FGPUResource * Dst, void const* Src, u64 Size);

	// Raw calls

	void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float4 color);
	void ClearDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, u8 stencil = 0);
	void CopyResource(FGPUResource* dst, FGPUResource* src);
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void SetRenderTarget(u32 index, D3D12_CPU_DESCRIPTOR_HANDLE rtv);
	void SetDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv);
	void SetViewport(D3D12_VIEWPORT viewport);
	void SetScissorRect(D3D12_RECT rect);
	void Draw(u32 vertexCount, u32 startVertex = 0, u32 instances = 1, u32 startInstance = 0);
	void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0, u32 instances = 1, u32 startInstance = 0);
	void SetVB(FBufferLocation BufferView, u32 Stream = 0);
	void SetIB(FBufferLocation BufferView);

	// Binding 

	void SetPSO(FPipelineState const* pipelineState);
	void SetRoot(FGraphicsRootLayout const* rootLayout);
	void SetConstantBuffer(FConstantBufferParam const * ConstantBuffer);
	void SetTexture(FShaderParam const * Texture, D3D12_CPU_DESCRIPTOR_HANDLE View);
	void SetRWTexture(FShaderParam const * RWTexture, D3D12_CPU_DESCRIPTOR_HANDLE View);

	// Helpers

	void PreDraw();
	void Reset();
};

class GPUComputeContext : public GPUContext {
public:
	void Open(ContextLifetime lifetime = ContextLifetime::ASYNC);
};

class GPUCopyContext : public GPUContext {
public:
};


struct FBarrierScope {
	FGPUResource*			Resource;
	u32						Subresource;
	D3D12_RESOURCE_STATES	From;
	D3D12_RESOURCE_STATES	To;
	D3D12_RESOURCE_STATES	End;
	bool					UseEnd;
	GPUContext&				Context;

	FBarrierScope(GPUContext& context, FGPUResource* res, u32 subres, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) :
		Context(context),
		Resource(res),
		Subresource(subres),
		From(from),
		To(to),
		UseEnd(false)
	{
		Context.Barrier(Resource, Subresource, From, To);
	}

	FBarrierScope(GPUContext& context, FGPUResource* res, u32 subres, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES end) :
		Context(context),
		Resource(res),
		Subresource(subres),
		From(from),
		To(to),
		End(end),
		UseEnd(true)
	{
		Context.Barrier(Resource, Subresource, From, To);
	}

	FBarrierScope(FBarrierScope const&) = delete;

	~FBarrierScope() {
		Context.Barrier(Resource, Subresource, To, UseEnd ? End : From);
	}
};
