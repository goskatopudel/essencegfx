#include "Scene.h"
#include "Camera.h"
#include "VideoMemory.h"

#include "ForwardPass.h"

template<class T>
FSceneRenderPassRef CreateSceneRenderPass() {
	FSceneRenderPassRef Ref = eastl::make_shared<FSceneRenderPass>(&GetInstance<T>());
	return Ref;
}

FRenderPassList::FRenderPassList(FSceneRenderPassRefParam InRenderPass) : 
	SceneRenderPass(InRenderPass)
{
}

FScene::FScene() : 
	DepthPrePassActors(CreateSceneRenderPass<FDepthPrePass>()),
	ForwardPassActors(CreateSceneRenderPass<FForwardPass>())
{
	DepthPrePassActors.SceneRenderPass->CullBitIndex = 0;
	ForwardPassActors.SceneRenderPass->CullBitIndex = 0;
}

// frusta:
// camera frustum (5 vertices)
// shadowmap frustum (8 vertices)
// frustum type: Scene, Custom_Volume

u32 FScene::GenerateActorId() {
	if (ActorId_FreeList.size()) {
		u32 Id = ActorId_FreeList.back();
		ActorId_FreeList.pop_back();
		return Id;
	}
	return ++ActorId_Counter;
}

void FScene::ReleaseActorId(u32 Id) {
	ActorId_FreeList.push_back(Id);
}

void FScene::AdvanceToNextFrame() {
	++CurrentFrameIndex;
}

FSceneActorRef FScene::SpawnActor(FRenderModelRefParam RenderModel, float3 Position) {
	FSceneActorRef Actor = eastl::make_shared<FSceneActor>(this, GenerateActorId());
	Actor->Position = Position;
	Actor->RenderModel = RenderModel;

	Actors.push_back(Actor);
	ActorInfo.push_back();
	ActorInfo.back().IsDirty = 1;
	ActorInfo.back().LastFrameUpdated = -1;

	DepthPrePassActors.Attach(Actor);
	ForwardPassActors.Attach(Actor);

	return Actor;
}

void FScene::RemoveActor(FSceneActorRefParam Actor)
{
	// todo
	// remove id
	// detach from passes
}

void FSceneRenderPass::QueryRenderTargets(FSceneRenderContext & SceneRenderContext) {
	RenderPass->QueryRenderTargets(SceneRenderContext, RenderTargets);
}

D3D12_VIEWPORT FSceneRenderState::GetViewport() const {
	D3D12_VIEWPORT Out;
	Out.MinDepth = 0.f;
	Out.MaxDepth = 1.f;
	Out.TopLeftX = 0.f;
	Out.TopLeftY = 0.f;
	Out.Width = float(Resolution.x);
	Out.Height = float(Resolution.y);
	return Out;
}

void FSceneRenderPass::Begin(FSceneRenderContext & RenderSceneContext, FCommandsStream & CmdStream) {
	for (u64 Index = 0; Index < _countof(RenderTargets.RenderTargets); ++Index) {
		if(RenderTargets.RenderTargets[Index].IsUsed()) {
			CmdStream.SetRenderTarget(RenderTargets.RenderTargets[Index].View, (u8)Index);
			CmdStream.SetAccess(RenderTargets.RenderTargets[Index].Resource, EAccessType::WRITE_RT);
		}
	}
	if (RenderTargets.DepthStencil.IsUsed()) {
		CmdStream.SetDepthStencil(RenderTargets.DepthStencil.View);
		CmdStream.SetAccess(RenderTargets.DepthStencil.Resource, EAccessType::WRITE_DEPTH);
	}

	RenderPass->Begin(RenderSceneContext, CmdStream);
	CmdStream.ClearDSV(RenderTargets.DepthStencil.View.DSV, RenderSceneContext.Config.ClearDepth);
	CmdStream.SetViewport(RenderSceneContext.State.GetViewport());
}

void FRenderPassList::Attach(FSceneActor * Actor) {
	check(IdLookup.count(Actor->Id) == 0);

	bool bUseWithPass = false;
	{
		u32 SubmeshIndex = 0;
		for (auto & Submesh : Actor->RenderModel->Submeshes) {
			if (Actor->RenderModel->Submeshes[SubmeshIndex].Material->IsRenderedWithPass(SceneRenderPass->RenderPass)) {
				bUseWithPass = true;
				break;
			}
		}
	}
	if (!bUseWithPass) {
		return;
	}

	IdLookup[Actor->Id] = (u32)Items.size();

	FRenderListItem & Item = Items.push_back();
	Item.Actor = Actor;

	FSceneActor_RenderPass SceneActor_RenderPass = {};
	SceneActor_RenderPass.SceneRenderPass = SceneRenderPass.get();
	SceneActor_RenderPass.IsInAnyRenderList = false;

	eastl::hash_map<FSceneRenderPass_MaterialInstance *, u32> PassMaterialInstanceLookup;

	u32 SubmeshIndex = 0;
	for (auto & Submesh : Actor->RenderModel->Submeshes) {
		if (Actor->RenderModel->Submeshes[SubmeshIndex].Material->IsRenderedWithPass(SceneRenderPass->RenderPass)) {
			auto & SubItem = Item.Submeshes.push_back();
			SubItem.SubmeshIndex = SubmeshIndex;
			SubItem.PassMaterialInstance = GetSceneRenderPass_MaterialInstance(SceneRenderPass.get(), Actor->RenderModel->Submeshes[SubmeshIndex].Material);

			auto MaterialIter = PassMaterialInstanceLookup.find(SubItem.PassMaterialInstance.get());
			if (MaterialIter == PassMaterialInstanceLookup.end()) {
				u32 MaterialInnerIndex = (u32)PassMaterialInstanceLookup.size();
				PassMaterialInstanceLookup[SubItem.PassMaterialInstance.get()] = MaterialInnerIndex;

				FActorMaterial ActorMaterial = {};
				ActorMaterial.Material = SubItem.PassMaterialInstance.get();

				SceneActor_RenderPass.MaterialsUsed.push_back(ActorMaterial);
				SceneActor_RenderPass.MaterialsUsed[MaterialInnerIndex].Submeshes.push_back(SubmeshIndex);
			}
			else {
				SceneActor_RenderPass.MaterialsUsed[MaterialIter->second].Submeshes.push_back(SubmeshIndex);
			}
		}

		++SubmeshIndex;
	}

	Actor->RenderPassInstances.push_back(SceneActor_RenderPass);
}

void FRenderPassList::Detach(FSceneActor * Actor) {
	check(IdLookup.count(Actor->Id) == 1);
	u32 Index = IdLookup[Actor->Id];
	RemoveSwap(Items, Index);
	IdLookup.erase(Actor->Id);
}

//void FRenderSceneContext::Render(FCommandsStream & CmdStream) {
//
//	CmdStream.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH);
//	CmdStream.SetAccess(ColorBuffer, EAccessType::WRITE_RT);
//
//	CmdStream.ClearDSV(DepthBuffer->GetDSV().DSV, DepthFar);
//	CmdStream.ClearRTV(ColorBuffer->GetRTV().RTV, float4(0));
//
//	RenderPass(CmdStream, Scene->ForwardPassActors);
//}

class FRenderObjectMaterial {
public:
};

class FMaterialCache {
public:

};

// things to cache:
// PSO: material, pass
// Tables: material, pass, object

enum class EShaderParamType {
	CBV, SRV, UAV, TABLE
};

struct FShaderParamItem {
	EShaderParamType Type;
	FUAVParam Param;
};

//class FRenderPassMaterialInstance {
//public:
//	// PSO
//	// params to set
//};
//DECORATE_CLASS_REF(FRenderPassMaterialInstance);

class FRenderPassDrawList_Material {
public:
	struct FListItem {
		FSceneActorRef Actor;
		eastl::vector<u32> SubmeshIndices;
	};
	FSceneRenderPass_MaterialInstanceRef Material;
	eastl::vector<FListItem> Items;
};

//class FRenderPassDrawList {
//public:
//	FRenderPass * const RenderPass;
//	eastl::vector<FRenderPassDrawList_Material> Materials;
//
//	FRenderPassDrawList(FRenderPass *);
//
//	void Attach(FSceneActorRefParam Actor);
//	void Detach(FSceneActorRefParam Actor);
//};
//
//FRenderPassDrawList::FRenderPassDrawList(FRenderPass * InRenderPass) :
//	RenderPass(InRenderPass)
//{
//
//}
//
//void FRenderPassDrawList::Attach(FSceneActorRefParam Actor) {
//
//}
//
//void FRenderPassDrawList::Detach(FSceneActorRefParam Actor) {
//
//}

struct FRenderPassDrawList {
	FRenderPass* RenderPass;
	struct FRenderItem {
		FSceneActor * Actor;
		FSceneRenderPass_MaterialInstance * MatInst;
		u32 SubmeshIndex;
	};
	eastl::vector<FRenderItem> Items;
};

bool IsBitSet(u32 Val, u32 Index) {
	return ((Val >> Index) & 0x1) > 0;
}

#include <EASTL\sort.h>

template<typename T>
void RemoveFromVector(eastl::vector<T> Vec, eastl::vector<u32> IndicesToRemove, bool Sorted = false) {
	if (IndicesToRemove.size() == 0) {
		return;
	}
	if (!Sorted) {
		eastl::stable_sort(IndicesToRemove.begin(), IndicesToRemove.end());
	}
	
	u64 TableIndex = 1;
	u64 Removed = 0;
	const u64 Last = IndicesToRemove[IndicesToRemove.size() - 1];
	for (u64 Index = IndicesToRemove[0] + 1; Index <= Last; ++Index) {
		if (Last - Index == IndicesToRemove.size() - Removed) {
			break;
		}

		while(IndicesToRemove[TableIndex] == Index) {
			++Index;
			++TableIndex;
		}
			
		eastl::swap(Vec[Index], Vec[IndicesToRemove[0] + Removed]);
	}

	u64 RangeStart = Last + 1 - IndicesToRemove.size();
	for (u64 Index = Last + 1; Index < Vec.size(); ++Index) {
		eastl::swap(Vec[RangeStart], Vec[Index]);
		++RangeStart;
	}

	Vec.resize(Vec.size() - IndicesToRemove.size());
}


struct FCulledActor {
	u32 Index;
	u32 CullMask;
};

void CullScene(FSceneRenderContext * SceneContext, eastl::vector<FCulledActor> & OutVisibleActors) {
#if 1
	u32 AllFrustaCullMask = 0;

	for (FSceneRenderPass* SceneRenderPass : SceneContext->RenderPasses) {
		AllFrustaCullMask |= (1 << SceneRenderPass->CullBitIndex);
	}

	u32 Index = 0;
	for (FSceneActor * SceneActor : SceneContext->Scene->Actors) {
		FCulledActor CulledActor = {};
		CulledActor.Index = Index;
		CulledActor.CullMask = AllFrustaCullMask;
		OutVisibleActors.push_back(CulledActor);
		++Index;
	}
#endif
}

void ProcessScene(FSceneRenderContext * SceneContext) {
	const FScene * Scene = SceneContext->Scene;

	const eastl::vector<FSceneRenderPass*> & RenderPasses = SceneContext->RenderPasses;
	eastl::vector<FCulledActor> VisibleActors;

	CullScene(SceneContext, VisibleActors);
	
	// filter dirty actors
	eastl::vector<FCulledActor> UpdateActors;
	for (FCulledActor CulledActor : VisibleActors) {
		if (Scene->ActorInfo[CulledActor.Index].IsDirty) {
			UpdateActors.push_back(CulledActor);
		}
	}

	struct FActorMaterialUpdate {
		FSceneActor * Actor;
		FSceneRenderPass_MaterialInstance * Material;

		FActorMaterialUpdate() = default;
	};
	eastl::vector<FActorMaterialUpdate> ActorMaterialUpdateList;
	eastl::vector<FActorMaterial*> ActorPassUpdateList;

	const u64 CurrentFrameIndex = Scene->CurrentFrameIndex;

	// prepare list of dirty actor-materials that will be used in frame rendering
	// updates scene-pass lists of render items: (actor, submesh) tuples
	eastl::hash_set<FSceneRenderPass_MaterialInstance*> UpdateMaterials;
	for (FCulledActor CulledActor : UpdateActors) {
		FSceneActor * Actor = Scene->Actors[CulledActor.Index];
		Actor->LastFrameUsed = CurrentFrameIndex;
		Actor->LastCullMask = CulledActor.CullMask;
		for (FSceneActor_RenderPass & ActorPass : Actor->RenderPassInstances) {
			if (IsBitSet(CulledActor.CullMask, ActorPass.SceneRenderPass->CullBitIndex)) {
				// update render lists
				// if actor is used in a pass, prepare subcalls for submeshes
				if (!ActorPass.IsInAnyRenderList) {
					for (FActorMaterial & ActorMaterial : ActorPass.MaterialsUsed) {
						for (u32 SubmeshIndex : ActorMaterial.Submeshes) {
							FRenderItem RenderItem = {};
							RenderItem.Actor = Actor;
							RenderItem.Material = &ActorMaterial;
							RenderItem.SubmeshIndex = SubmeshIndex;
							RenderItem.SortIndex = 0;
							ActorPass.SceneRenderPass->RenderList.push_back(RenderItem);
						}
					}
				}
				// prepare list of specific actor-material tuples to update
				for (FActorMaterial & ActorMaterial : ActorPass.MaterialsUsed) {
					UpdateMaterials.insert(ActorMaterial.Material);
					FActorMaterialUpdate Update = {};
					Update.Actor = Actor;
					Update.Material = ActorMaterial.Material;
					ActorMaterialUpdateList.push_back(Update);
				}
			}
		}
	}

	// process list of materials that need update
	for (FSceneRenderPass_MaterialInstance * MatInst : UpdateMaterials) {
		MatInst->Prepare();
		// MatInst->UpdateMaterialDescriptors();
	}

	for (FActorMaterialUpdate ActorMatUpdate : ActorMaterialUpdateList) {
		//ActorMatUpdate.Material->UpdateActorDescriptors(ActorMatUpdate.Actor);
	}
	
	// clean render lists of elements that aren't visible this frame
	{
		for (FSceneRenderPass* SceneRenderPass : RenderPasses) {
			eastl::vector<u32> RemoveIndices;
			u32 Index = 0;
			for (FRenderItem RenderItem : SceneRenderPass->RenderList) {
				bool bRemove = RenderItem.Actor->LastFrameUsed != CurrentFrameIndex || !IsBitSet(RenderItem.Actor->LastCullMask, SceneRenderPass->CullBitIndex);
				if (bRemove) {
					RemoveIndices.push_back(Index);
				}
				Index++;
			}
			RemoveFromVector(SceneRenderPass->RenderList, RemoveIndices);
		}
	}

	
	for (FSceneRenderPass * Pass : RenderPasses) {
#if 1
		eastl::stable_sort(Pass->RenderList.begin(), Pass->RenderList.end(), [](FRenderItem A, FRenderItem B) {
			return A.SortIndex < B.SortIndex;
		});
#endif
	}
}

FGPUResource * FSceneRenderContext::GetDepthBuffer() {
	return State.DepthBuffer;
}

FGPUResource * FSceneRenderContext::GetColorBuffer() {
	return State.ColorBuffer;
}

void FSceneRenderContext::AllocateRenderTargets() {

	if (!State.DepthBuffer.IsValid() || !State.ColorBuffer.IsValid()) {
		State.DepthBuffer = GetTexturesAllocator()->CreateTexture(State.Resolution.x, State.Resolution.y, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"SceneDepth", DXGI_FORMAT_UNKNOWN, float4(0), Config.ClearDepth);
		State.ColorBuffer = GetTexturesAllocator()->CreateTexture(State.Resolution.x, State.Resolution.y, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, TextureFlags::ALLOW_RENDER_TARGET, L"SceneColor");
	}
}

void FSceneRenderContext::SetupNextFrameRendering(FScene * InScene, Vec2u InResolution, FCamera * InCamera) {
	Scene = InScene;
	Camera = InCamera;
	State.Resolution = InResolution;

	AllocateRenderTargets();
	
	for (FSceneRenderPass* SceneRenderPass : RenderPasses) {
		SceneRenderPass->QueryRenderTargets(*this);

		Frusta.resize(eastl::max(Frusta.size(), (u64)SceneRenderPass->CullBitIndex + 1));
	}
}

FGPUResourceRef RenderSceneToTexture(FCommandsStream & CmdStream, FSceneRenderContext * SceneRenderContext) {
	ProcessScene(SceneRenderContext);

	for (FSceneRenderPass * Pass : SceneRenderContext->RenderPasses) {
		// setup pass targets
		// Pass->Setup(CmdStream);
		//  = SetRenderTargets

		// SET VIEWPORT
		// SET ?

		Pass->Begin(*SceneRenderContext, CmdStream);

		FSceneRenderPass_MaterialInstance * PrevMaterial = nullptr;

		for (FRenderItem Item : Pass->RenderList) {
			Item.Actor;
			Item.Material;
			Item.SubmeshIndex;

			//Item.Actor->RenderModel->VertexBuffer->Get
			/*FBufferLocation VB;
			VB.Address = VertexBuffer->GetGPUAddress();
			VB.Size = vtxBytesize;
			VB.Stride = sizeof(ImDrawVert);
			CmdStream.SetVB(, 0);*/
			// SET VB
			// SET IB


			// setup material (pso, root params)
			FSceneRenderPass_MaterialInstance * Material = Item.Material->Material;

			if (Material != PrevMaterial) {
				// todo: does change root as return! =
				//CmdStream.SetPipelineState(Material->PSO);
				PrevMaterial = Material;
			}

			for (u32 SubmeshIndex : Item.Material->Submeshes) {
				//draw call params
				//Item.Actor->RenderModel->Submeshes[SubmeshIndex];
				auto A = Item.Actor->RenderModel->Submeshes[SubmeshIndex].IndicesNum;
				auto B = Item.Actor->RenderModel->Submeshes[SubmeshIndex].StartIndex;
				auto C = Item.Actor->RenderModel->Submeshes[SubmeshIndex].BaseVertex;
				//CmdStream.DrawIndexed(A, B, C);
			}
		}
	}

	return SceneRenderContext->State.ColorBuffer;
}

//void FRenderSceneContext::RenderPass(FCommandsStream & CmdStream, FRenderPassList & RenderList) {
//	// find objects visible in frustum
//
//	// cache visible materials, prepare flat lists
//	for (auto & Item : RenderList.Items) {
//		for (auto & Submesh : Item.Submeshes) {
//			RenderList.RenderPass->PreCacheMaterial(*this, Submesh.MaterialInstancePass);
//
//			//Submesh.SubmeshIndex;
//			//Item.Actor->RenderModel->Submeshes;
//		}
//	}
//
//	struct FRenderItem_Object {
//		FBufferLocation VB;
//		FBufferLocation IB;
//	};
//
//	struct FDrawArgs {
//		u32 IndexCount;
//		u32 StartIndex;
//		i32 BaseVertex;
//	};
//
//	struct FRenderItem_ObjectMaterial {
//		FRenderPassMaterialInstance * Material;
//		FDrawArgs DrawArgs;
//	};
//	
//	struct FRenderItem {
//		u64 SortKey;
//		u16 Index0;
//		u16 Index1;
//	};
//
//	eastl::vector<FRenderItem> RenderItems;
//	eastl::vector<FRenderItem_Object> RenderItems_Objects;
//	eastl::vector<FRenderItem_ObjectMaterial> RenderItems_ObjectMaterials;
//		
//
//	FRenderPass * Pass = (FRenderPass*)RenderList.RenderPass;
//	Pass->Begin(*this, CmdStream);
//	
//	const u64 RenderItemsNum = RenderItems.size();
//	for (u64 Index = 0; RenderItemsNum; Index++) {
//		FRenderItem Item = RenderItems[Index];
//		FRenderItem_Object Object = RenderItems_Objects[Item.Index0];
//		FRenderItem_ObjectMaterial ObjectMaterial = RenderItems_ObjectMaterials[Item.Index1];
//
//		CmdStream.SetVB(Object.VB, 0);
//		CmdStream.SetIB(Object.IB);
//		CmdStream.SetPipelineState(ObjectMaterial.Material->PSO);
//		//CmdStream.SetConstantBuffer();
//		//CmdStream.SetTexture();
//		ObjectMaterial.Material->SetShaderParams(&CmdStream);
//		CmdStream.DrawIndexed(ObjectMaterial.DrawArgs.IndexCount, ObjectMaterial.DrawArgs.StartIndex, ObjectMaterial.DrawArgs.BaseVertex);
//	}
//}

//void FRenderSceneContext::AllocateResources() {
//	if (!DepthBuffer.IsValid() || !ColorBuffer.IsValid()) {
//		DepthBuffer = GetTexturesAllocator()->CreateTexture(BufferWidth, BufferHeight, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"SceneDepth", DXGI_FORMAT_UNKNOWN, float4(0), DepthFar);
//		ColorBuffer = GetTexturesAllocator()->CreateTexture(BufferWidth, BufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, TextureFlags::ALLOW_RENDER_TARGET, L"SceneColor");
//	}
//}
