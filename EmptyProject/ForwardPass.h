#pragma once
#include "Scene.h"
#include "Pipeline.h"


class FForwardPass : public FRenderPass {
public:
	FPipelineCache PipelineCache;

	void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) override;
	void PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FSceneRenderPass_MaterialInstanceRefParam Cachable) override;
	void QueryRenderTargets(FSceneRenderContext & SceneRenderContext, FRenderTargetsBundle & Bundle) override;
	FRootSignature * GetDefaultRootSignature() override;
};

class FDepthPrePass : public FRenderPass {
public:
	FPipelineCache PipelineCache;

	void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) override;
	void PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FSceneRenderPass_MaterialInstanceRefParam Cachable) override;
	void QueryRenderTargets(FSceneRenderContext & SceneRenderContext, FRenderTargetsBundle & Bundle) override;
	FRootSignature * GetDefaultRootSignature() override;
};