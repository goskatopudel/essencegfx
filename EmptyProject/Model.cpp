#include "Model.h"
#include "tiny_obj_loader.h"
#include "mikktspace.h"
#include "Print.h"
#include "Resource.h"
#include "VideoMemory.h"

#include <DirectXMath.h>
#include "UVAtlas\UVAtlas.h"

#if _DEBUG
#pragma comment(lib, "UVAtlas/debug/UVAtlas.lib")
#else 
#pragma comment(lib, "UVAtlas/release/UVAtlas.lib")
#endif

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/hash_map.h>
#include "MathVector.h"
#include "MathFunctions.h"
#include <EASTL/unique_ptr.h>

class FVertexContainer {
public:
	static const u32 MAX_STREAMS = 8;
	static const u32 MAX_U16 = 65535;

	struct FVertexStream {
		eastl::vector<u8>	Data;
		u16					Stride;
	};

	eastl::array<FVertexStream, MAX_STREAMS>		Streams;
	eastl::vector<u16>								Indices;
	u32												MaxIndex = 0;
	u32												VerticesNum = 0;

	FOwnedResource									IndexBuffer;
	eastl::array<FOwnedResource, MAX_STREAMS>		VertexBuffers;

	struct FAppendResult {
		u32		IndexCount;
		u32		StartIndex;
		i32		BaseVertex;
	};

	void AppendIndices(u32 const * InIndices, u64 InIndicesNum, u32 InVertexOffset, eastl::vector<FAppendResult> &outResult);

	void FeedStream(u32 stream, u8 const * src, u64 size);

	template<typename T> void FeedStream(u32 stream, T const & src) {
		FeedStream(stream, (u8 const *)&src, sizeof(T));
	}

	void CreateBuffers(FGPUContext & CopyContext) {
		IndexBuffer = GetBuffersAllocator()->CreateBuffer(Indices.size() * sizeof(u16), 0, L"IndexBuffer");
		VertexBuffers[0] = GetBuffersAllocator()->CreateBuffer(Streams[0].Data.size(), 0, L"VertexBuffer0");
		VertexBuffers[1] = GetBuffersAllocator()->CreateBuffer(Streams[1].Data.size(), 0, L"VertexBuffer1");

		CopyContext.CopyToBuffer(IndexBuffer, Indices.data(), Indices.size() * sizeof(u16));
		CopyContext.CopyToBuffer(VertexBuffers[0], Streams[0].Data.data(), Streams[0].Data.size());
		CopyContext.CopyToBuffer(VertexBuffers[1], Streams[1].Data.data(), Streams[1].Data.size());

		CopyContext.Barrier(IndexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_IB);
		CopyContext.Barrier(VertexBuffers[0], ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
		CopyContext.Barrier(VertexBuffers[1], ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
	}
};

void FVertexContainer::AppendIndices(u32 const * InIndices, u64 InIndicesNum, u32 InVertexOffset, eastl::vector<FAppendResult> &outResult) {
	outResult.clear();
	u32 N = (u32)InIndicesNum;
	u32 startIndex = (u32)0;
	u32 bufferOffset = (u32)Indices.size();

	while (startIndex < N) {
		u32 minIndex = InIndices[startIndex];
		u32 maxIndex = InIndices[startIndex];

		for (u32 index = startIndex; index < N; ++index) {
			minIndex = eastl::min(minIndex, InIndices[index]);
			maxIndex = eastl::min(maxIndex, InIndices[index]);
		}

		u32 endIndex = N;
		u32 upperLimit = MAX_U16 + minIndex;
		if (maxIndex > upperLimit) {
			u32 endIndex = startIndex;
			for (; endIndex < N; ++endIndex) {
				if (InIndices[endIndex] > upperLimit) {
					break;
				}
			}
		}

		for (u32 index = startIndex; index < endIndex; ++index) {
			Indices.push_back(InIndices[index] - minIndex);
		}

		maxIndex -= minIndex;
		MaxIndex = eastl::max(MaxIndex, maxIndex);
		i32 baseVertex = minIndex;

		FAppendResult result;
		result.IndexCount = endIndex - startIndex;
		result.StartIndex = bufferOffset + startIndex;
		result.BaseVertex = baseVertex + InVertexOffset;
		outResult.push_back(result);

		startIndex = endIndex;
	}
}

void FVertexContainer::FeedStream(u32 stream, u8 const * src, u64 size) {
	u64 offset = Streams[stream].Data.size();
	Streams[stream].Data.resize(offset + size);
	memcpy(Streams[stream].Data.data() + offset, src, size);
}

FVertexContainer	VertexContainer;

FBufferLocation GetIB() {
	FBufferLocation result;
	result.Address = VertexContainer.IndexBuffer->GetGPUAddress();
	result.Size = (u32)(VertexContainer.Indices.size() * sizeof(u16));
	result.Stride = 2;
	return result;
}

FBufferLocation GetVB(u32 stream) {
	FBufferLocation result;
	result.Address = VertexContainer.VertexBuffers[stream]->GetGPUAddress();
	result.Size = (u32)(VertexContainer.Streams[stream].Data.size());
	result.Stride = VertexContainer.Streams[stream].Stride;
	return result;
}

void UpdateGeometryBuffers(FGPUContext & Context) {
	VertexContainer.CreateBuffers(Context);
}

FModel::FModel(const wchar_t * filename) : 
	FatData(new FModelFat()),
	IndexStride(2)
	{
	FatData->Name = eastl::wstring(filename);
}

void FModel::AddMesh(eastl::wstring name, FMaterial* material, u32 indexCount, u32 startIndex, i32 baseVertex) {
	FMesh Mesh;
	Mesh.IndexCount = indexCount;
	Mesh.StartIndex = startIndex;
	Mesh.BaseVertex = baseVertex;
	Meshes.push_back(Mesh);

	FatData->FatMeshes.push_back();
	FatData->FatMeshes[FatData->FatMeshes.size() - 1].Material = material;
	FatData->FatMeshes[FatData->FatMeshes.size() - 1].Name = name;

	FatData->IndicesNum += indexCount;
}

FMaterial* CreateMaterial() {
	FMaterial* Material = new FMaterial();
	Material->FatData = eastl::make_unique<FMaterialFat>();
	return Material;
}

void	LoadMaterialTextures() {

}

#include <EASTL/set.h>

void	LoadModelTextures(FGPUContext & Context, FModel* Model) {
	for (auto & Mesh : Model->FatData->FatMeshes) {
		if (Mesh.Material) {
			Mesh.Material->FatData->BaseColorTexture = LoadDDSImage(Mesh.Material->FatData->BaseColorTextureFilename.c_str(), true, Context);
			Mesh.Material->FatData->MetallicTexture = LoadDDSImage(Mesh.Material->FatData->MetallicTextureFilename.c_str(), false, Context);
			Mesh.Material->FatData->NormalMapTexture = LoadDDSImage(Mesh.Material->FatData->NormalMapTextureFilename.c_str(), false, Context);
			Mesh.Material->FatData->RoughnessTexture = LoadDDSImage(Mesh.Material->FatData->RoughnessTextureFilename.c_str(), false, Context);
			if (Mesh.Material->FatData->AlphaMaskTextureFilename.size() > 0) {
				Mesh.Material->FatData->AlphaMaskTexture = LoadDDSImage(Mesh.Material->FatData->AlphaMaskTextureFilename.c_str(), false, Context);
			}
		}
	}
}

FAABB GenerateAABB(float3 const * positions, u32 num) {
	FAABB aabb;
	aabb.VecMax = -10000000.f;
	aabb.VecMin = 10000000.f;

	for (u32 index = 0; index < num; ++index) {
		aabb.VecMin = min(aabb.VecMin, *(const float3*)&(positions[index]));
		aabb.VecMax = max(aabb.VecMax, *(const float3*)&(positions[index]));
	}

	return aabb;
}

#include "FileIO.h"

struct FIOStream {
public:
	FILE * File = nullptr;

	~FIOStream() {
		if (File) {
			fclose(File);
			File = nullptr;
		}
	}

	void OpenForWrite(const wchar_t * filename) {
		verify(_wfopen_s(&File, filename, L"wb") == 0);
	}

	void Write(const void* Src, u64 Bytesize) {
		fwrite(Src, Bytesize, 1, File);
	}

	template<typename T>
	void Write(T const& TypedValue) {
		Write(&TypedValue, sizeof(T));
	}

	template<typename T>
	void Write(eastl::vector<T> const& Vector) {
		u64 size = Vector.size();
		Write(size);
		Write(Vector.data(), Vector.size() * sizeof(T));
	}

	template<>
	void Write(eastl::string const& Str) {
		u64 Len = Str.size();
		Write(Len);
		Write(Str.data(), Len + 1);
	}



	void OpenForRead(const wchar_t * filename) {
		verify(_wfopen_s(&File, filename, L"rb") == 0);
	}

	void Read(void* Dst, u64 Bytesize) {
		if (Bytesize) {
			verify(fread(Dst, Bytesize, 1, File) == 1);
		}
	}

	template<typename T>
	void Read(T & TypedRef) {
		Read(&TypedRef, sizeof(T));
	}

	template<typename T>
	void Read(eastl::vector<T> & Vector) {
		u64 size;
		Read(size);
		Vector.resize(size);
		Read(Vector.data(), Vector.size() * sizeof(T));
	}

	template<>
	void Read(eastl::string & Str) {
		u64 size;
		Read(size);
		Str.resize(size);
		Read((void*)Str.data(), size + 1);
	}
};

struct FBinaryModel {
	struct FSurfacePart {
		eastl::string				Name;
		u32							IndexCount;
		u32							IndexStart;
		i32							MaterialID;
	};

	struct FSurface {
		eastl::string				Name;
		eastl::vector<FSurfacePart>	Parts;
		eastl::vector<float3>		Positions;
		eastl::vector<float3>		Normals;
		eastl::vector<float2>		Texcoords;
		eastl::vector<u32>			Indices;
	};

	struct FSurfaceMaterial {
		eastl::string				Name;
		eastl::string				BaseColorTextureFilename;
		eastl::string				MetallicTextureFilename;
		eastl::string				RoughnessTextureFilename;
		eastl::string				NormalMapTextureFilename;
		eastl::string				AlphaMaskTextureFilename;
	};

	eastl::vector<FSurface>		Surfaces;
	eastl::vector<FSurfaceMaterial>	Materials;

	void Serialize(const wchar_t* filename) {
		FIOStream stream;
		stream.OpenForWrite(filename);
		u64 SurfacesNum = Surfaces.size();
		stream.Write(SurfacesNum);
		for (auto & Surface : Surfaces) {
			stream.Write(Surface.Name);
			u64 PartsNum = Surface.Parts.size();
			stream.Write(PartsNum);
			for (auto & Part : Surface.Parts) {
				stream.Write(Part.Name);
				stream.Write(Part.IndexCount);
				stream.Write(Part.IndexStart);
				stream.Write(Part.MaterialID);
			}
			stream.Write(Surface.Positions);
			stream.Write(Surface.Normals);
			stream.Write(Surface.Texcoords);
			stream.Write(Surface.Indices);
		}
		u64 MaterialsNum = Materials.size();
		stream.Write(MaterialsNum);
		for (auto & Material : Materials) {
			stream.Write(Material.Name);
			stream.Write(Material.BaseColorTextureFilename);
			stream.Write(Material.MetallicTextureFilename);
			stream.Write(Material.RoughnessTextureFilename);
			stream.Write(Material.NormalMapTextureFilename);
			stream.Write(Material.AlphaMaskTextureFilename);
		}
	}

	void Deserialize(const wchar_t* filename) {
		FIOStream stream;
		stream.OpenForRead(filename);
		u64 SurfacesNum;
		stream.Read(SurfacesNum);
		for (u64 i = 0; i < SurfacesNum; ++i) {
			FSurface Surface;
			stream.Read(Surface.Name);
			u64 PartsNum;
			stream.Read(PartsNum);
			for (u64 p = 0; p < PartsNum; ++p) {
				FSurfacePart Part;
				stream.Read(Part.Name);
				stream.Read(Part.IndexCount);
				stream.Read(Part.IndexStart);
				stream.Read(Part.MaterialID);
				Surface.Parts.push_back(std::move(Part));
			}
			stream.Read(Surface.Positions);
			stream.Read(Surface.Normals);
			stream.Read(Surface.Texcoords);
			stream.Read(Surface.Indices);
			Surfaces.push_back(std::move(Surface));
		}
		u64 MaterialsNum;
		stream.Read(MaterialsNum);
		for (u64 i = 0; i < MaterialsNum; ++i) {
			FSurfaceMaterial Material;
			stream.Read(Material.Name);
			stream.Read(Material.BaseColorTextureFilename);
			stream.Read(Material.MetallicTextureFilename);
			stream.Read(Material.RoughnessTextureFilename);
			stream.Read(Material.NormalMapTextureFilename);
			stream.Read(Material.AlphaMaskTextureFilename);
			Materials.push_back(std::move(Material));
		}
	}
};


void	ConvertObjToBinary(const wchar_t* inFilename, const wchar_t* outFilename) {
	std::vector<tinyobj::shape_t>		LoadedShapes;
	std::vector<tinyobj::material_t>	LoadedMaterials;
	std::string Err;

	auto filenameStr = ConvertToString(inFilename, wcslen(inFilename));

	bool isLoaded = tinyobj::LoadObj(LoadedShapes, LoadedMaterials, Err, filenameStr.c_str(), "Models/");

	if (isLoaded) {
		FBinaryModel	Binary;

		for (auto& Material : LoadedMaterials) {
			Binary.Materials.emplace_back();
			Binary.Materials.back().Name = eastl::string(Material.name.c_str());
			Binary.Materials.back().BaseColorTextureFilename = eastl::string(Material.diffuse_texname.c_str());
			Binary.Materials.back().MetallicTextureFilename = eastl::string(Material.ambient_texname.c_str());
			Binary.Materials.back().NormalMapTextureFilename = eastl::string(Material.bump_texname.c_str());
			Binary.Materials.back().RoughnessTextureFilename = eastl::string(Material.specular_highlight_texname.c_str());
			Binary.Materials.back().AlphaMaskTextureFilename = eastl::string(Material.alpha_texname.c_str());
		}

		for (auto& Shape : LoadedShapes) {
			Binary.Surfaces.emplace_back();
			Binary.Surfaces.back().Positions.resize(Shape.mesh.positions.size() / 3);
			memcpy(Binary.Surfaces.back().Positions.data(), Shape.mesh.positions.data(), sizeof(float3) * Binary.Surfaces.back().Positions.size());
			Binary.Surfaces.back().Normals.resize(Shape.mesh.normals.size() / 3);
			memcpy(Binary.Surfaces.back().Normals.data(), Shape.mesh.normals.data(), sizeof(float3) * Binary.Surfaces.back().Normals.size());
			Binary.Surfaces.back().Texcoords.resize(Shape.mesh.texcoords.size() / 2);
			memcpy(Binary.Surfaces.back().Texcoords.data(), Shape.mesh.texcoords.data(), sizeof(float2) * Binary.Surfaces.back().Texcoords.size());
			for (u32 index = 0; index < Binary.Surfaces.back().Texcoords.size(); index++) {
				Binary.Surfaces.back().Texcoords[index].y = 1.f - Binary.Surfaces.back().Texcoords[index].y;
			}
			Binary.Surfaces.back().Indices.resize(Shape.mesh.indices.size());
			memcpy(Binary.Surfaces.back().Indices.data(), Shape.mesh.indices.data(), sizeof(u32) * Binary.Surfaces.back().Indices.size());
			Binary.Surfaces.back().Name = eastl::string(Shape.name.c_str());

			FBinaryModel::FSurfacePart surfacePart = {};
			surfacePart.MaterialID = Shape.mesh.material_ids[0];
			surfacePart.Name.sprintf("%s_mat%d", Shape.name.c_str(), surfacePart.MaterialID);
			u32 facesNum = (u32)Shape.mesh.material_ids.size();
			for (u32 face = 0; face < facesNum; ++face) {
				surfacePart.IndexCount += 3;

				if (face + 1 == facesNum || Shape.mesh.material_ids[face + 1] != surfacePart.MaterialID) {
					Binary.Surfaces.back().Parts.push_back(std::move(surfacePart));

					surfacePart.IndexCount = 0;
					surfacePart.IndexStart = face * 3;
					if (face + 1 < facesNum) {
						surfacePart.MaterialID = Shape.mesh.material_ids[face + 1];
						surfacePart.Name.sprintf("%s_mat%d", Shape.name.c_str(), surfacePart.MaterialID);
					}
				}
			}
		}

		Binary.Serialize(outFilename);
	}
}

FModel*	LoadModelFromBinary(const wchar_t* filename) {
	FBinaryModel	Binary;
	Binary.Deserialize(filename);

	FModel* model = nullptr;

	VertexContainer.Streams[0].Stride = sizeof(FVertexPositionUV);
	VertexContainer.Streams[1].Stride = sizeof(FVertexNormal);

	model = new FModel(filename);
	eastl::vector<FMaterial*>	Materials;
	Materials.reserve(Binary.Materials.size());

	for (auto& ModelMaterial : Binary.Materials) {
		FMaterial* Material = CreateMaterial();
		Material->FatData->Name = ConvertToWString(ModelMaterial.Name);
		Material->FatData->BaseColorTextureFilename = ConvertToWString(ModelMaterial.BaseColorTextureFilename);
		Material->FatData->MetallicTextureFilename = ConvertToWString(ModelMaterial.MetallicTextureFilename);
		Material->FatData->RoughnessTextureFilename = ConvertToWString(ModelMaterial.RoughnessTextureFilename);
		Material->FatData->NormalMapTextureFilename = ConvertToWString(ModelMaterial.NormalMapTextureFilename);
		Materials.push_back(Material);
	}

	eastl::vector<FVertexContainer::FAppendResult> SubmeshesRaw;
	for (auto& Shape : Binary.Surfaces) {
		for (auto& part : Shape.Parts) {
			eastl::wstring shapeName = ConvertToWString(part.Name.data(), part.Name.size());
			VertexContainer.AppendIndices(Shape.Indices.data() + part.IndexStart, part.IndexCount, VertexContainer.VerticesNum, SubmeshesRaw);

			for (u32 submeshIndex = 0; submeshIndex < SubmeshesRaw.size(); submeshIndex++) {
				eastl::wstring submeshName;
				if (SubmeshesRaw.size() == 1) {
					submeshName.sprintf(L"%s (%u/%u)", shapeName.c_str(), submeshIndex, (u32)SubmeshesRaw.size());
				}
				else {
					submeshName.sprintf(L"%s", shapeName.c_str());
				}

				model->AddMesh(
					submeshName,
					part.MaterialID >= 0 ? Materials[part.MaterialID] : nullptr,
					SubmeshesRaw[submeshIndex].IndexCount,
					SubmeshesRaw[submeshIndex].StartIndex,
					SubmeshesRaw[submeshIndex].BaseVertex
					);
			}
		}

		u32 verticesNum = (u32)Shape.Positions.size();
		model->FatData->VerticesNum += verticesNum;
		VertexContainer.VerticesNum += verticesNum;
		//model->FatData->BoundingBox = GenerateAABB((float3*)shape.mesh.positions.data(), verticesNum);

		if (Shape.Texcoords.size()) {
			check(Shape.Texcoords.size() == verticesNum);
		}
		if (Shape.Normals.size()) {
			check(Shape.Normals.size() == verticesNum);
		}

		for (u32 v = 0; v < verticesNum; ++v) {
			FVertexPositionUV Vertex;
			Vertex.Position = Shape.Positions[v];
			if (Shape.Texcoords.size()) {
				Vertex.Texcoord0 = Shape.Texcoords[v];
			}
			else {
				Vertex.Texcoord0 = {};
			}
			VertexContainer.FeedStream(0, Vertex);

			FVertexNormal Vertex1;
			if (Shape.Normals.size()) {
				Vertex1.Normal = Shape.Normals[v];
			}
			else {
				Vertex1 = {};
			}
			VertexContainer.FeedStream(1, Vertex1);
		}
	}

	return model;
}

FModel* LoadModelFromOBJ(const wchar_t* filename) {

	std::vector<tinyobj::shape_t>		LoadedShapes;
	std::vector<tinyobj::material_t>	LoadedMaterials;
	std::string Err;

	auto filenameStr = ConvertToString(filename, wcslen(filename));

	bool isLoaded = tinyobj::LoadObj(LoadedShapes, LoadedMaterials, Err, filenameStr.c_str(), "Models/");

	eastl::vector<FMaterial*>	ModelReferencedMaterials;
	if (LoadedMaterials.size()) {
		ModelReferencedMaterials.resize(LoadedMaterials.size());
	}

	FModel* model = nullptr;

	VertexContainer.Streams[0].Stride = sizeof(FVertexPositionUV);
	VertexContainer.Streams[1].Stride = sizeof(FVertexNormal);

	if (isLoaded) {
		model = new FModel(filename);

		for (auto& shape : LoadedShapes) {
			eastl::wstring shapeName = ConvertToWString(shape.name.data(), shape.name.size());
			// todo: split by same shape.mesh.material_ids, produce separate bitmasks, index buffers

			// split by material_ids
			eastl::vector<FVertexContainer::FAppendResult> SubmeshesRaw;
			VertexContainer.AppendIndices(shape.mesh.indices.data(), shape.mesh.indices.size(), VertexContainer.VerticesNum, SubmeshesRaw);

			for (u32 submeshIndex = 0; submeshIndex < SubmeshesRaw.size(); submeshIndex++) {
				eastl::wstring submeshName;
				if (SubmeshesRaw.size() == 1) {
					submeshName.sprintf(L"%s (%u/%u)", shapeName.c_str(), submeshIndex, (u32)SubmeshesRaw.size());
				}
				else {
					submeshName.sprintf(L"%s", shapeName.c_str());
				}

				model->AddMesh(
					submeshName,
					shape.mesh.material_ids[0] >= 0 ? ModelReferencedMaterials[shape.mesh.material_ids[0]] : nullptr,
					SubmeshesRaw[submeshIndex].IndexCount, 
					SubmeshesRaw[submeshIndex].StartIndex,
					SubmeshesRaw[submeshIndex].BaseVertex
					);
			}

			u32 verticesNum = (u32)shape.mesh.positions.size() / 3;
			model->FatData->VerticesNum += verticesNum;
			VertexContainer.VerticesNum += verticesNum;
			model->FatData->BoundingBox = GenerateAABB((float3*)shape.mesh.positions.data(), verticesNum);

			if (shape.mesh.texcoords.size()) {
				check(shape.mesh.texcoords.size() / 2 == shape.mesh.positions.size() / 3);
			}
			if (shape.mesh.normals.size()) {
				check(shape.mesh.normals.size() / 3 == shape.mesh.positions.size() / 3);
			}

			for (u32 v = 0; v < verticesNum; ++v) {
				FVertexPositionUV Vertex;
				Vertex.Position = float3(shape.mesh.positions[v * 3], shape.mesh.positions[v * 3 + 1], shape.mesh.positions[v * 3 + 2]);
				if (shape.mesh.texcoords.size()) {
					Vertex.Texcoord0 = float2(shape.mesh.texcoords[v * 2], shape.mesh.texcoords[v * 2 + 1]);
				}
				else {
					Vertex.Texcoord0 = {};
				}
				VertexContainer.FeedStream(0, Vertex);

				FVertexNormal Vertex1;
				if (shape.mesh.normals.size()) {
					Vertex1.Normal = float3(shape.mesh.normals[v * 3], shape.mesh.normals[v * 3 + 1], shape.mesh.normals[v * 3 + 2]);
				}
				else {
					Vertex1 = {};
				}
				VertexContainer.FeedStream(1, Vertex1);
			}
		}
	}

	if (Err.size()) {
		PrintFormated(L"Error loading %s: %s", filename, ConvertToWString(Err.c_str(), Err.size()).c_str());
	}

	return model;
}