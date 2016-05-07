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
#include "DirectXMesh\DirectXMesh.h"
#include <EASTL/unique_ptr.h>
#include "tiny_obj_loader.h"
#define YO_NOINLINE
#define YO_NOIMG
#include "yocto_obj.h"

#if _DEBUG
#pragma comment(lib, "UVAtlas/debug/UVAtlas.lib")
#pragma comment(lib, "DirectXMesh/debug/DirectXMesh.lib")
#else 
#pragma comment(lib, "UVAtlas/release/UVAtlas.lib")
#pragma comment(lib, "DirectXMesh/release/DirectXMesh.lib")
#endif

void CreateAtlas(FEditorMesh * Mesh) {
	return;
	if (Mesh->Texcoords1.size() == 0) {
		using namespace DirectX;
		std::vector<UVAtlasVertex> MeshOutVertexBuffer;
		std::vector<u8> MeshOutIndexBuffer;
		std::vector<u32> FacePartitioning;
		std::vector<u32> VertexRemapArray;
		float MaxStretch;
		u64 NumCharts;

		std::vector<u32> Adjacency;
		Adjacency.resize(Mesh->GetIndicesNum());

		HRESULT Hr = DirectX::GenerateAdjacencyAndPointReps(
			Mesh->Indices.data(),
			Mesh->GetIndicesNum() / 3,
			(DirectX::XMFLOAT3*)Mesh->Positions.data(),
			Mesh->GetVerticesNum(),
			0.00001f,
			nullptr,
			Adjacency.data()
		);

		if (SUCCEEDED(Hr)) {
			PrintFormated(L"Adjacency created");
		}
		else {
			PrintFormated(L"Failed to create adjacency");
		}

		Hr = DirectX::UVAtlasCreate(
			(DirectX::XMFLOAT3*)Mesh->Positions.data(),
			Mesh->GetVerticesNum(),
			Mesh->Indices.data(),
			DXGI_FORMAT_R32_UINT,
			Mesh->GetIndicesNum() / 3,
			0,
			0.05f,
			1024,
			1024,
			2.5f,
			Adjacency.data(),
			nullptr,
			nullptr,
			nullptr,
			0.1f,
			DirectX::UVATLAS_DEFAULT,
			MeshOutVertexBuffer,
			MeshOutIndexBuffer,
			&FacePartitioning,
			&VertexRemapArray,
			&MaxStretch,
			&NumCharts);

		if (SUCCEEDED(Hr)) {
			PrintFormated(L"Atlas created");
		}
		else {
			PrintFormated(L"Failed to create atlas");
		}

		eastl::vector<float3>	Positions;
		eastl::vector<float3>	Normals;
		eastl::vector<u32>		Indices;
		eastl::vector<float2>	Texcoords1;

		Positions.resize(MeshOutVertexBuffer.size());
		Texcoords1.resize(MeshOutVertexBuffer.size());
		if (Mesh->Normals.size()) {
			Normals.resize(MeshOutVertexBuffer.size());
		}
		Indices.resize(MeshOutIndexBuffer.size() / sizeof(u32));

		for (u32 Index = 0; Index < MeshOutVertexBuffer.size(); Index++) {
			Positions[Index] = float3(MeshOutVertexBuffer[Index].pos.x, MeshOutVertexBuffer[Index].pos.y, MeshOutVertexBuffer[Index].pos.z);
			Texcoords1[Index] = float2(MeshOutVertexBuffer[Index].uv.x, MeshOutVertexBuffer[Index].uv.y);
		}

		for (u32 Index = 0; Index < Indices.size(); Index++) {
			Indices[Index] = ((u32*)MeshOutIndexBuffer.data())[Index];
		}

		if (Normals.size()) {
			for (u32 Index = 0; Index < VertexRemapArray.size(); Index++) {
				Normals[Index] = Mesh->Normals[VertexRemapArray[Index]];
			}
		}

		Mesh->Positions = std::move(Positions);
		Mesh->Normals = std::move(Normals);
		Mesh->Indices = std::move(Indices);
		Mesh->Texcoords1 = std::move(Texcoords1);
	}
}

u32		FEditorMesh::GetVerticesNum() {
	return (u32)Positions.size();
}

u32		FEditorMesh::GetIndicesNum() {
	return (u32)Indices.size();
}

void FEditorMesh::Clear() {
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
			Texcoords0.push_back(float2(ParShapesMesh->tcoords[Index * 2], ParShapesMesh->tcoords[Index * 2 + 1]));
		}
	}

	for (u32 Index = 0; Index < (u32)ParShapesMesh->ntriangles; Index++) {
		Indices.push_back(ParShapesMesh->triangles[Index * 3]);
		Indices.push_back(ParShapesMesh->triangles[Index * 3 + 1]);
		Indices.push_back(ParShapesMesh->triangles[Index * 3 + 2]);
	}
}

void FEditorMesh::CopyDataToBuffers(FGPUContext & Context) {
	IndexBuffer = GetBuffersAllocator()->CreateBuffer(Indices.size() * sizeof(u32), 0, L"IndexBuffer");
	VertexBuffer = GetBuffersAllocator()->CreateBuffer(sizeof(FEditorMeshVertex) * GetVerticesNum(), 0, L"VertexBuffer");

	Context.CopyToBuffer(IndexBuffer, Indices.data(), Indices.size() * sizeof(u32));

	eastl::vector<FEditorMeshVertex> VertexBufferData;
	VertexBufferData.resize(GetVerticesNum());

	for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
		VertexBufferData[Index].Position = Positions[Index];
	}
	if (Normals.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Normal = Normals[Index];
		}
	}
	if (Texcoords0.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Texcoord0 = Texcoords0[Index];
		}
	}
	if (Texcoords1.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Texcoord1 = Texcoords1[Index];
		}
	}
	if (Tangents.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Tangent = Tangents[Index];
		}
	}
	if (Bitangents.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Bitangent = Bitangents[Index];
		}
	}
	if (Colors.size()) {
		for (u32 Index = 0; Index < GetVerticesNum(); Index++) {
			VertexBufferData[Index].Color = Colors[Index];
		}
	}

	Context.CopyToBuffer(VertexBuffer, VertexBufferData.data(), sizeof(FEditorMeshVertex) * GetVerticesNum());

	Context.Barrier(IndexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_IB);
	Context.Barrier(VertexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
}

FBufferLocation FEditorMesh::GetIndexBuffer() {
	FBufferLocation Location;
	Location.Address = IndexBuffer->GetGPUAddress();
	Location.Size = sizeof(u32) * GetIndicesNum();
	Location.Stride = sizeof(u32);
	return Location;
}

FBufferLocation FEditorMesh::GetVertexBuffer() {
	FBufferLocation Location;
	Location.Address = VertexBuffer->GetGPUAddress();
	Location.Size = sizeof(FEditorMeshVertex) * GetVerticesNum();
	Location.Stride = sizeof(FEditorMeshVertex);
	return Location;
}

void FEditorModel::AddMesh(FEditorMesh && Mesh) {
	Meshes.emplace_back(Mesh);
}

void FEditorModel::CopyDataToBuffers(FGPUContext & Context) {
	for (auto & Mesh : Meshes) {
		Mesh.CopyDataToBuffers(Context);
	}
}

FEditorMesh CreateRock(i32 seed, i32 nsubdivisions) {
	par_shapes_mesh * par_mesh = par_shapes_create_rock(seed, nsubdivisions);
	FEditorMesh Mesh;
	Mesh.Consume(par_mesh);
	par_shapes_free_mesh(par_mesh);
	return std::move(Mesh);
}

FEditorMesh CreateSphere(i32 nsubdivisions) {
	par_shapes_mesh * par_mesh = par_shapes_create_subdivided_sphere(nsubdivisions);
	FEditorMesh Mesh;
	Mesh.Consume(par_mesh);
	par_shapes_free_mesh(par_mesh);
	return std::move(Mesh);
}

FEditorMesh CreatePlane(i32 slices, i32 stacks) {
	par_shapes_mesh * par_mesh = par_shapes_create_plane(slices, stacks);
	FEditorMesh Mesh;
	Mesh.Consume(par_mesh);
	par_shapes_free_mesh(par_mesh);
	return std::move(Mesh);
}

FEditorMesh CreateKleinBottle(i32 slices, i32 stacks) {
	par_shapes_mesh * par_mesh = par_shapes_create_klein_bottle(slices, stacks);
	FEditorMesh Mesh;
	Mesh.Consume(par_mesh);
	par_shapes_free_mesh(par_mesh);
	return std::move(Mesh);
}

FEditorMesh CreateMeshFromYoShape(yo_shape * shape) {
	FEditorMesh Mesh;

	u32 VerticesNum = shape->nverts;

	if (shape->pos) {
		Mesh.Positions.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Positions[i] = float3(shape->pos[3 * i], shape->pos[3 * i + 1], shape->pos[3 * i + 2]);
		}
	}
	if (shape->norm) {
		Mesh.Normals.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Normals[i] = float3(shape->norm[3 * i], shape->norm[3 * i + 1], shape->norm[3 * i + 2]);
		}
	}
	if (shape->texcoord) {
		Mesh.Texcoords0.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Texcoords0[i] = float2(shape->texcoord[2 * i], shape->texcoord[2 * i + 1]);
		}
	}
	if (shape->color) {
		Mesh.Colors.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Colors[i] = Color4b(
				(u8)(255 * shape->color[3 * i]), 
				(u8)(255 * shape->color[3 * i + 1]), 
				(u8)(255 * shape->color[3 * i + 2]), 
				1);
		}
	}

	if (shape->elem) {
		Mesh.Indices.resize(shape->nelems);
		for (i32 i = 0; i < shape->nelems; i++) {
			Mesh.Indices[i] = shape->elem[i];
		}
	}
	return std::move(Mesh);
}

FEditorMesh CreateMeshFromTinyObjShape(tinyobj::shape_t * Shape) {
	FEditorMesh Mesh;
	u32 VerticesNum = (u32)Shape->mesh.positions.size() / 3;
	u32 IndicesNum = (u32)Shape->mesh.indices.size();

	Mesh.Positions.resize(VerticesNum);
	for (u32 i = 0; i < VerticesNum; i++) {
		Mesh.Positions[i] = float3(
			Shape->mesh.positions[3 * i], 
			Shape->mesh.positions[3 * i + 1], 
			Shape->mesh.positions[3 * i + 2]);
	}

	if (Shape->mesh.normals.size()) {
		Mesh.Normals.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Normals[i] = float3(
				Shape->mesh.normals[3 * i],
				Shape->mesh.normals[3 * i + 1], 
				Shape->mesh.normals[3 * i + 2]);
		}
	}
	if (Shape->mesh.texcoords.size()) {
		Mesh.Texcoords0.resize(VerticesNum);
		for (u32 i = 0; i < VerticesNum; i++) {
			Mesh.Texcoords0[i] = float2(
				Shape->mesh.texcoords[2 * i],
				Shape->mesh.texcoords[2 * i + 1]);
		}
	}

	Mesh.Indices.resize(IndicesNum);
	for (u32 i = 0; i < IndicesNum; i++) {
		Mesh.Indices[i] = Shape->mesh.indices[i];
	}

	return std::move(Mesh);
}

#include "BVH.h"

void LoadOBJ(FEditorModel * Model, const wchar_t * ObjFilename, const wchar_t * CachedFilename) {
	eastl::string ObjFilenameStr = ConvertToString(ObjFilename, wcslen(ObjFilename));
	eastl::string CachedFilenameStr;
	if (CachedFilename) {
		ConvertToString(CachedFilename, wcslen(CachedFilename));
	}

	std::vector<tinyobj::shape_t>		LoadedShapes;
	std::vector<tinyobj::material_t>	LoadedMaterials;
	std::string Err;

	bool isLoaded = tinyobj::LoadObj(LoadedShapes, LoadedMaterials, Err, ObjFilenameStr.c_str(), "Models/");

	if (isLoaded) {
		for (auto& Shape : LoadedShapes) {
			Model->AddMesh(CreateMeshFromTinyObjShape(&Shape));
		}
		CreateAtlas(&Model->Meshes[0]);
		//BuildBVH(&Model->Meshes[0]);
	}
}

//void LoadOBJ(FEditorModel * Model, const wchar_t * ObjFilename, const wchar_t * CachedFilename) {
//	yo_scene * loaded_scene = nullptr;
//
//	eastl::string ObjFilenameStr = ConvertToString(ObjFilename, wcslen(ObjFilename));
//	eastl::string CachedFilenameStr;
//	if (CachedFilename) {
//		ConvertToString(CachedFilename, wcslen(CachedFilename));
//	}
//
//	loaded_scene = nullptr;
//	if (CachedFilename) {
//		loaded_scene = yo_load_objbin(CachedFilenameStr.c_str(), false);
//	}
//	if (!loaded_scene) {
//		if (CachedFilename) {
//			PrintFormated(L"Failed to load bin cache %s, fallback to obj\n", CachedFilename);
//		}
//		loaded_scene = yo_load_obj(ObjFilenameStr.c_str(), false, false);
//		if (!loaded_scene) {
//			PrintFormated(L"Failed to load %s obj\n", ObjFilename);
//		}
//		else if(CachedFilename){
//			bool cached = yo_save_objbin(CachedFilenameStr.c_str(), loaded_scene, false);
//			if (cached) {
//				PrintFormated(L"Cached %s\n", CachedFilename);
//			}
//			else {
//				PrintFormated(L"Failed to store cache %s\n", CachedFilename);
//			}
//		}
//	}
//	else {
//		PrintFormated(L"Loaded %s\n", CachedFilename);
//	}
//
//	if (loaded_scene) {
//		if (Model) {
//			for (i32 Shape = 0; Shape < loaded_scene->nshapes; Shape++) {
//				Model->AddMesh(CreateMeshFromYoShape(&loaded_scene->shapes[Shape]));
//			}
//			CreateAtlas(&Model->Meshes[0]);
//		}
//
//		yo_free_scene(loaded_scene);
//	}
//}