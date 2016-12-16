#pragma once
#include "Resource.h"
#include "RenderMaterial.h"

struct FMeshRichVertex {
	float3 Position;
	float3 Normal;
	float3 Tangent;
	float3 Bitangent;
	float2 Texcoord0;
	float2 Texcoord1;
	Color4b Color;
};

struct FSubmesh {
	u32 StartIndex;
	u32 IndicesNum;
	i32 BaseVertex;
	FRenderMaterialInstanceRef Material;
};

class FRenderModel {
public:
	eastl::wstring Name;
	FGPUResourceRef VertexBuffer;
	FGPUResourceRef IndexBuffer;
	FInputLayout * InputLayout;
	eastl::vector<FSubmesh> Submeshes;
	FBufferLocation GetVertexBufferView(u32 Stream = 0) const;
	FBufferLocation GetIndexBufferView() const;
};
DECORATE_CLASS_REF(FRenderModel);


FRenderModelRef GetModel(const wchar_t * Filename, const wchar_t * Path, const wchar_t * TexturesPath);