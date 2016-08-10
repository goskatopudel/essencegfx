#include "DepthPass.h"
#include "Model.h"
#include <DirectXMath.h>
#include "Scene.h"
#include "Shader.h"
#include "Pipeline.h"
using namespace DirectX;

struct FShadowProjectionConstants {
	float4x4 WorldViewProjectionMatrix;
};

template<bool VSM>
class FStaticModelShaderState_Depth : public FShaderState {
public:
	FConstantBuffer ShadowCB;

	FStaticModelShaderState_Depth() :
		FShaderState(
			GetShader("Shaders/DepthPass.hlsl", "VertexMain", "vs_5_1", {}, 0),
			VSM ? GetShader("Shaders/DepthPass.hlsl", "PixelMain", "ps_5_1", {}, 0) : NO_SHADER) {}

	void InitParams() override final {
		ShadowCB = Root->CreateConstantBuffer(this, "Constants");
	}
};

void PreRender_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene) {
	Commands.SetRenderTargetsBundle(&Viewport->RenderTargets);
	Commands.ClearDSV(Viewport->RenderTargets.DepthBuffer->GetDSV());
	Commands.ClearRTV(Viewport->RenderTargets.Outputs[0].GetRTV(), 0.f);
	Commands.SetViewport(Viewport->RenderTargets.Viewport);
}

void RenderModel_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene, FSceneStaticMesh * StaticMesh) {
	static FStaticModelShaderState_Depth<true> ShaderState;
	
	static FInputLayout * StaticMeshInputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});

	static FPipelineCache PipelineCache;
	static FPipelineContext<FCommandsStream> PipelineContext;

	PipelineContext.Bind(&Commands, &PipelineCache);
	PipelineContext.SetInputLayout(StaticMeshInputLayout);
	PipelineContext.SetRenderTargetsBundle(&Viewport->RenderTargets);
	PipelineContext.SetShaderState(&ShaderState);

	D3D12_RASTERIZER_DESC Desc;
	SetD3D12StateDefaults(&Desc);
	Desc.CullMode = D3D12_CULL_MODE_NONE;
	PipelineContext.SetRasterizerState(Desc);

	PipelineContext.ApplyState();

	FShadowProjectionConstants Constants;
	StoreTransposed(GetWorldMatrix(StaticMesh->Position, 1) * Load(Viewport->ViewProjectionMatrix), &Constants.WorldViewProjectionMatrix);
	Commands.SetConstantBufferData(&ShaderState.ShadowCB, &Constants, sizeof(Constants));

	Commands.SetVB(StaticMesh->Model->VertexBuffer, 0);
	Commands.SetIB(StaticMesh->Model->IndexBuffer);
	for (auto & Mesh : StaticMesh->Model->Meshes) {
		Commands.DrawIndexed(Mesh.IndicesNum, Mesh.StartIndex, Mesh.BaseVertex);
	}
}

void Render_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene) {

	PreRender_Depth(Commands, Viewport, Scene);
	for (auto StaticMeshPtr : Scene->StaticMeshes) {
		RenderModel_Depth(Commands, Viewport, Scene, StaticMeshPtr);
	}
}