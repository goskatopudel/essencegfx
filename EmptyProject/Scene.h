#pragma once
#include "Essence.h"
#include "RenderModel.h"
#include "MathVector.h"

class FScene;
class FSceneRenderPass;
class FSceneRenderContext;
class FActorMaterial;
class FSceneRenderPass_MaterialInstance;

class FRenderPass {
public:
	ERenderPass Pass;
	virtual void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {}
	virtual void PreCacheMaterial(FSceneRenderContext & RenderSceneContext, FSceneRenderPass_MaterialInstanceRefParam Cachable) {}
	virtual void SetCompilationEnv(FShaderCompilationEnvironment & Env) {}
	virtual void QueryRenderTargets(FSceneRenderContext & SceneRenderContext, FRenderTargetsBundle & Bundle) {}
};

struct FRenderItem {
	u64 SortIndex;
	FSceneActor * Actor;
	FActorMaterial * Material;
	u32 SubmeshIndex;
};

struct FRenderTargetsBundle {
	struct {
		FRenderTargetView View;
		FGPUResource * Resource;
		inline const bool IsUsed() { return View.Format > 0; }
	} RenderTargets[FStateCache::MAX_RTVS];
	struct {
		FDepthStencilView View;
		FGPUResource * Resource;
		// todo: read/write depth
		inline const bool IsUsed() { return View.Format > 0; }
	} DepthStencil;
};

// encapsulates (scene, pass) tuple
class FSceneRenderPass {
public:
	FScene * Scene;
	FRenderPass * RenderPass;
	u32 CullBitIndex;

	eastl::vector<u32> Actors;
	// all items that need to be rendered (passed broad visibility test)
	// updated each frame
	eastl::vector<FRenderItem> RenderList;

	FRenderTargetsBundle RenderTargets = {};
	
	void QueryRenderTargets(FSceneRenderContext & SceneRenderContext);
	void Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream);

	FSceneRenderPass(FRenderPass * InRenderPass) : RenderPass(InRenderPass) {}
};
DECORATE_CLASS_REF(FSceneRenderPass);

class FActorMaterial {
public:
	FSceneRenderPass_MaterialInstance * Material;
	eastl::vector<u32> Submeshes;
	eastl::vector<u32> RootParams;
};

struct FSceneActor_RenderPass {
	FSceneRenderPass * SceneRenderPass;
	eastl::vector<FActorMaterial> MaterialsUsed;
	bool IsInAnyRenderList;
};

// this is tightly coupled with single scene instance
class FSceneActor {
public:
	const u32 Id;
	FScene * const Scene;

	float3 Position;
	FRenderModelRef RenderModel;

	u64 LastFrameUsed = -1;
	u32 LastCullMask = 0;

	// all passes that use the actor
	eastl::vector<FSceneActor_RenderPass> RenderPassInstances;

	FSceneActor(FScene * InScene, u32 InId) : 
		Id(InId),
		Scene(InScene)
		{
	}
};
DECORATE_CLASS_REF(FSceneActor);

class FRenderListItem {
public:
	FSceneActor * Actor;
	struct FSubmeshMaterial {
		u32 SubmeshIndex;
		FSceneRenderPass_MaterialInstanceRef PassMaterialInstance;
	};
	eastl::vector<FSubmeshMaterial> Submeshes;
};

class FRenderPass;

class FRenderPassList {
public:
	FSceneRenderPassRef SceneRenderPass;
	eastl::hash_map<u32, u32> IdLookup;
	eastl::vector<FRenderListItem> Items;

	FRenderPassList(FSceneRenderPassRefParam);

	void Attach(FSceneActor * Actor);
	void Detach(FSceneActor * Actor);
};

struct FSceneObjectRenderInfo {
public:
	u64 LastFrameUpdated : 63;
	u64 IsDirty : 1;
};

class FScene {
public:
	u64 CurrentFrameIndex; // used to identify outdated actors

	eastl::vector<FSceneActorRef> Actors;
	eastl::vector<FSceneObjectRenderInfo> ActorInfo;
	FRenderPassList DepthPrePassActors;
	FRenderPassList ForwardPassActors;

	u32 ActorId_Counter;
	eastl::vector<u32> ActorId_FreeList;
	u32 GenerateActorId();
	void ReleaseActorId(u32);

	FScene();
	FSceneActorRef SpawnActor(FRenderModelRefParam RenderModel, float3 Position);
	void RemoveActor(FSceneActorRefParam Actor);

	void AdvanceToNextFrame();
};

class FCamera;

#include "MathMatrix.h"

struct FSceneViewMatrices {
	float4x4 View;
	float4x4 InvView;
	float4x4 Projection;
	float4x4 InvProjection;
};

enum class EDepthProjection {
	Standard,
	InverseDepth
};

struct FSceneRenderConfig {
	EDepthProjection Projection = EDepthProjection::Standard;
	float ClearDepth = 1.f;
};

class FSceneRenderState {
public:
	Vec2u Resolution;
	FSceneViewMatrices ViewMatrices;
	u64 CurrentFrameIndex;

	FGPUResourceRef DepthBuffer;
	FGPUResourceRef ColorBuffer;

	D3D12_VIEWPORT GetViewport() const;
};

struct FFrustum {
	// ?
};

class FSceneRenderContext {
public:
	FScene * Scene;
	FCamera * Camera;
	FSceneRenderState State;
	FSceneRenderConfig Config;

	eastl::vector<FFrustum> Frusta;
	eastl::vector<FSceneRenderPass*> RenderPasses;

	FGPUResource * GetDepthBuffer();
	FGPUResource * GetColorBuffer();

	void AllocateRenderTargets();
	void SetupNextFrameRendering(FScene * InScene, Vec2u InResolution, FCamera * InCamera);
};


