#include "BasePass.h"
#include "Model.h"
#include <DirectXMath.h>
#include "Scene.h"
#include "Application.h"
#include "Shader.h"
#include "Pipeline.h"
using namespace DirectX;

class FStaticModelShaderState_Shadow : public FShaderState {
public:
	FConstantBuffer FrameCB;
	FConstantBuffer ObjectCB;
	FTextureParam ShadowmapTexture;
	FTextureParam ShadowmapM2Texture;

	FStaticModelShaderState_Shadow() :
		FShaderState(
			GetShader("Shaders/VSMShadow.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/VSMShadow.hlsl", "PixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		FrameCB = Root->CreateConstantBuffer(this, "FrameConstants");
		ObjectCB = Root->CreateConstantBuffer(this, "ObjectConstants");

		ShadowmapTexture = Root->CreateTextureParam(this, "ShadowmapTexture");
		ShadowmapM2Texture = Root->CreateTextureParam(this, "ShadowmapM2Texture");
	}
};

void PreRender_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene) {
	Commands.SetRenderTargetsBundle(&Viewport->RenderTargets);
	Commands.ClearDSV(Viewport->RenderTargets.DepthBuffer->GetDSV());
	Commands.SetViewport(Viewport->RenderTargets.Viewport);
}

void RenderModel_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene, FSceneStaticMesh * StaticMesh) {
	// 
	static FStaticModelShaderState_Shadow ShaderState;
	static FPipelineState * PipelineState;

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
	PipelineContext.ApplyState();

	FFrameConstants FrameConstants;
	StoreTransposed(Load(Viewport->ViewProjectionMatrix), &FrameConstants.ViewProjectionMatrix);
	FrameConstants.ScreenResolution = float2((float)GApplication::WindowWidth, (float)GApplication::WindowHeight);

	FObjectConstants ObjectConstants;
	auto WorldMatrix = GetWorldMatrix(StaticMesh->Position, 1);
	StoreTransposed(WorldMatrix, &ObjectConstants.WorldMatrix);
	StoreTransposed(WorldMatrix * Load(Viewport->WorldToShadowmap), &ObjectConstants.ShadowMatrix);

	Commands.SetConstantBufferData(&ShaderState.FrameCB, &FrameConstants, sizeof(FrameConstants));
	Commands.SetConstantBufferData(&ShaderState.ObjectCB, &ObjectConstants, sizeof(ObjectConstants));
	Commands.SetTexture(&ShaderState.ShadowmapTexture, Viewport->Shadowmap->GetSRV());
	Commands.SetTexture(&ShaderState.ShadowmapM2Texture, Viewport->ShadowmapM2->GetSRV());

	Commands.SetVB(StaticMesh->Model->VertexBuffer, 0);
	Commands.SetIB(StaticMesh->Model->IndexBuffer);
	for (auto & Mesh : StaticMesh->Model->Meshes) {
		Commands.DrawIndexed(Mesh.IndicesNum, Mesh.StartIndex, Mesh.BaseVertex);
	}
}

void Render_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene) {

	PreRender_Forward(Commands, Viewport, Scene);
	for (auto StaticMeshPtr : Scene->StaticMeshes) {
		RenderModel_Forward(Commands, Viewport, Scene, StaticMeshPtr);
	}
}
