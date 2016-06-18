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
			GetShader("Shaders/TextureMap.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/TextureMap.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
	}
};

#include "UtilStates.h"

void RenderAtlas(FCommandsStream & CmdStream, FEditorModel * Model) {
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
		PipelineDesc.RTVFormats[0] = ObjectPositionsTexture->GetWriteFormat();
		PipelineDesc.RTVFormats[1] = ObjectNormalsTexture->GetWriteFormat();
		PipelineDesc.NumRenderTargets = 2;

		PipelineState = GetGraphicsPipelineState(&ModelShaderState, &PipelineDesc, InputLayout);
	}

	CmdStream.SetAccess(ObjectPositionsTexture, EAccessType::WRITE_RT);
	CmdStream.SetAccess(ObjectNormalsTexture, EAccessType::WRITE_RT);

	CmdStream.SetPipelineState(PipelineState);

	CmdStream.SetRenderTarget(0, ObjectPositionsTexture->GetRTV());
	CmdStream.SetRenderTarget(1, ObjectNormalsTexture->GetRTV());
	CmdStream.SetDepthStencil({});
	CmdStream.SetViewport(ObjectPositionsTexture->GetSizeAsViewport());

	u64 MeshesNum = Model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		CmdStream.SetIB(Model->Meshes[MeshIndex].GetIndexBuffer());
		CmdStream.SetVB(Model->Meshes[MeshIndex].GetVertexBuffer(), 0);
		CmdStream.DrawIndexed(Model->Meshes[MeshIndex].GetIndicesNum());
	}

	CmdStream.SetRenderTarget(1, {});
}

FOwnedResource AOTexture;
FOwnedResource AOTexture1;

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
		PipelineDesc.RTVFormats[0] = ObjectPositionsTexture->GetWriteFormat();
		PipelineDesc.RTVFormats[1] = ObjectNormalsTexture->GetWriteFormat();
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
		PipelineDesc.RTVFormats[0] = GetBackbufferFormat();
		PipelineDesc.RTVFormats[1] = NULL_FORMAT;
		PipelineDesc.NumRenderTargets = 1;

		CopyPipelineState = GetGraphicsPipelineState(CopyShaderState, &PipelineDesc, InputLayout);
	}

	Stream.SetPipelineState(CopyPipelineState);

	Stream.SetRenderTarget(0, GetBackbuffer()->GetRTV());
	Stream.SetRenderTarget(1, {});
	Stream.SetDepthStencil({});
	Stream.SetViewport(GetBackbuffer()->GetSizeAsViewport());
	Stream.SetTexture(&CopyShaderState->SourceTexture, AOTexture->GetSRV());
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
		ConstantBuffer = Root->CreateConstantBuffer(this, "Constants");
		UVTexture = Root->CreateTextureParam(this, "UVTexture");
	}
};

#include "BVH.h"
#include "DebugPrimitivesRendering.h"

float Halton(u32 Index, u32 Base) {
	float result = 0;
	float invbase = 1.f / (float)Base;
	float f = 1;
	u32 i = Index;
	while (i > 0) {
		f = f * invbase;
		result = result + f * (i % Base);
		i = i / Base;
	}
	return result;
}

class FBakeAOShaderState : public FShaderState {
public:
	FTextureParam	PositionsInput;
	FTextureParam	NormalsInput;
	FRWTextureParam OutTexture;
	FTextureParam	BVHNodes;
	FTextureParam	Primitives;
	FTextureParam	PositionsBuffer;
	FTextureParam	IndicesBuffer;
	FTextureParam	BlendTexture;

	FConstantBuffer			ConstantBuffer;

	struct FConstantBufferData {
		float2	Samples[16];
	};

	FBakeAOShaderState() :
		FShaderState(
			GetShader("Shaders/BakeTextureSignal.hlsl", "BakeAO", "cs_5_0", {}, 0)) {}

	void InitParams() override final {
		PositionsInput = Root->CreateTextureParam(this, "PositionsTexture");
		NormalsInput = Root->CreateTextureParam(this, "NormalsTexture");
		OutTexture = Root->CreateRWTextureParam(this, "BakedSignal");
		BVHNodes = Root->CreateTextureParam(this, "BVHNodes");
		Primitives = Root->CreateTextureParam(this, "Primitives");
		PositionsBuffer = Root->CreateTextureParam(this, "PositionsBuffer");
		IndicesBuffer = Root->CreateTextureParam(this, "IndicesBuffer");
		BlendTexture = Root->CreateTextureParam(this, "BlendTexture");

		ConstantBuffer = Root->CreateConstantBuffer(this, "Constants");
	}
};

FOwnedResource BVHNodes;
FOwnedResource BVHPrimitives;
FOwnedResource BVHIndices;
FOwnedResource BVHPositions;

void TransferBVHToGPU(FGPUContext & Context, FLinearBVH const * LinearBVH) {
	
	struct GPUBVHNode {
		float3 	VMin;
		u32		SecondChild;
		float3 	VMax;
		u32		PrimitivesNum;
		u32		PrimitivesOffset;
		u32		SplitAxis;
	};


	BVHNodes = GetBuffersAllocator()->CreateBuffer(LinearBVH->Nodes.size(), 4, sizeof(GPUBVHNode), EBufferFlags::ShaderReadable, L"BVHNodes");
	BVHPrimitives = GetBuffersAllocator()->CreateBuffer(LinearBVH->Primitives.size(), 4, sizeof(u32), EBufferFlags::ShaderReadable, L"BVHPrimitives");
	BVHPositions = GetBuffersAllocator()->CreateBuffer(LinearBVH->PositionsNum, 4, sizeof(float3), EBufferFlags::ShaderReadable, L"BVHPositions");
	BVHIndices = GetBuffersAllocator()->CreateBuffer(LinearBVH->IndicesNum, 4, sizeof(u32), EBufferFlags::ShaderReadable, L"BVHIndices");

	eastl::vector<GPUBVHNode> GPUNodes;
	GPUNodes.reserve(LinearBVH->Nodes.size());

	for (u32 Index = 0; Index < LinearBVH->Nodes.size(); Index++) {
		GPUNodes.push_back_uninitialized();
		GPUNodes.back().VMin = LinearBVH->Nodes[Index].Bounds.VMin;
		GPUNodes.back().SecondChild = LinearBVH->Nodes[Index].SecondChild;
		GPUNodes.back().VMax = LinearBVH->Nodes[Index].Bounds.VMax;
		GPUNodes.back().PrimitivesNum = LinearBVH->Nodes[Index].PrimitivesNum;
		GPUNodes.back().PrimitivesOffset = LinearBVH->Nodes[Index].PrimitivesOffset;
		GPUNodes.back().SplitAxis = LinearBVH->Nodes[Index].SplitAxis;
	}

	Context.CopyToBuffer(BVHNodes, GPUNodes.data(), LinearBVH->Nodes.size() * sizeof(GPUBVHNode));
	Context.CopyToBuffer(BVHPrimitives, LinearBVH->Primitives.data(), LinearBVH->Primitives.size() * sizeof(u32));
	Context.CopyToBuffer(BVHPositions, LinearBVH->Positions, LinearBVH->PositionsNum * sizeof(float3));
	Context.CopyToBuffer(BVHIndices, LinearBVH->Indices, LinearBVH->IndicesNum * sizeof(u32));

	Context.Barrier(BVHNodes, 0, EAccessType::COPY_DEST, EAccessType::READ_NON_PIXEL);
	Context.Barrier(BVHPrimitives, 0, EAccessType::COPY_DEST, EAccessType::READ_NON_PIXEL);
	Context.Barrier(BVHPositions, 0, EAccessType::COPY_DEST, EAccessType::READ_NON_PIXEL);
	Context.Barrier(BVHIndices, 0, EAccessType::COPY_DEST, EAccessType::READ_NON_PIXEL);
}

void BakeAO(FGPUContext & Context, FEditorModel * Model) {
	if (!AOTexture.IsValid()) {
		AOTexture = GetTexturesAllocator()->CreateTexture(1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_UNORM, ALLOW_UNORDERED_ACCESS, L"AO");
		AOTexture1 = GetTexturesAllocator()->CreateTexture(1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_UNORM, ALLOW_UNORDERED_ACCESS, L"AO1");
	}

	TransferBVHToGPU(Context, &Model->Meshes[0].BVH);

	static FBakeAOShaderState ShaderState;
	static FPipelineFactory LocalPipelineFactory;

	LocalPipelineFactory.SetShaderState(&ShaderState);
	FPipelineState * PipelineState = LocalPipelineFactory.GetPipelineState();

	static FCommandsStream CmdStream;
	CmdStream.Reset();

	RenderAtlas(CmdStream, Model);

	eastl::swap(AOTexture, AOTexture1);

	CmdStream.SetAccess(ObjectPositionsTexture, EAccessType::READ_NON_PIXEL);
	CmdStream.SetAccess(ObjectNormalsTexture, EAccessType::READ_NON_PIXEL);
	CmdStream.SetAccess(AOTexture, EAccessType::WRITE_UAV);
	CmdStream.SetAccess(AOTexture1, EAccessType::READ_NON_PIXEL);

	CmdStream.SetPipelineState(PipelineState);
	CmdStream.SetTexture(&ShaderState.PositionsInput, ObjectPositionsTexture->GetSRV());
	CmdStream.SetTexture(&ShaderState.NormalsInput, ObjectNormalsTexture->GetSRV());
	CmdStream.SetRWTexture(&ShaderState.OutTexture, AOTexture->GetUAV());
	CmdStream.SetTexture(&ShaderState.BVHNodes, BVHNodes->GetSRV());
	CmdStream.SetTexture(&ShaderState.Primitives, BVHPrimitives->GetSRV());
	CmdStream.SetTexture(&ShaderState.PositionsBuffer, BVHPositions->GetSRV());
	CmdStream.SetTexture(&ShaderState.IndicesBuffer, BVHIndices->GetSRV());
	CmdStream.SetTexture(&ShaderState.BlendTexture, AOTexture1->GetSRV());

	static u32 HaltonIndex = 0;

	FBakeAOShaderState::FConstantBufferData Constants;
	for (u32 Index = 0; Index < 16; ++Index) {
		Constants.Samples[Index].x = Halton(HaltonIndex, 2);
		Constants.Samples[Index].y = Halton(HaltonIndex, 2);
		++HaltonIndex;
	}
	CmdStream.SetConstantBuffer(&ShaderState.ConstantBuffer, CreateCBVFromData(&ShaderState.ConstantBuffer, Constants));

	CmdStream.Dispatch(1024 / 8, 1024 / 8, 1);

	CmdStream.SetAccess(AOTexture, EAccessType::READ_PIXEL);
	CmdStream.SetAccess(AOTexture1, EAccessType::READ_PIXEL);

	CmdStream.Close();
	Playback(Context, &CmdStream);
}

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

	if (ViewParams.DrawAtlas) {
		RenderAtlasView(Context, Model);
		return;
	}

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
		else if (ViewParams.Mode == EViewMode::BakedAO) {
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
	if (ViewParams.Mode == EViewMode::Texcoord0 || ViewParams.Mode == EViewMode::Texcoord1) {
		Context.SetTexture(&UsedShaderState->UVTexture, UVMappingTexture->GetSRV());
	}
	else if (ViewParams.Mode == EViewMode::BakedAO) {
		Context.SetTexture(&UsedShaderState->UVTexture, AOTexture->GetSRV());
	}
	else {
		Context.SetTexture(&UsedShaderState->UVTexture, UVMappingTexture->GetSRV());
	}

	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(SRGBTarget ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM));
	Context.SetDepthStencil(Viewport.DepthBuffer->GetDSV());
	Context.SetViewport(Viewport.RenderTarget->GetSizeAsViewport());

	u64 MeshesNum = Model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		Context.SetIB(Model->Meshes[MeshIndex].GetIndexBuffer());
		Context.SetVB(Model->Meshes[MeshIndex].GetVertexBuffer(), 0);
		Context.DrawIndexed(Model->Meshes[MeshIndex].GetIndicesNum());
	}

	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(DXGI_FORMAT_R8G8B8A8_UNORM));

	static FDebugPrimitivesAccumulator DebugRender;
	FPrettyColorFactory	ColorFactory(0.9f);

	/*for (u32 Index = 0; Index < Model->Meshes.size(); Index++) {
		Color4b Color = ColorFactory.GetNext();
		Color.a = 128;
		DebugRender.AddMeshWireframe(&Model->Meshes[Index], Color);
	}

	for (u32 Index = 0; Index < Model->Meshes.size(); Index++) {
		DebugRender.AddMeshNormals(&Model->Meshes[Index], 0.25f);
	}*/

	DebugRender.FlushToViewport(Context, Viewport);

}

