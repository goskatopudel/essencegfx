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

FRootSignature * FForwardPass::GetDefaultRootSignature() {
	static eastl::unique_ptr<FRootSignature> RootSignature;
	if (RootSignature.get()) {
		return RootSignature.get();
	}
	
	RootSignature = eastl::make_unique<FRootSignature>();
	RootSignature->InitDefault(D3D12_SHADER_VISIBILITY_PIXEL, STAGE_VERTEX | STAGE_PIXEL);
	RootSignature->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_VERTEX);
	RootSignature->AddTableCBVRange(0, 1, 2);
	RootSignature->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignature->AddTableSRVRange(0, 8, 1);
	RootSignature->AddTableCBVRange(0, 1, 1);
	RootSignature->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignature->AddTableSRVRange(0, 4, 0);
	RootSignature->AddTableCBVRange(0, 1, 0);
	RootSignature->AddTableParam(PARAM_3, D3D12_SHADER_VISIBILITY_VERTEX);
	RootSignature->AddTableCBVRange(0, 1, 0);
	RootSignature->SerializeAndCreate();

	return RootSignature.get();
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

FRootSignature * FDepthPrePass::GetDefaultRootSignature() {
	static eastl::unique_ptr<FRootSignature> RootSignature;
	if (RootSignature.get()) {
		return RootSignature.get();
	}

	RootSignature = eastl::make_unique<FRootSignature>();
	RootSignature->InitDefault(D3D12_SHADER_VISIBILITY_PIXEL, STAGE_VERTEX | STAGE_PIXEL);
	RootSignature->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_VERTEX);
	RootSignature->AddTableCBVRange(0, 1, 2);
	RootSignature->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignature->AddTableSRVRange(0, 8, 1);
	RootSignature->AddTableCBVRange(0, 1, 1);
	RootSignature->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignature->AddTableSRVRange(0, 4, 0);
	RootSignature->AddTableCBVRange(0, 1, 0);
	RootSignature->AddTableParam(PARAM_3, D3D12_SHADER_VISIBILITY_VERTEX);
	RootSignature->AddTableCBVRange(0, 1, 0);
	RootSignature->SerializeAndCreate();

	return RootSignature.get();
}