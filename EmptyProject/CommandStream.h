#pragma once
#include "Essence.h"
#include "Device.h"
#include <d3d12.h>
#include "MathVector.h"

enum class EPipelineType {
	Graphics,
	Compute
};

struct FBufferLocation {
	GPU_VIRTUAL_ADDRESS		Address;
	u32						Size;
	u32						Stride;
};

struct FRenderTargetView {
	D3D12_CPU_DESCRIPTOR_HANDLE RTV;
	DXGI_FORMAT Format;

	FRenderTargetView() = default;
};

struct FDepthStencilView {
	D3D12_CPU_DESCRIPTOR_HANDLE DSV;
	DXGI_FORMAT Format;

	FDepthStencilView() = default;
};

class FGPUContext;
class FPipelineState;
struct FSRVParam;
struct FUAVParam;
struct FCBVParam;

using RenderCmdFunc = u64(*) (FGPUContext *, void *);

struct FRenderCmdHeader {
	RenderCmdFunc	Func;
};

struct	FRenderCmdBarriersBatch {
	class FCommandsStream *	This;
	u32						BatchIndex;
};

u64	FRenderCmdBarriersBatchFunc(FGPUContext * Context, void * DataVoidPtr);

struct	FRenderCmdClearRTV {
	D3D12_CPU_DESCRIPTOR_HANDLE RTV;
	float4 Color;
};

u64	FRenderCmdClearRTVFunc(FGPUContext * Context, void * DataVoidPtr);

struct	FRenderCmdClearDSV {
	D3D12_CPU_DESCRIPTOR_HANDLE	DSV;
	float Depth;
	u8 Stencil;
};

u64	FRenderCmdClearDSVFunc(FGPUContext * Context, void * DataVoidPtr);

struct	FRenderCmdClearUAV {
	D3D12_CPU_DESCRIPTOR_HANDLE	UAV;
	FGPUResource * Resource;
	Vec4u Value;
};

u64	FRenderCmdClearUAVFunc(FGPUContext * Context, void * DataVoidPtr);

struct	FRenderCmdSetCounter {
	FGPUResource * Resource;
	u32 Value;
};

u64	FRenderCmdSetCounterFunc(FGPUContext * Context, void * DataVoidPtr);

struct	FRenderCmdSetPipelineState {
	FPipelineState *	State;
};

u64	FRenderCmdSetPipelineStateFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetVB {
	FBufferLocation	Location;
	u8				Stream;
};

u64	FRenderCmdSetVBFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetIB {
	FBufferLocation Location;
};

u64 FRenderCmdSetIBFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetTopology {
	D3D_PRIMITIVE_TOPOLOGY Topology;
};

u64 FRenderCmdSetTopologyFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetViewport {
	D3D12_VIEWPORT	Viewport;
};

u64 FRenderCmdSetViewportFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetRenderTarget {
	FRenderTargetView			View;
	u8							Index;
};

u64 FRenderCmdSetRenderTargetFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetDepthStencil {
	FDepthStencilView			View;
};

u64 FRenderCmdSetDepthStencilFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetTexture {
	FSRVParam *				Param;
	D3D12_CPU_DESCRIPTOR_HANDLE	SRV;
};

u64 FRenderCmdSetTextureFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetConstantBuffer {
	FCBVParam *			Param;
	D3D12_CPU_DESCRIPTOR_HANDLE	CBV;
};

u64 FRenderCmdSetConstantBufferFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetRWTexture {
	FUAVParam *			Param;
	D3D12_CPU_DESCRIPTOR_HANDLE	UAV;
};

u64 FRenderCmdSetRWTextureFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdSetScissorRect {
	D3D12_RECT	Rect;
};

u64 FRenderCmdSetScissorRectFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdDraw {
	u32 VertexCount;
	u32 StartVertex;
	u32 Instances;
	u32 StartInstance;
};

u64 FRenderCmdDrawFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdDrawIndexed {
	u32 IndexCount;
	u32 StartIndex;
	i32 BaseVertex;
	u32 Instances;
	u32 StartInstance;
};

u64 FRenderCmdDrawIndexedFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdDispatch {
	u32 X;
	u32 Y;
	u32 Z;
};

u64 FRenderCmdDispatchFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdCopyResource {
	FGPUResource * Dst;
	FGPUResource * Src;
};

u64 FRenderCmdCopyResourceFunc(FGPUContext * Context, void * DataVoidPtr);

struct FRenderCmdCopyTextureRegion {
	FGPUResource * Dst;
	FGPUResource * Src;
	u16 DstSubresource;
	u16 SrcSubresource;
};

u64 FRenderCmdCopyTextureRegionFunc(FGPUContext * Context, void * DataVoidPtr);