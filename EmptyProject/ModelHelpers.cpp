#include "ModelHelpers.h"
#include "mikktspace.h"
#include "Print.h"
#include "Resource.h"
#include "VideoMemory.h"
#include <DirectXMath.h>
#include "UVAtlas\UVAtlas.h"
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/hash_map.h>
#include "MathVector.h"
#include "MathFunctions.h"
#include <EASTL/unique_ptr.h>
#define YO_NOINLINE
#define YO_NOIMG
#include "yocto_obj.h"

#if _DEBUG
#pragma comment(lib, "UVAtlas/debug/UVAtlas.lib")
#else 
#pragma comment(lib, "UVAtlas/release/UVAtlas.lib")
#endif

void CreateAtlas() {
	/*yo_scene * scene;
	scene->shapes[0].*/
	//DirectX::UVAtlasCreate(,

}

void FEditorMesh::Clear() {
	VerticesNum = 0;
	IndicesNum = 0;
	Positions.clear();
	Normals.clear();
	Tangents.clear();
	Bitangents.clear();
	Texcoords0.clear();
	Texcoords1.clear();
	Colors.clear();
	Indices.clear();
}

void FEditorMesh::Consume(par_shapes_mesh * ParShapesMesh) {
	Clear();

	for (u32 Index = 0; Index < (u32)ParShapesMesh->npoints; Index++) {
		Positions.push_back(float3(ParShapesMesh->points[Index * 3], ParShapesMesh->points[Index * 3 + 1], ParShapesMesh->points[Index * 3 + 2]));
	}
	if (ParShapesMesh->normals) {
		for (u32 Index = 0; Index < (u32)ParShapesMesh->npoints; Index++) {
			Normals.push_back(float3(ParShapesMesh->normals[Index * 3], ParShapesMesh->normals[Index * 3 + 1], ParShapesMesh->normals[Index * 3 + 2]));
		}
	}
	if (ParShapesMesh->tcoords) {
		for (u32 Index = 0; Index < (u32)ParShapesMesh->npoints; Index++) {
			Texcoords0.push_back(float2(ParShapesMesh->tcoords[Index * 2], ParShapesMesh->points[Index * 2 + 1]));
		}
	}

	for (u32 Index = 0; Index < (u32)ParShapesMesh->ntriangles; Index++) {
		Indices.push_back(ParShapesMesh->triangles[Index * 3]);
		Indices.push_back(ParShapesMesh->triangles[Index * 3 + 1]);
		Indices.push_back(ParShapesMesh->triangles[Index * 3 + 2]);
	}

	VerticesNum = ParShapesMesh->npoints;
	IndicesNum = ParShapesMesh->ntriangles * 3;
}

void FEditorMesh::CopyDataToBuffers(FGPUContext & Context) {
	IndexBuffer = GetBuffersAllocator()->CreateBuffer(Indices.size() * sizeof(u32), 0, L"IndexBuffer");
	VertexBuffer = GetBuffersAllocator()->CreateBuffer(sizeof(FEditorMeshVertex) * VerticesNum, 0, L"VertexBuffer");

	Context.CopyToBuffer(IndexBuffer, Indices.data(), Indices.size() * sizeof(u32));

	eastl::vector<FEditorMeshVertex> VertexBufferData;
	VertexBufferData.resize(VerticesNum);

	for (u32 Index = 0; Index < VerticesNum; Index++) {
		VertexBufferData[Index].Position = Positions[Index];
	}
	if (Normals.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Normal = Normals[Index];
		}
	}
	if (Texcoords0.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Texcoord0 = Texcoords0[Index];
		}
	}
	if (Texcoords1.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Texcoord1 = Texcoords1[Index];
		}
	}
	if (Tangents.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Tangent = Tangents[Index];
		}
	}
	if (Bitangents.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Bitangent = Bitangents[Index];
		}
	}
	if (Colors.size()) {
		for (u32 Index = 0; Index < VerticesNum; Index++) {
			VertexBufferData[Index].Color = Colors[Index];
		}
	}

	Context.CopyToBuffer(VertexBuffer, VertexBufferData.data(), sizeof(FEditorMeshVertex) * VerticesNum);

	Context.Barrier(IndexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_IB);
	Context.Barrier(VertexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
}

FBufferLocation FEditorMesh::GetIndexBuffer() {
	FBufferLocation Location;
	Location.Address = IndexBuffer->GetGPUAddress();
	Location.Size = sizeof(u32) * IndicesNum;
	Location.Stride = sizeof(u32);
	return Location;
}

FBufferLocation FEditorMesh::GetVertexBuffer() {
	FBufferLocation Location;
	Location.Address = VertexBuffer->GetGPUAddress();
	Location.Size = sizeof(FEditorMeshVertex) * VerticesNum;
	Location.Stride = sizeof(FEditorMeshVertex);
	return Location;
}

void FEditorModel::AddMesh(FEditorMesh && Mesh) {
	Meshes.emplace_back(Mesh);
}

FEditorMesh CreateRock(i32 seed, i32 nsubdivisions) {
	par_shapes_mesh * par_mesh = par_shapes_create_rock(seed, nsubdivisions);
	FEditorMesh Mesh;
	Mesh.Consume(par_mesh);
	par_shapes_free_mesh(par_mesh);
	return std::move(Mesh);
}

void LoadOBJ(const wchar_t * ObjFilename, const wchar_t * CachedFilename) {
	yo_scene * loaded_scene = nullptr;

	auto ObjFilenameStr = ConvertToString(ObjFilename, wcslen(ObjFilename));
	auto CachedFilenameStr = ConvertToString(CachedFilename, wcslen(CachedFilename));

	loaded_scene = yo_load_objbin(CachedFilenameStr.c_str(), false);
	if (!loaded_scene) {
		PrintFormated(L"Failed to load bin cache %s, fallback to obj\n", CachedFilename);
		loaded_scene = yo_load_obj(ObjFilenameStr.c_str(), true, false);
		if (!loaded_scene) {
			PrintFormated(L"Failed to load %s obj\n", ObjFilename);
		}
		else {
			bool cached = yo_save_objbin(CachedFilenameStr.c_str(), loaded_scene, false);
			if (cached) {
				PrintFormated(L"Cached %s\n", CachedFilename);
			}
			else {
				PrintFormated(L"Failed to store cache %s\n", CachedFilename);
			}
		}
	}
	else {
		PrintFormated(L"Loaded %s\n", CachedFilename);
	}

	if (loaded_scene) {
		yo_free_scene(loaded_scene);
	}
}