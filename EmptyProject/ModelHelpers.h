#pragma once
#include "Essence.h"
#include "MathVector.h"
#include <EASTL/vector.h>
#include "CommandStream.h"
#include "par_shapes.h"
#include "Resource.h"

void LoadOBJ(const wchar_t* filename, const wchar_t * CachedFilename);

struct FEditorMeshVertex{
	float3		Position;
	float3		Normal;
	float3		Tangent;
	float3		Bitangent;
	float2		Texcoord0;
	float2		Texcoord1;
	Color4b		Color;
};

class FEditorMesh {
public:
	eastl::vector<float3>	Positions;
	eastl::vector<float3>	Normals;
	eastl::vector<float3>	Tangents;
	eastl::vector<float3>	Bitangents;
	eastl::vector<float2>	Texcoords0;
	eastl::vector<float2>	Texcoords1;
	eastl::vector<Color4b>	Colors;
	eastl::vector<u32>		Indices;

	u32						VerticesNum;
	u32						IndicesNum;

	void Clear();
	void Consume(par_shapes_mesh * ParShapesMesh);

	FOwnedResource	VertexBuffer;
	FOwnedResource	IndexBuffer;

	void CopyDataToBuffers(FGPUContext & Context);

	FBufferLocation GetIndexBuffer();
	FBufferLocation GetVertexBuffer();
};

class FEditorModel {
public:
	eastl::vector<FEditorMesh>	Meshes;

	void AddMesh(FEditorMesh && Mesh);
};

FEditorMesh CreateRock(i32 seed, i32 nsubdivisions);