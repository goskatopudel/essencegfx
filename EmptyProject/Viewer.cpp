#include "Viewer.h"
#include "ModelHelpers.h"
#include "MathMatrix.h"
#include "Pipeline.h"
#include "Commands.h"
#include "Shader.h"
#include "Resource.h"
#include "Camera.h"
#include "VideoMemory.h"

FOwnedResource ObjectPositionsTexture;
FOwnedResource ObjectNormalsTexture;

class FRenderToAtlasShaderState : public FShaderState {
public:
	struct FConstantBufferData {
	};

	FRenderToAtlasShaderState() :
		FShaderState(
			GetShader("Shaders/ModelAtlas.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/ModelAtlas.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
	}
};

#include "UtilStates.h"

void RenderAtlasView(FGPUContext & Context, FEditorModel * Model) {
	if (!ObjectPositionsTexture.IsValid()) {
		ObjectPositionsTexture = GetTexturesAllocator()->CreateTexture(1024, 1024, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, ALLOW_RENDER_TARGET, L"Positions");
		ObjectNormalsTexture = GetTexturesAllocator()->CreateTexture(1024, 1024, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, ALLOW_RENDER_TARGET, L"Normals");
	}

	static FInputLayout * InputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});

	FPipelineState * PipelineState = nullptr;
	static FRenderToAtlasShaderState ModelShaderState;

	if (!PipelineState) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		// PipelineDesc.SetRT(0, );
		PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32_FLOAT;
		PipelineDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		PipelineDesc.NumRenderTargets = 2;

		PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
	}

	static FCommandsStream Stream;

	Stream.Reset();

	Stream.SetAccess(ObjectPositionsTexture, EAccessType::WRITE_RT);
	Stream.SetAccess(ObjectNormalsTexture, EAccessType::WRITE_RT);

	Stream.SetPipelineState(PipelineState);

	Stream.SetRenderTarget(0, ObjectPositionsTexture->GetRTV());
	Stream.SetRenderTarget(1, ObjectNormalsTexture->GetRTV());
	Stream.SetDepthStencil({});
	Stream.SetViewport(ObjectPositionsTexture->GetSizeAsViewport());

	u64 MeshesNum = Model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		Stream.SetIB(Model->Meshes[MeshIndex].GetIndexBuffer());
		Stream.SetVB(Model->Meshes[MeshIndex].GetVertexBuffer(), 0);
		Stream.DrawIndexed(Model->Meshes[MeshIndex].GetIndicesNum());
	}

	Stream.SetAccess(ObjectPositionsTexture, EAccessType::READ_PIXEL);
	Stream.SetAccess(ObjectNormalsTexture, EAccessType::READ_PIXEL);

	FPipelineState * CopyPipelineState = nullptr;
	auto CopyShaderState = GetInstance<FCopyPSShaderState>();

	if (!CopyPipelineState) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		// PipelineDesc.SetRT(0, );
		PipelineDesc.RTVFormats[0] = GetBackbufferFormat();
		PipelineDesc.NumRenderTargets = 1;

		CopyPipelineState = GetGraphicsPipelineState(CopyShaderState, &PipelineDesc, InputLayout);
	}

	Stream.SetPipelineState(CopyPipelineState);

	Stream.SetRenderTarget(0, GetBackbuffer()->GetRTV());
	Stream.SetDepthStencil({});
	Stream.SetViewport(GetBackbuffer()->GetSizeAsViewport());
	Stream.SetTexture(&CopyShaderState->SourceTexture, ObjectNormalsTexture->GetSRV());
	Stream.Draw(3);

	Stream.Close();

	Playback(Context, &Stream);
}


FOwnedResource	UVMappingTexture;

void InitViewer(FGPUContext & Context) {
	if (!UVMappingTexture.IsValid()) {
		UVMappingTexture = LoadDDSImage(L"Textures/uvchecker.DDS", true, Context);
	}
}

class FDebugModelShaderState : public FShaderState {
public:
	FConstantBuffer			ConstantBuffer;
	FTextureParam			UVTexture;

	struct FConstantBufferData {
		float4x4	ViewProj;
		float4x4	World;
		float4x4	InvView;
		u32			CustomUint0;
	};

	FDebugModelShaderState() :
		FShaderState(
			GetShader("Shaders/DebugModel.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/DebugModel.hlsl", "PSDebug", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		ConstantBuffer = Root->CreateConstantBuffer("Constants");
		UVTexture = Root->CreateTextureParam("UVTexture");
	}
};

#include "BVH.h"
#include "DebugPrimitivesRendering.h"

void RenderModel(
	FGPUContext & Context,
	FEditorModel * Model, 
	FTransformation Transformation, 
	FRenderViewport const& Viewport, 
	FViewParams const& ViewParams
) {
	InitViewer(Context);

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

	static FDebugModelShaderState ModelShaderState;
	UsedShaderState = &ModelShaderState;

	if (true) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		// PipelineDesc.SetRT(0, );
		PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		PipelineDesc.NumRenderTargets = 1;
		// PipelineDesc.SetDSV(0, );
		PipelineDesc.DSVFormat = Viewport.DepthBuffer->GetFormat();
		PipelineDesc.DepthStencilState.DepthEnable = true;
		PipelineDesc.RasterizerState.FillMode = ViewParams.Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

		if (ViewParams.Mode == EViewMode::Default) {
			SRGBTarget = true;
			static FPipelineState * PipelineState;
			PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
			UsedPipelineState = PipelineState;
		}
		else if (ViewParams.Mode == EViewMode::Normals) {
			SRGBTarget = false;
			static FPipelineState * PipelineState;
			PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
			UsedPipelineState = PipelineState;
		}
		else if (ViewParams.Mode == EViewMode::Texcoord0 || ViewParams.Mode == EViewMode::Texcoord1) {
			SRGBTarget = true;
			static FPipelineState * PipelineState;
			PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
			UsedPipelineState = PipelineState;
		}
	}

	if (!UsedPipelineState) {
		return;
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

	FDebugModelShaderState::FConstantBufferData Constants;
	XMStoreFloat4x4((XMFLOAT4X4*)&Constants.ViewProj, ViewProjTMatrix);
	XMStoreFloat4x4((XMFLOAT4X4*)&Constants.World, WorldTMatrix);
	XMStoreFloat4x4((XMFLOAT4X4*)&Constants.InvView, InvViewTMatrix);
	Constants.CustomUint0 = (u32)ViewParams.Mode;
	Context.SetConstantBuffer(&UsedShaderState->ConstantBuffer, CreateCBVFromData(&UsedShaderState->ConstantBuffer, Constants));
	Context.SetTexture(&UsedShaderState->UVTexture, UVMappingTexture->GetSRV());

	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(SRGBTarget ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM));
	Context.SetDepthStencil(Viewport.DepthBuffer->GetDSV());
	Context.SetViewport(Viewport.RenderTarget->GetSizeAsViewport());

	u64 MeshesNum = Model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		Context.SetIB(Model->Meshes[MeshIndex].GetIndexBuffer());
		Context.SetVB(Model->Meshes[MeshIndex].GetVertexBuffer(), 0);
		Context.DrawIndexed(Model->Meshes[MeshIndex].GetIndicesNum());
	}

	if (ViewParams.DrawAtlas) {
		RenderAtlasView(Context, Model);
	}

	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(DXGI_FORMAT_R8G8B8A8_UNORM));

	static FDebugPrimitivesAccumulator DebugRender;
	FPrettyColorFactory	ColorFactory(0.5f);

	DebugRender.AddMeshWireframe(&Model->Meshes[0], ColorFactory.GetNext());

	DebugRender.FlushToViewport(Context, Viewport);

}

