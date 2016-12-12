#pragma once
#include "Scene.h"
#include "Pipeline.h"

class FDummyStateConsumer {
public:
	void SetPipelineState(FPipelineState * PipelineState) {}
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY Topology) {}
	void SetRenderTarget(FRenderTargetView, u32 Index) {}
	void SetDepthStencil(FDepthStencilView dsv) {}
	void SetViewport(D3D12_VIEWPORT const & Viewport) {}
	void SetScissorRect(D3D12_RECT const & Rect) {}
	void SetVB(FBufferLocation const & BufferView, u32 Stream = 0) {}
	void SetIB(FBufferLocation const & BufferView) {}
};


class FForwardPass : public FRenderPass {
public:
	FPipelineCache PipelineCache;

	void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) override;
	void PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FRenderPass_MaterialInstanceRefParam Cachable) override;
};

class FDepthPrePass : public FRenderPass {
public:
	FPipelineCache PipelineCache;

	void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) override;
	void PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FRenderPass_MaterialInstanceRefParam Cachable) override;
};