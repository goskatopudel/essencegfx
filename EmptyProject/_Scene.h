//#pragma once
//#include "MathVector.h"
//#include "MathMatrix.h"
//#include <EASTL\string.h>
//#include <EASTL\shared_ptr.h>
//#include "Model.h"
//#include <DirectXMath.h>
//
//class FRenderObject {
//	u64 ID;
//	FModelRef Model;
//	float3 Position;
//	float Scale;
//	float4 Rotation;
//};
//
//typedef eastl::shared_ptr<FRenderObject> FRenderObjectRef;
//typedef eastl::shared_ptr<FRenderObject> & FRenderObjectRefParam;
//
//class FCachedRenderMaterialState {
//public:
//	// state
//	FVertexInterfaceRef VertexInterface;
//	FRenderMaterialRef Material;
//	FRenderPassRef Pass;
//	// cached
//	FShaderStateRef Shaders;
//	FPipelineState * PipelineState;
//	//FCachedRootParams * RootParams;
//};
//
//typedef eastl::shared_ptr<FCachedRenderMaterialState> FCachedRenderMaterialStateRef;
//typedef eastl::shared_ptr<FCachedRenderMaterialState> & FCachedRenderMaterialStateRefParam;
//
//class FRenderObjectRenderPass {
//public:
//	FRenderObjectRef RenderObject;
//	eastl::vector<FCachedRenderMaterialStateRef> PerMeshCachedMaterial;
//};
//
//class FSceneRenderPassList {
//public:
//	FRenderPassRef Pass;
//	eastl::vector<FRenderObjectRenderPass> Objects;
//};
//
////FRenderObjectRef CreateRenderObject();
//
//class FRenderScene {
//public:
//	FSceneRenderPassList BasePassRenderables;
//
//	//void AddBasePassRenderable();
//};
//
////#include "RenderMaterial.h"
//
// struct FRenderObjectProxy {
//	FVertexInterface * VertexInterface;
//	FBufferLocation VB;
//	FBufferLocation IB;
//	float3 Position;
//	float Scale;
//	float4 Rotation;
//	//Custom
//};
//
//struct FRenderObjectMaterialProxy {
//	FMeshDraw DrawArgs;
//	FCachedRenderMaterialState * CachedMaterial;
//	//MaterialCallback
//	//Custom
//};
//
//struct FRenderObjectProxyIndirection {
//	u64 SortKey;
//	u16 Index0;
//	u16 Index1;
//};
//
//class FRenderList {
//public:
//	eastl::vector<FRenderObjectProxyIndirection> Indirection;
//	eastl::vector<FRenderObjectProxy> Proxies_0;
//	eastl::vector<FRenderObjectMaterialProxy> Proxies_1;
//};