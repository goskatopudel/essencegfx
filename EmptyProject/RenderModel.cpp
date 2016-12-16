#include "RenderModel.h"
#include "tiny_obj_loader.h"
#include "Print.h"
#include "Hash.h"
#include "RenderMaterial.h"

#include <EASTL\sort.h>

void PrepareDefaultMaterialDesc(FBasicMaterialDesc & MaterialDesc) {
	MaterialDesc.Diffuse = float3(0, 0, 0);
	MaterialDesc.DiffuseTexturePath = L"textures/checker.dds";
	MaterialDesc.bTransparent = 0;
}

FBufferLocation FRenderModel::GetVertexBufferView(u32 Stream) const {
	FBufferLocation Location;
	check(0);
	return Location;
}

FBufferLocation FRenderModel::GetIndexBufferView() const {
	FBufferLocation Location;
	check(0);
	return Location;
}

FRenderModelRef GetModel(const wchar_t * Filename, const wchar_t * Path, const wchar_t * TexturesPath) {
	eastl::wstring combinedpath = eastl::wstring(Path) + Filename;
	eastl::string scombinedpath = ConvertToString(combinedpath.data(), combinedpath.length());
	eastl::string path = ConvertToString(Path, wcslen(Path));

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string Err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &Err, scombinedpath.c_str(), path.c_str())) {
		PrintFormated(L"Failed to load %s: %s\n", combinedpath.c_str(), ConvertToWString(Err.c_str()).c_str());
		return{};
	}

	FRenderModelRef Model = eastl::make_shared<FRenderModel>();
	Model->Name = combinedpath;
	eastl::hash_map<u64, u32> VertexLookup;

	eastl::vector<FMeshRichVertex> Vertices;
	eastl::vector<u32> Indices;

	Model->InputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0) 
	});
	
	for (size_t s = 0; s < shapes.size(); s++) {
		u32 StartIndex = (u32)Indices.size();
		i32 BaseVertex = (i32)Vertices.size();
		VertexLookup.clear();
	
		// Loop over faces(polygon)
		size_t index_offset = 0;
		i32 material_index = -1;

		struct face_material_t {
			u32 face_index;
			i32 material_index;
		};

		eastl::vector<face_material_t> faceMaterials;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			face_material_t fm = {};
			fm.face_index = (u32)f;
			fm.material_index = shapes[s].mesh.material_ids[f];
			faceMaterials.push_back(fm);
		}
		eastl::stable_sort(faceMaterials.begin(), faceMaterials.end(), [](face_material_t a, face_material_t b) { return a.material_index < b.material_index; });

		for (size_t face_iter = 0; face_iter < shapes[s].mesh.num_face_vertices.size(); face_iter++) {
			material_index = shapes[s].mesh.material_ids[faceMaterials[face_iter].face_index];

			FBasicMaterialDesc MaterialDesc = {};
			if(material_index != -1)
			{
				MaterialDesc.Diffuse = float3(materials[material_index].diffuse[0], materials[material_index].diffuse[1], materials[material_index].diffuse[2]);
				MaterialDesc.DiffuseTexturePath = eastl::wstring(TexturesPath) + ConvertToWString(materials[material_index].diffuse_texname.c_str());
				MaterialDesc.bTransparent = materials[material_index].illum == 4;
			}
			else
			{
				PrepareDefaultMaterialDesc(MaterialDesc);
			}

			for (; face_iter < shapes[s].mesh.num_face_vertices.size(); face_iter++) {
				u32 f = faceMaterials[face_iter].face_index;

				if (shapes[s].mesh.material_ids[f] != material_index) {
					face_iter = face_iter - 1;
					material_index = -1;
					StartIndex = (u32)Indices.size();
					break;
				}

				int fv = shapes[s].mesh.num_face_vertices[f];
				i32 faceMaterial = shapes[s].mesh.material_ids[f];

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
					u64 key = MurmurHash2_64(&idx, sizeof(idx), 0);

					auto Iter = VertexLookup.find(key);
					if (Iter != VertexLookup.end()) {
						Indices.push_back(Iter->second);
					}
					else {
						Indices.push_back(u32(Vertices.size() - BaseVertex));
						FMeshRichVertex Vertex = {};
						Vertex.Position = float3(attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2]);
						if (attrib.normals.size()) {
							Vertex.Normal = float3(attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]);
						}
						if (attrib.texcoords.size()) {
							Vertex.Texcoord0 = float2(attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]);
						}
						Vertices.push_back(Vertex);
					}
				}
				index_offset += fv;
			}

			Model->Submeshes.push_back();
			FSubmesh & Submesh = Model->Submeshes[Model->Submeshes.size() - 1];
			Submesh.StartIndex = StartIndex;
			Submesh.IndicesNum = (u32)Indices.size() - StartIndex;
			Submesh.BaseVertex = BaseVertex;
			Submesh.Material = GetBasicMaterialInstance(MaterialDesc);
		}
	}

	return Model;
}