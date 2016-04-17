#include "Viewer.h"
#include "ModelHelpers.h"
#include "MathMatrix.h"
#include "Pipeline.h"
#include "Commands.h"
#include "Shader.h"
#include "Resource.h"
#include "Camera.h"

class FDebugModelShaderState : public FShaderState {
public:
	FConstantBuffer			FrameConstantBuffer;
	FConstantBuffer			ObjectConstantBuffer;

	struct FFrameConstantBufferData {
		float4x4 ViewProj;
	};

	struct FObjectConstantBufferData {
		float4x4 World;
	};

	FDebugModelShaderState() :
		FShaderState(
			GetShader("Shaders/DebugModel.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/DebugModel.hlsl", "PSNormal", "ps_5_0", {}, 0)) {}

	void InitParams() override final {

		FrameConstantBuffer = Root->CreateConstantBuffer("FrameConstants");
		ObjectConstantBuffer = Root->CreateConstantBuffer("ObjectConstants");
	}
};

void RenderModel(
	FGPUContext & Context,
	FEditorModel * Model, 
	FTransformation Transformation, 
	FRenderViewport const& Viewport, 
	FViewParams const& ViewParams
) {
	static FInputLayout * InputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});
	FDebugModelShaderState * UsedShaderState = nullptr;
	FPipelineState * UsedPipelineState = nullptr;

	bool SRGBTarget = true;

	if (ViewParams.Mode == EViewMode::Default) {

	}
	else if (ViewParams.Mode == EViewMode::Normals) {
		SRGBTarget = false;

		static FPipelineState * PipelineState;
		static FDebugModelShaderState ModelShaderState;

		if (true) {
			D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
			// PipelineDesc.SetRT(0, );
			PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			PipelineDesc.NumRenderTargets = 1;
			// PipelineDesc.SetDSV(0, );
			PipelineDesc.DSVFormat = Viewport.DepthBuffer->GetFormat();
			PipelineDesc.DepthStencilState.DepthEnable = true;
			PipelineDesc.RasterizerState.FillMode = ViewParams.Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

			PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
		}

		UsedShaderState = &ModelShaderState;
		UsedPipelineState = PipelineState;
	}

	using namespace DirectX;

	auto ProjectionMatrix = XMMatrixPerspectiveFovLH(
		3.14f * 0.25f,
		(float)Viewport.Resolution.x / (float)Viewport.Resolution.y,
		0.01f, 1000.f);

	auto ViewMatrix = XMMatrixLookToLH(
		ToSIMD(Viewport.Camera->Position),
		ToSIMD(Viewport.Camera->Direction),
		ToSIMD(Viewport.Camera->Up));

	auto WorldTMatrix = XMMatrixTranspose(
		XMMatrixTranslation(Transformation.Position.x, Transformation.Position.y, Transformation.Position.z)
		* XMMatrixScaling(Transformation.Scale, Transformation.Scale, Transformation.Scale) 
		);

	XMVECTOR Determinant;
	auto InvViewTMatrix = XMMatrixTranspose(XMMatrixInverse(&Determinant, ViewMatrix));
	auto ViewProjTMatrix = XMMatrixTranspose(ViewMatrix * ProjectionMatrix);

	Context.SetRoot(UsedShaderState->Root);
	Context.SetPSO(UsedPipelineState);

	FDebugModelShaderState::FFrameConstantBufferData FrameConstants;
	XMStoreFloat4x4((XMFLOAT4X4*)&FrameConstants.ViewProj, ViewProjTMatrix);
	Context.SetConstantBuffer(&UsedShaderState->FrameConstantBuffer, CreateCBVFromData(&UsedShaderState->FrameConstantBuffer, FrameConstants));

	FDebugModelShaderState::FObjectConstantBufferData ObjectConstants;
	XMStoreFloat4x4((XMFLOAT4X4*)&ObjectConstants.World, WorldTMatrix);
	Context.SetConstantBuffer(&UsedShaderState->ObjectConstantBuffer, CreateCBVFromData(&UsedShaderState->ObjectConstantBuffer, ObjectConstants));

	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(SRGBTarget ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM));
	Context.SetDepthStencil(Viewport.DepthBuffer->GetDSV());
	Context.SetViewport(Viewport.RenderTarget->GetSizeAsViewport());

	u64 MeshesNum = Model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		Context.SetIB(Model->Meshes[MeshIndex].GetIndexBuffer());
		Context.SetVB(Model->Meshes[MeshIndex].GetVertexBuffer(), 0);
		Context.DrawIndexed(Model->Meshes[MeshIndex].IndicesNum);
	}
}

