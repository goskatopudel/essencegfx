#pragma once
#include "Essence.h"

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/array.h>
#include "MathVector.h"

#include "Commands.h"
#include <EASTL/unique_ptr.h>
#include "Resource.h"

class FGPUResource;

class FModel;
class FGPUContext;
class FMaterial;
class FModelFat;
class FMaterialFat;

struct FVertexPositionUV {
	float3 Position;
	float2 Texcoord0;
};

struct FVertexNormal {
	float3 Normal;
};

struct FMesh {
	u32		IndexCount;
	u32		StartIndex;
	i32		BaseVertex;
};

class FMeshFat {
public:
	eastl::wstring	Name;
	FMaterial*		Material;
};

struct FAABB {
	float3	VecMin;
	float3	VecMax;
};

class FModel {
public:
	eastl::vector<FMesh>					Meshes;
	u32										IndexStride : 4;
	eastl::unique_ptr<FModelFat>			FatData;

	FModel(const wchar_t * filename);
	void AddMesh(eastl::wstring name, FMaterial* material, u32 indexCount, u32 startIndex, i32 baseVertex);
};

class FModelFat {
public:
	eastl::wstring							Name;
	u32										IndicesNum = 0;
	u32										VerticesNum = 0;
	FAABB									BoundingBox;
	eastl::vector<FMeshFat>					FatMeshes;
};

class FMaterial {
public:
	eastl::unique_ptr<FMaterialFat>			FatData;
};

class FMaterialFat {
public:
	eastl::wstring	Name;
	eastl::wstring	BaseColorTextureFilename;
	eastl::wstring	MetallicTextureFilename;
	eastl::wstring	RoughnessTextureFilename;
	eastl::wstring	NormalMapTextureFilename;
	eastl::wstring	AlphaMaskTextureFilename;
	FOwnedResource	BaseColorTexture;
	FOwnedResource	MetallicTexture;
	FOwnedResource	RoughnessTexture;
	FOwnedResource	NormalMapTexture;
	FOwnedResource	AlphaMaskTexture;
	float3			BaseColor;
	float			Roughness;
	float			Metallic;
};

FModel* LoadModelFromOBJ(const wchar_t* filename);
void	ConvertObjToBinary(const wchar_t* inFilename, const wchar_t* outFilename);
FModel*	LoadModelFromBinary(const wchar_t* filename);
void	LoadModelTextures(FGPUContext & Context, FModel* Model);
void	UpdateGeometryBuffers(FGPUContext & Context);

FBufferLocation GetIB();
FBufferLocation GetVB(u32 stream);