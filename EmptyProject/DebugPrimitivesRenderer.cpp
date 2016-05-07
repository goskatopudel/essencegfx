#include "DebugPrimitivesRendering.h"
#include "MathGeometry.h"
#include "MathFunctions.h"
#include "VideoMemory.h"
#include "Hash.h"

float4 LinearToSRGB(float4 Color) {
	for (u32 I = 0; I < 3; ++I) {
		Color[I] = Color[I] < 0.0031308f ? 12.92f * Color[I] : 1.055f * powf(abs(Color[I]), 1.0f / 2.4f) - 0.055f;
	}
	return Color;
}

float4 SRGBToLinear(float4 Color) {
	for (u32 I = 0; I < 3; ++I) {
		Color[I] = Color[I] < 0.04045f ? Color[I] / 12.92f : powf((abs(Color[I]) + 0.055f) / 1.055f, 2.4f);
	}
	return Color;
}

Color4b ToColor4b(float4 Color, bool ConvertToSRGB) {
	return Color4b((ConvertToSRGB ? LinearToSRGB(Color) : Color) * 255.f);
}

float4 HSVToSRGB(float4 Color) {
	u32 Hi = eastl::min((u32)(Color.r * 6), 5u);
	float f = Color.r * 6 - Hi;
	float p = Color.b * (1 - Color.g);
	float q = Color.b * (1 - f * Color.g);
	float t = Color.b * (1 - (1 - f) * Color.g);

	float3 C;
	switch (Hi) {
	case 0:
		C = float3(Color.g, t, p);
		break;
	case 1:
		C = float3(q, Color.g, p);
		break;
	case 2:
		C = float3(p, Color.g, t);
		break;
	case 3:
		C = float3(p, q, Color.g);
		break;
	case 4:
		C = float3(t, p, Color.g);
		break;
	case 5:
		C = float3(Color.g, p, q);
		break;
	}

	return float4(C, Color.w);
}

float frac(float X) {
	return X - fmodf(X, 1);
}

FPrettyColorFactory::FPrettyColorFactory(float Random) : R(Random) {
}

Color4b FPrettyColorFactory::GetNext(float Sat, float Val, bool bSRGB) {
	const float GR = 0.618033988749895f;
	R = frac(R + GR);
	float4 Color = HSVToSRGB(float4(R, Sat, Val, 1));
	if (!bSRGB) {
		Color = SRGBToLinear(Color);
	}
	return ToColor4b(Color, false);
} 

void FDebugPrimitivesAccumulator::AddLine(float3 P0, float3 P1, Color4b Color) {
	if (!Batches.size() || Batches.back().Type != EPrimitive::Line) {
		FBatch Batch = {};
		Batch.Num = 0;
		Batch.Type = EPrimitive::Line;
		Batches.push_back(Batch);
	}

	FDebugVertex V0;
	V0.Position = P0;
	V0.Color = Color;
	Vertices.push_back(V0);
	FDebugVertex V1;
	V1.Position = P1;
	V1.Color = Color;
	Vertices.push_back(V1);

	Batches.back().Num++;
}

void FDebugPrimitivesAccumulator::AddPolygon(float3 P0, float3 P1, float3 P2, Color4b Color) {
	if (!Batches.size() || Batches.back().Type != EPrimitive::Polygon) {
		FBatch Batch = {};
		Batch.Num = 0;
		Batch.Type = EPrimitive::Polygon;
		Batches.push_back(Batch);
	}

	FDebugVertex V0;
	V0.Position = P0;
	V0.Color = Color;
	Vertices.push_back(V0);
	FDebugVertex V1;
	V1.Position = P1;
	V1.Color = Color;
	Vertices.push_back(V1);
	FDebugVertex V2;
	V2.Position = P2;
	V2.Color = Color;
	Vertices.push_back(V2);

	Batches.back().Num++;
}

void FDebugPrimitivesAccumulator::AddBBox(FBBox const& BBox, Color4b Color) {
	float3 Extent = BBox.GetExtent() * 2.f;
	float3 A = BBox.VMin;
	float3 B = A + float3(Extent.x, 0, 0);
	float3 C = B + float3(0, Extent.y, 0);
	float3 D = C - float3(Extent.x, 0, 0);

	float3 E = A + float3(0, 0, Extent.z);
	float3 F = B + float3(0, 0, Extent.z);
	float3 G = C + float3(0, 0, Extent.z);
	float3 H = D + float3(0, 0, Extent.z);

	AddLine(A, B, Color);
	AddLine(B, C, Color);
	AddLine(C, D, Color);
	AddLine(A, D, Color);
	AddLine(E, F, Color);
	AddLine(F, G, Color);
	AddLine(G, H, Color);
	AddLine(E, H, Color);
	AddLine(A, E, Color);
	AddLine(B, F, Color);
	AddLine(C, G, Color);
	AddLine(D, H, Color);
}

#include "ModelHelpers.h"

void FDebugPrimitivesAccumulator::AddMeshWireframe(FEditorMesh * Mesh, Color4b Color) {
	u32 P = Mesh->GetIndicesNum() / 3;
	for (u32 Index = 0; Index < P; ++Index) {
		AddLine(
			Mesh->Positions[Mesh->Indices[Index * 3]], 
			Mesh->Positions[Mesh->Indices[Index * 3 + 1]],
			Color);
		AddLine(
			Mesh->Positions[Mesh->Indices[Index * 3 + 1]],
			Mesh->Positions[Mesh->Indices[Index * 3 + 2]],
			Color);
		AddLine(
			Mesh->Positions[Mesh->Indices[Index * 3]],
			Mesh->Positions[Mesh->Indices[Index * 3 + 2]],
			Color);
	}
}

void FDebugPrimitivesAccumulator::AddMeshPolygons(FEditorMesh * Mesh, Color4b Color) {
	u32 P = Mesh->GetIndicesNum() / 3;
	for (u32 Index = 0; Index < P; ++Index) {
		AddPolygon(
			Mesh->Positions[Mesh->Indices[Index * 3]],
			Mesh->Positions[Mesh->Indices[Index * 3 + 1]],
			Mesh->Positions[Mesh->Indices[Index * 3 + 2]],
			Color);
	}
}

#include "Pipeline.h"
#include "Shader.h"

class FDrawPrimitiveShaderState : public FShaderState {
public:
	struct FConstantBufferData {
		float4x4	ViewProjMatrix;
	};

	FConstantBuffer	ConstantBuffer;

	FDrawPrimitiveShaderState() :
		FShaderState(
			GetShader("Shaders/Primitive.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/Primitive.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		ConstantBuffer = Root->CreateConstantBuffer("Constants");
	}
};


void FDebugPrimitivesAccumulator::FlushToViewport(FGPUContext & Context, FRenderViewport const& Viewport) {
	if (Batches.size() == 0) {
		return;
	}

	static FInputLayout * InputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});

	FOwnedResource VertexBuffer = GetUploadAllocator()->CreateBuffer(Vertices.size() * sizeof(FDebugVertex), 8);
	memcpy(VertexBuffer->GetMappedPtr(), Vertices.data(), Vertices.size() * sizeof(FDebugVertex));
	FBufferLocation BufferLocation = {};
	BufferLocation.Address = VertexBuffer->GetGPUAddress();
	BufferLocation.Size = (u32)(Vertices.size() * sizeof(FDebugVertex));
	BufferLocation.Stride = sizeof(FDebugVertex);

	Context.SetVB(BufferLocation);
	static FDrawPrimitiveShaderState ShaderState;

	FDrawPrimitiveShaderState::FConstantBufferData Constants = {};
	Constants.ViewProjMatrix = Viewport.TViewProjectionMatrix;

	auto CBV = CreateCBVFromData(&ShaderState.ConstantBuffer, Constants);

	u32 StartVertex = 0;

	static FPipelineFactory LocalPipelineFactory;
	FPipelineState * PipelineState = nullptr;

	LocalPipelineFactory.SetInputLayout(InputLayout);
	LocalPipelineFactory.SetShaderState(&ShaderState);
	LocalPipelineFactory.SetDepthStencil(Viewport.DepthBuffer ? Viewport.DepthBuffer->GetWriteFormat() : NULL_FORMAT);
	LocalPipelineFactory.SetRenderTarget(Viewport.RenderTarget->GetWriteFormat(Viewport.OutputSRGB), 0);

	D3D12_BLEND_DESC BlendDesc;
	SetD3D12StateDefaults(&BlendDesc);
	BlendDesc.RenderTarget[0].BlendEnable = true;
	BlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	BlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	LocalPipelineFactory.SetBlendState(BlendDesc);

	Context.SetDepthStencil(Viewport.DepthBuffer->GetDSV());
	Context.SetRenderTarget(0, Viewport.RenderTarget->GetRTV(Viewport.RenderTarget->GetWriteFormat(Viewport.OutputSRGB)));

	for (auto & Batch : Batches) {
		if (Batch.Type == EPrimitive::Line) {			
			LocalPipelineFactory.SetTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

			Context.SetTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			Context.SetPSO(LocalPipelineFactory.GetPipelineState());
			Context.SetRoot(ShaderState.Root);

			Context.SetConstantBuffer(&ShaderState.ConstantBuffer, CBV);
			Context.Draw(Batch.Num * 2, StartVertex);
			StartVertex += Batch.Num * 2;
		}
		else if (Batch.Type == EPrimitive::Polygon) {
			LocalPipelineFactory.SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			Context.SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Context.SetPSO(LocalPipelineFactory.GetPipelineState());
			Context.SetRoot(ShaderState.Root);

			Context.SetConstantBuffer(&ShaderState.ConstantBuffer, CBV);
			Context.Draw(Batch.Num * 3, StartVertex);
			StartVertex += Batch.Num * 3;
		}
	}

	Batches.clear();
	Vertices.clear();
}