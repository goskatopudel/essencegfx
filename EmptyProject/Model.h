#pragma once
//
//#include "Essence.h"
//#include <EASTL\shared_ptr.h>
//#include <EASTL\vector.h>
//#include "MathVector.h"
//#include "Resource.h"
//#include "RenderMaterial.h"
//
//template<typename T>
//struct FAssetRef : public eastl::shared_ptr<T> {
//	using shared_ptr<T>::shared_ptr;
//	using shared_ptr<T>::operator=;
//
//	operator T *() const { return get(); };
//};
//
//struct FMeshRichVertex {
//	float3 Position;
//	float3 Normal;
//	float3 Tangent;
//	float3 Bitangent;
//	float2 Texcoord0;
//	float2 Texcoord1;
//	Color4b Color;
//};
//
//struct FMeshDraw {
//	u32 StartIndex;
//	u32 IndicesNum;
//	i32 BaseVertex;
//};
//
//class FModelFat {
//public:
//	eastl::wstring Name;
//
//	eastl::vector<FMeshRichVertex> Vertices;
//	eastl::vector<u32> Indices;
//
//	FGPUResourceRef VertexBuffer;
//	FGPUResourceRef IndexBuffer;
//};
//
//class FModel {
//public:
//	eastl::vector<FMeshDraw> Meshes;
//	eastl::vector<FRenderMaterialInstanceRef> MeshesMaterials;
//
//	FBufferLocation VertexBuffer;
//	FBufferLocation IndexBuffer;
//
//	eastl::unique_ptr<FModelFat> Fat;
//
//	u32 GetVerticesNum() const;
//	u32 GetIndicesNum() const;
//	bool ReadyForDrawing() const;
//	void CopyDataToGPUBuffers(FGPUContext & Context);
//};
//
//typedef FAssetRef<FModel> FModelRef;
//
////FModelRef GetModel(const wchar_t * Filename);
