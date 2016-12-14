#include "ForwardPass.h"
#include "Scene.h"

template<typename TConsumer>
void ForwardSetupState(FSceneRenderContext & RenderSceneContext, TConsumer & Consumer) {
	/*FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	Consumer.SetRenderTarget(ColorBuffer->GetRTV());
	Consumer.SetDepthStencil(DepthBuffer->GetDSV());*/


}

void FForwardPass::Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {
	/*FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	CmdStream.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH);
	CmdStream.SetAccess(ColorBuffer, EAccessType::WRITE_RT);

	ForwardSetupState(RenderSceneContext, CmdStream);*/

	CmdStream.ClearRTV(RenderSceneContext.GetColorBuffer()->GetRTV().RTV, float4(0.1f, 0, 0, 0));
}

void FForwardPass::PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FSceneRenderPass_MaterialInstanceRefParam Cachable) {
	/*FStateProxy<FDummyStateConsumer> StateProxy;
	StateProxy.SetCache(PipelineCache);

	ForwardSetupState(RenderSceneContext, StateProxy);

	StateProxy.ApplyState();
	Cachable->PSO = StateProxy.CurrentPipelineState;*/
}

void FForwardPass::QueryRenderTargets(FSceneRenderContext & SceneRenderContext, FRenderTargetsBundle & Bundle) {
	Bundle.RenderTargets[0].Resource = SceneRenderContext.GetColorBuffer();
	Bundle.RenderTargets[0].View = SceneRenderContext.GetColorBuffer()->GetRTV();

	Bundle.DepthStencil.Resource = SceneRenderContext.GetDepthBuffer();
	Bundle.DepthStencil.View = SceneRenderContext.GetDepthBuffer()->GetDSV();
}

void FDepthPrePass::Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {
	/*FGPUResource * DepthBuffer = RenderSceneContext.GetDepthBuffer();
	FGPUResource * ColorBuffer = RenderSceneContext.GetColorBuffer();
	CmdStream.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH);
	CmdStream.SetAccess(ColorBuffer, EAccessType::WRITE_RT);

	ForwardSetupState(RenderSceneContext, CmdStream);*/
}

void FDepthPrePass::PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FSceneRenderPass_MaterialInstanceRefParam Cachable) {
	/*FStateProxy<FDummyStateConsumer> StateProxy;
	StateProxy.SetCache(PipelineCache);

	ForwardSetupState(RenderSceneContext, StateProxy);

	StateProxy.ApplyState();
	Cachable->PSO = StateProxy.CurrentPipelineState;*/
}

void FDepthPrePass::QueryRenderTargets(FSceneRenderContext & SceneRenderContext, FRenderTargetsBundle & Bundle) {
	Bundle.DepthStencil.Resource = SceneRenderContext.GetDepthBuffer();
	Bundle.DepthStencil.View = SceneRenderContext.GetDepthBuffer()->GetDSV();
}