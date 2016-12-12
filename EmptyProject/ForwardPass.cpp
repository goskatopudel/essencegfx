#include "ForwardPass.h"
#include "Scene.h"

template<typename TConsumer>
void ForwardSetupState(FSceneRenderContext & RenderSceneContext, TConsumer & Consumer) {
	FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	Consumer.SetRenderTarget(ColorBuffer->GetRTV());
	Consumer.SetDepthStencil(DepthBuffer->GetDSV());
}

void FForwardPass::Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {
	FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	CmdStream.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH);
	CmdStream.SetAccess(ColorBuffer, EAccessType::WRITE_RT);

	ForwardSetupState(RenderSceneContext, CmdStream);
}

void FForwardPass::PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FRenderPass_MaterialInstanceRefParam Cachable) {
	FStateProxy<FDummyStateConsumer> StateProxy;
	StateProxy.SetCache(PipelineCache);

	ForwardSetupState(RenderSceneContext, StateProxy);

	StateProxy.ApplyState();
	Cachable->PSO = StateProxy.CurrentPipelineState;
}

void FDepthPrePass::Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {
	FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	CmdStream.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH);
	CmdStream.SetAccess(ColorBuffer, EAccessType::WRITE_RT);

	ForwardSetupState(RenderSceneContext, CmdStream);
}

void FDepthPrePass::PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FRenderPass_MaterialInstanceRefParam Cachable) {
	FStateProxy<FDummyStateConsumer> StateProxy;
	StateProxy.SetCache(PipelineCache);

	ForwardSetupState(RenderSceneContext, StateProxy);

	StateProxy.ApplyState();
	Cachable->PSO = StateProxy.CurrentPipelineState;
}