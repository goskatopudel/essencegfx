#pragma once
#include "Essence.h"
#include "Device.h"
#include "MathVector.h"
#include "CommandStream.h"

#include <EASTL/vector.h>
#include <EASTL/array.h>
#include "d3dx12.h"

class FShader;
class FPipelineState;
class FRootLayout;
class GPUCommandList;
class GPUCommandQueue;
class FGPUContext;
class GPUCopyContext;
class GPUComputeContext;

enum class EContextPriority {
	NORMAL,
	HIGH
};

enum class EContextLifetime {
	FRAME,
	ASYNC,
	CONTEXT_LIFETIMES_COUNT,
	DEFAULT
};

class FGPUSyncPoint {
public:
	u32	Generation;
	u32 Index;

	FGPUSyncPoint() = default;
	FGPUSyncPoint(u32 index, u32 generation) : Index(index), Generation(generation) {}
	bool IsCompleted();
	void WaitForCompletion();
	void SetTrigger(GPUCommandQueue*);
	void SetTrigger(GPUCommandQueue*, u64);
	bool IsSet();
};

bool operator == (FGPUSyncPoint A, FGPUSyncPoint B);
bool operator != (FGPUSyncPoint A, FGPUSyncPoint B);

FGPUSyncPoint GetDummyGPUSyncPoint();
FGPUSyncPoint GetCurrentFrameGPUSyncPoint();
FGPUSyncPoint GetLastFrameGPUSyncPoint();

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

	void Wait(FGPUSyncPoint fence);
	void Execute(GPUCommandList* list);
	void Flush();

	void WaitForCompletion();
	void WaitForCompletion(u64);
	u64	GetCurrentSyncValue() const;
	u64 GetCompletedValue();
	u64 AdvanceSyncValue();
	FGPUSyncPoint GenerateGPUSyncPoint();
	FGPUSyncPoint GetCompletionGPUSyncPoint();
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
	UNSPECIFIED = 0,
	COMMON = 0x2000,
	SPLIT_NO_ACCESS = 0x1000, 
	READ_PIXEL = 0x1,
	READ_NON_PIXEL = 0x2,
	READ_DEPTH = 0x4,
	COPY_SRC = 0x8,
	WRITE_RT = 0x10,
	WRITE_DEPTH = 0x20,
	WRITE_UAV = 0x40,
	COPY_DEST = 0x80,
	READ_IB = 0x100,
	READ_VB_CB = 0x200
};
DEFINE_ENUM_FLAG_OPERATORS(EAccessType);

class FGPUResource;
class FRootSignature;
class FDescriptorAllocator;

const u32 MAX_ROOT_PARAMS = 8;

enum EContextType {
	DIRECT,
	COMPUTE,
	COPY
};

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

	void Free(FGPUSyncPoint sync);
};

struct FBoundRootParam {
	FDescriptorsAllocation						Descriptors;
	eastl::vector<D3D12_CPU_DESCRIPTOR_HANDLE>	SrcRanges;
	eastl::vector<u32>							SrcRangesNums;
	u32											TableLen;
	bool										Dirty;
};

struct FCBVParam;
struct FSRVParam;
struct FUAVParam;

struct FResourceBarrier {
	FGPUResource *	Resource;
	u32				Subresource;
	EAccessType		From;
	EAccessType		To;
};

struct FStateCache {
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

	bool SetTopology(D3D_PRIMITIVE_TOPOLOGY Topology);
	bool SetRenderTarget(FRenderTargetView View, u32 Index);
	bool SetDepthStencil(FDepthStencilView View);
	bool SetViewport(D3D12_VIEWPORT const & Viewport);
	bool SetScissorRect(D3D12_RECT const & Rect);
	bool SetVB(FBufferLocation const & BufferView, u32 Stream = 0);
	bool SetIB(FBufferLocation const & BufferView);

	void Reset();
};

class FGPUContext {
public:
	GPUCommandList*		CommandList;
	GPUCommandQueue*	Queue;

	void Close();
	void Execute();
	void ExecuteImmediately();
	FGPUSyncPoint GetCompletionGPUSyncPoint();

	ID3D12GraphicsCommandList* RawCommandList() const;

	eastl::vector<FResourceBarrier>			BarriersList;
	u32										FlushCounter;
	void Barriers(FResourceBarrier const * Barriers, u32 Num);
	void Barrier(FGPUResource* resource, u32 subresource, EAccessType before, EAccessType after);
	void FlushBarriers();	

	EPipelineType PipelineType;
	u32 DirtyRoot : 1;

	FStateCache StateCache;

	ID3D12Device* Device;
	FPipelineState const * PipelineState;
	FRootLayout const * RootLayout;
	FRootSignature const * RootSignature;
	FDescriptorAllocator* OnlineDescriptors;
	eastl::array<FBoundRootParam, MAX_ROOT_PARAMS> RootParams;
	u32 RootParamsNum;

	// Execution 

	void Open(EContextType Type, EContextLifetime lifetime = EContextLifetime::DEFAULT);

	// Utils

	void CopyDataToSubresource(FGPUResource * Dst, u32 Subresource, void const * Src, u64 RowPitch, u64 SlicePitch);
	void CopyToBuffer(FGPUResource * Dst, void const* Src, u64 Size);

	// Raw calls

	void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float4 color);
	void ClearDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, u8 stencil = 0);
	void ClearUAV(D3D12_CPU_DESCRIPTOR_HANDLE uav, FGPUResource * resource, Vec4u value = 0u);
	void CopyResource(FGPUResource* dst, FGPUResource* src);
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void SetRenderTarget(FRenderTargetView view, u32 index);
	void SetDepthStencil(FDepthStencilView view);
	void SetViewport(D3D12_VIEWPORT const & viewport);
	void SetScissorRect(D3D12_RECT const & rect);
	void Draw(u32 vertexCount, u32 startVertex = 0, u32 instances = 1, u32 startInstance = 0);
	void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0, u32 instances = 1, u32 startInstance = 0);
	void SetVB(FBufferLocation const & BufferView, u32 Stream = 0);
	void SetIB(FBufferLocation const & BufferView);
	void Dispatch(u32 X, u32 Y = 1, u32 Z = 1);
	void CopyTextureRegion(FGPUResource * dst, u32 dstSubresource, FGPUResource * src, u32 srcSubresource);
	void SetCounter(FGPUResource * dst, u32 value);

	// Binding 

	void SetPipelineState(FPipelineState const* PipelineState);
	void SetPSO(FPipelineState const* pipelineState);
	void SetRoot(FRootLayout const* rootLayout);
	void SetConstantBuffer(FCBVParam const * ConstantBuffer, D3D12_CPU_DESCRIPTOR_HANDLE CBV);
	void SetTexture(FSRVParam const * Texture, D3D12_CPU_DESCRIPTOR_HANDLE View);
	void SetRWTexture(FUAVParam const * RWTexture, D3D12_CPU_DESCRIPTOR_HANDLE View);

	// Helpers

	//void SetRenderTargetsBundle(struct FRenderTargetsBundle const * Bundle);
	void PreDraw();
	void Reset();
};

#include <EASTL\hash_map.h>

class FResourceStateRegistry {
public:
	class FResourceEntry {
	public:
		EAccessType		AllSubresources;
		EAccessType		Complementary;
		eastl::hash_map<u32, EAccessType> Subresources;
	};

	eastl::hash_map<FGPUResource*, FResourceEntry>	Resources;

	void SetCurrentState(FGPUResource* Resource, u32 Subresource, EAccessType Access);
	void Deregister(FGPUResource* Resource);
};

FResourceStateRegistry * GetResourceStateRegistry();

struct FResourceAccess {
	u32			Subresource;
	EAccessType	Access;
	u32			BatchIndex;
};

struct FResourceAccessNode {
	u32					Subresource;
	EAccessType			Access;
	eastl::vector<u32>	PrevIndices;
	u32					BatchIndex;
	u8					Immutable : 1;
	u8					Complementary : 1;
};

#include <EASTL/hash_set.h>

enum class ECommandsStreamMode {
	Direct, 
	Indirect
};

class FCommandsStream {
	// todo: bypass recoding when no access changes, go directly to d3d12 command list (for geometry passes)
public:
	ECommandsStreamMode Mode = ECommandsStreamMode::Indirect;

	eastl::unique_ptr<u8[]> Data;
	u64 Offset = 0;
	u64 MaxSize = 0;
	bool IsClosed = 0;

	struct FResourceAccessList {
		eastl::vector<FResourceAccess> Accesses;
		u32 BatchedNum;
	};
	eastl::hash_map<FGPUResource*, FResourceAccessList>	ResourceAccessList;
	eastl::hash_set<FGPUResource*> ProcessList;
	u32 BatchCounter = 0;
	eastl::hash_map<u32, eastl::vector<FResourceBarrier>> Barriers;

	void Close();
	void ProcessBarriersPreExecution(FResourceStateRegistry & Registry);
	void BatchBarriers();
	void ExecuteBatchedBarriers(FGPUContext * Context, u32 BatchIndex);

	//void SetRenderTargetsBundle(struct FRenderTargetsBundle const * Bundle);
	void SetConstantBufferData(FCBVParam * ConstantBuffer, const void * Data, u64 Size);
	
	void PreCommandAdd() {
		check(!IsClosed);
	}

	inline void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float4 color) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdClearRTV, FRenderCmdClearRTVFunc>();
		Data->RTV = rtv;
		Data->Color = color;
	}

	inline void ClearDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, u8 stencil = 0) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdClearDSV, FRenderCmdClearDSVFunc>();
		Data->DSV = dsv;
		Data->Depth = depth;
		Data->Stencil = stencil;
	}

	inline void ClearUAV(D3D12_CPU_DESCRIPTOR_HANDLE uav, FGPUResource * resource, Vec4u value = 0u) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdClearUAV, FRenderCmdClearUAVFunc>();
		Data->UAV = uav;
		Data->Resource = resource;
		Data->Value = value;
	}

	inline void SetCounter(FGPUResource * dst, u32 value = 0) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdSetCounter, FRenderCmdSetCounterFunc>();
		Data->Resource = dst;
		Data->Value = value;
	}

	void SetAccess(FGPUResource * Resource, EAccessType Access, u32 Subresource = ALL_SUBRESOURCES);

	inline void Draw(u32 vertexCount, u32 startVertex = 0, u32 instances = 1, u32 startInstance = 0) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdDraw, FRenderCmdDrawFunc>();
		Data->VertexCount = vertexCount;
		Data->StartVertex = startVertex;
		Data->Instances = instances;
		Data->StartInstance = startInstance;
	}

	inline void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0, u32 instances = 1, u32 startInstance = 0) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdDrawIndexed, FRenderCmdDrawIndexedFunc>();
		Data->IndexCount = indexCount;
		Data->StartIndex = startIndex;
		Data->BaseVertex = baseVertex;
		Data->Instances = instances;
		Data->StartInstance = startInstance;
	}

	inline void Dispatch(u32 X, u32 Y = 1, u32 Z = 1) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdDispatch, FRenderCmdDispatchFunc>();
		Data->X = X;
		Data->Y = Y;
		Data->Z = Z;
	}

	inline void Dispatch(Vec3u Kernel) {
		PreCommandAdd();
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdDispatch, FRenderCmdDispatchFunc>();
		Data->X = Kernel.x;
		Data->Y = Kernel.y;
		Data->Z = Kernel.z;
	}

	inline void SetScissorRect(D3D12_RECT const& Rect) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetScissorRect, FRenderCmdSetScissorRectFunc>();
		Data->Rect = Rect;
	}

	inline void SetConstantBuffer(FCBVParam * ConstantBuffer, D3D12_CPU_DESCRIPTOR_HANDLE CBV) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetConstantBuffer, FRenderCmdSetConstantBufferFunc>();
		Data->Param = ConstantBuffer;
		Data->CBV = CBV;
	}

	inline void SetTexture(FSRVParam * Texture, D3D12_CPU_DESCRIPTOR_HANDLE SRV) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetTexture, FRenderCmdSetTextureFunc>();
		Data->Param = Texture;
		Data->SRV = SRV;
	}
	inline void SetRWTexture(FUAVParam * Texture, D3D12_CPU_DESCRIPTOR_HANDLE UAV) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetRWTexture, FRenderCmdSetRWTextureFunc>();
		Data->Param = Texture;
		Data->UAV = UAV;
	}
	inline void SetPipelineState(FPipelineState * PipelineState) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetPipelineState, FRenderCmdSetPipelineStateFunc>();
		Data->State = PipelineState;
	}
	inline void SetViewport(D3D12_VIEWPORT const& Viewport) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetViewport, FRenderCmdSetViewportFunc>();
		Data->Viewport = Viewport;
	}
	inline void SetVB(FBufferLocation& Location, u8 Index) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetVB, FRenderCmdSetVBFunc>();
		Data->Location = Location;
		Data->Stream = Index;
	}
	inline void SetIB(FBufferLocation& Location) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetIB, FRenderCmdSetIBFunc>();
		Data->Location = Location;
	}
	inline void SetTopology(D3D_PRIMITIVE_TOPOLOGY Topology) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetTopology, FRenderCmdSetTopologyFunc>();
		Data->Topology = Topology;
	}
	inline void SetRenderTarget(FRenderTargetView View, u8 Index = 0) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetRenderTarget, FRenderCmdSetRenderTargetFunc>();
		Data->View = View;
		Data->Index = Index;
	}
	inline void SetDepthStencil(FDepthStencilView View) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdSetDepthStencil, FRenderCmdSetDepthStencilFunc>();
		Data->View = View;
	}
	inline void CopyResource(FGPUResource * Dst, FGPUResource * Src) {
		PreCommandAdd();
		auto Data = ReservePacket<FRenderCmdCopyResource, FRenderCmdCopyResourceFunc>();
		Data->Dst = Dst;
		Data->Src = Src;
	}
	inline void CopyTextureRegion(FGPUResource * Dst, u16 DstSubresource, FGPUResource * Src, u16 SrcSubresource) {
		PreCommandAdd();
		SetAccess(Dst, EAccessType::COPY_DEST, DstSubresource);
		SetAccess(Src, EAccessType::COPY_SRC, SrcSubresource);
		BatchBarriers();
		auto Data = ReservePacket<FRenderCmdCopyTextureRegion, FRenderCmdCopyTextureRegionFunc>();
		Data->Dst = Dst;
		Data->DstSubresource = DstSubresource;
		Data->Src = Src;
		Data->SrcSubresource = SrcSubresource;
	}

	//

	void	ReserveStreamSize(u64 Size);
	void *	Reserve(u64 Size);
	void	Reset();

	FCommandsStream() {
		ReserveStreamSize(10 * 1024);
	}

	template<typename T> inline T*	Reserve() {
		return (T*)Reserve(sizeof(T));
	}

	inline FRenderCmdHeader*		ReserveHeader() {
		return Reserve<FRenderCmdHeader>();
	}

	template<typename T, RenderCmdFunc Func>
	inline T* ReservePacket() {
		FRenderCmdHeader * Header = ReserveHeader();
		Header->Func = Func;
		return Reserve<T>();
	}
};

D3D12_CPU_DESCRIPTOR_HANDLE CreateCBVFromData(FCBVParam *, void const * Data, u64 Size);

template<typename T>
inline D3D12_CPU_DESCRIPTOR_HANDLE CreateCBVFromData(FCBVParam * CB, T const& DataRef) {
	return CreateCBVFromData(CB, &DataRef, sizeof(T));
}

void Playback(FGPUContext & Context, FCommandsStream * Stream);

void ProcessResourceBarriers(FGPUResource * Resource, eastl::vector<FResourceAccessNode>& InitialNodes, eastl::vector<FResourceAccess> const& Requests, eastl::hash_map<u32, eastl::vector<FResourceBarrier>> &OutBarriers);

struct FBarrierScope {
	FGPUResource*			Resource;
	u32						Subresource;
	EAccessType				From;
	EAccessType				To;
	EAccessType				End;
	bool					UseEnd;
	FGPUContext&			Context;

	FBarrierScope(FGPUContext& context, FGPUResource* res, u32 subres, EAccessType from, EAccessType to) :
		Context(context),
		Resource(res),
		Subresource(subres),
		From(from),
		To(to),
		UseEnd(false)
	{
		Context.Barrier(Resource, Subresource, From, To);
	}

	FBarrierScope(FGPUContext& context, FGPUResource* res, u32 subres, EAccessType from, EAccessType to, EAccessType end) :
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

// todo: better place
template<typename T, typename V>
T IntDivRoundUp(T Divident, V Divisor) {
	return (Divident + Divisor - V(1)) / Divisor;
}
