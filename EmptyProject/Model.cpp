//#include "tiny_obj_loader.h"
//#include "Model.h"
//#include <EASTL/string.h>
//#include "Print.h"
//#include "Hash.h"
//
//FModelRef GetModel(const wchar_t * Filename) {
//	eastl::string ObjFilenameStr = ConvertToString(Filename, wcslen(Filename));
//
//	tinyobj::attrib_t attrib;
//	std::vector<tinyobj::shape_t> shapes;
//	std::vector<tinyobj::material_t> materials;
//	std::string Err;
//	if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &Err, ObjFilenameStr.c_str())) {
//		PrintFormated(L"Failed to load %s: %s\n", Filename, ConvertToWString(Err.c_str()).c_str());
//		return {};
//	}
//
//	FModelRef Model = eastl::make_shared<FModel>();
//	Model->Fat = eastl::make_unique<FModelFat>();
//
//	Model->Fat->Name = Filename;
//
//	eastl::hash_map<u64, u32> VertexLookup;
//
//	for (size_t s = 0; s < shapes.size(); s++) {
//		u32 StartIndex = (u32)Model->Fat->Indices.size();
//		i32 BaseVertex = (i32)Model->Fat->Vertices.size();
//		VertexLookup.clear();
//
//		// Loop over faces(polygon)
//		size_t index_offset = 0;
//		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
//			int fv = shapes[s].mesh.num_face_vertices[f];
//
//			// Loop over vertices in the face.
//			for (size_t v = 0; v < fv; v++) {
//				// access to vertex
//				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
//				u64 key = MurmurHash2_64(&idx, sizeof(idx), 0);
//
//				auto Iter = VertexLookup.find(key);
//				if (Iter != VertexLookup.end()) {
//					Model->Fat->Indices.push_back(Iter->second);
//				}
//				else {
//					Model->Fat->Indices.push_back(u32(Model->Fat->Vertices.size() - BaseVertex));
//					FMeshRichVertex Vertex = {};
//					Vertex.Position = float3(attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2]);
//					if(attrib.normals.size()) {
//						Vertex.Normal = float3(attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]);
//					}
//					if (attrib.texcoords.size()) {
//						Vertex.Texcoord0 = float2(attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]);
//					}
//					Model->Fat->Vertices.push_back(Vertex);
//				}
//			}
//			index_offset += fv;
//
//			// per-face material
//			shapes[s].mesh.material_ids[f];
//		}
//
//		FMeshDraw Mesh;
//		Mesh.StartIndex = StartIndex;
//		Mesh.IndicesNum = (u32)Model->Fat->Indices.size() - StartIndex;
//		Mesh.BaseVertex = BaseVertex;
//		Model->Meshes.push_back(Mesh);
//		//Model->MeshesMaterials.push_back();
//	}
//
//	return Model;
//}
//
//#include "VideoMemory.h"
//
//bool FModel::ReadyForDrawing() const {
//	return Fat->VertexBuffer.get() && Fat->IndexBuffer.get();
//}
//
//u32 FModel::GetVerticesNum() const {
//	return (u32)Fat->Vertices.size();
//}
//
//u32 FModel::GetIndicesNum() const {
//	return (u32)Fat->Indices.size();
//}
//
//void FModel::CopyDataToGPUBuffers(FGPUContext & Context) {
//	Fat->IndexBuffer = GetBuffersAllocator()->CreateSimpleBuffer(Fat->Indices.size() * sizeof(u32), 0, L"IndexBuffer");
//	Fat->VertexBuffer = GetBuffersAllocator()->CreateSimpleBuffer(sizeof(FMeshRichVertex) * GetVerticesNum(), 0, L"VertexBuffer");
//
//	Context.CopyToBuffer(Fat->IndexBuffer, Fat->Indices.data(), Fat->Indices.size() * sizeof(u32));
//	Context.CopyToBuffer(Fat->VertexBuffer, Fat->Vertices.data(), sizeof(FMeshRichVertex) * GetVerticesNum());
//
//	Context.Barrier(Fat->IndexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_IB);
//	Context.Barrier(Fat->VertexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
//
//	VertexBuffer.Address = Fat->VertexBuffer->GetGPUAddress();
//	VertexBuffer.Size = sizeof(FMeshRichVertex) * GetVerticesNum();
//	VertexBuffer.Stride = sizeof(FMeshRichVertex);
//
//	IndexBuffer.Address = Fat->IndexBuffer->GetGPUAddress();
//	IndexBuffer.Size = sizeof(u32) * GetIndicesNum();
//	IndexBuffer.Stride = sizeof(u32);
//}