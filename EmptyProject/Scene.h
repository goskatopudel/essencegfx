#pragma once
#include "MathVector.h"
#include "MathMatrix.h"
#include <EASTL\string.h>
#include <EASTL\shared_ptr.h>
#include "Model.h"
#include <DirectXMath.h>

class FGPUContext;

class FSceneObject {
public:
	virtual ~FSceneObject() {}
	virtual void Prepare(FGPUContext & Context) {}
};

class FSceneStaticMesh;

class FScene {
public:
	u32 IdCounter = 0;
	eastl::hash_map<u32, eastl::unique_ptr<FSceneObject>> Objects;
	eastl::vector<FSceneStaticMesh*> StaticMeshes;

	void AddStaticMesh(const wchar_t* Name, FModelRef Model);

	void Prepare(FGPUContext & Context);
};

class FSceneStaticMesh : public FSceneObject {
public:
	u32 Id;
	eastl::wstring Name;
	FModelRef Model;
	float3 Position = float3(0, 0, 0);

	FSceneStaticMesh() = default;
	FSceneStaticMesh(u32 id, eastl::wstring && name, FModelRef model) : Id(id), Name(name), Model(model) {

	}

	void Prepare(FGPUContext & Context) override;
};

void CreateWorldMatrixT(float3 Position, float Scale, float4x4 &OutMatrix);
DirectX::XMMATRIX GetWorldMatrix(float3 Position, float Scale);
void StoreTransposed(DirectX::CXMMATRIX Matrix, float4x4 * Out);
DirectX::XMMATRIX Load(float4x4 const & Matrix);