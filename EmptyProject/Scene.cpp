#include "Scene.h"
#include <DirectXMath.h>


void FScene::AddStaticMesh(const wchar_t* Name, FModelRef Model) {
	eastl::unique_ptr<FSceneObject> & URef = Objects[IdCounter++] = eastl::make_unique<FSceneStaticMesh>(IdCounter - 1, Name, Model);
	FSceneStaticMesh* Ptr = (FSceneStaticMesh*)URef.get();

	StaticMeshes.push_back(Ptr);
}

void FScene::Prepare(FGPUContext & Context) {
	for (auto & Object : Objects) {
		Object.second->Prepare(Context);
	}
}

void FSceneStaticMesh::Prepare(FGPUContext & Context) {
	if (!Model->ReadyForDrawing()) {
		Model->CopyDataToGPUBuffers(Context);
	}
}

void CreateWorldMatrixT(float3 Position, float Scale, float4x4 &OutMatrix) {
	using namespace DirectX;
	auto WorldTMatrix = XMMatrixTranspose(
		XMMatrixTranslation(Position.x, Position.y, Position.z)
		* XMMatrixScaling(Scale, Scale, Scale)
	);
	XMStoreFloat4x4((XMFLOAT4X4*)&OutMatrix, WorldTMatrix);
}