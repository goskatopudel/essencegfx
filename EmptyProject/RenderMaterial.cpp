#include "RenderMaterial.h"
#include "Pipeline.h"
#include "Scene.h"

class FTestMaterialShaderState : public FShaderState {
public:
	FSRVParam AlbedoTexture;
	FCBVParam FrameCBV;
	FCBVParam ObjectCBV;

	FTestMaterialShaderState() {}

	void InitParams() override final {
		AlbedoTexture = RootLayout->CreateSRVParam(this, "AlbedoTexture");
		FrameCBV = RootLayout->CreateCBVParam(this, "Frame");
		ObjectCBV = RootLayout->CreateCBVParam(this, "Object");
	}
};

//class FRootSignature {
//public:
//	eastl::hash_map<u32, u32> RootTables;
//};

//FRootParamsBundle FRenderMaterialPass::UpdateDescriptors(FRootParamsStream & RootParamsStream, FSceneActor * Actor) {
//	return RenderMaterial->UpdateDescriptors(RootParamsStream, Actor, this);
//}

// root table params:
// per-object (constants, textures)-table
// per-material (constants, textures)-table
// per-frame (constants, textures, uavs?)-table
// bindless (textures)-table

#include "MathMatrix.h"
#include "Shaders\BasicMaterial_Data.h"

struct FRootParamsTransaction {
public:
	void SetSRV(FSRVParam Param) {}
	void SetCBV(FCBVParam Param) {}
	void SetUAV(FUAVParam Param) {}
};

class FRootParamsManager {
public:
//	FRootParamsBundle Commit(FRootParamsStream & RootParamsStream, FRootParamsTransaction & Transaction) { return{}; }
};

eastl::wstring ConvertToResourceString(eastl::wstring const & String) {
	eastl::wstring Result = String;
	for (u64 Index = 0; Index < Result.length(); ++Index) {
		Result[Index] = eastl::CharToLower(Result[Index]);
	}
	return Result;
}

#include "Hash.h"

// Type: constant buffer, string to identify
// Type: shader resource, string to identify
// Type: uav, string to identify

class FRenderMaterialManager {
public:
	eastl::hash_map<u64, FRenderMaterialRef> RenderMaterialsMap;

	FRenderMaterialRef GetRenderMaterial(eastl::wstring const & ShaderName) {
		eastl::wstring wShaderNameFinal = ConvertToResourceString(ShaderName);
		eastl::string ShaderNameFinal = ConvertToString(wShaderNameFinal);

		u64 Key = MurmurHash2_64(ShaderNameFinal.data(), sizeof(ShaderNameFinal[0]) * ShaderNameFinal.length(), 0);

		auto RenderMaterialIter = RenderMaterialsMap.find(Key);
		if (RenderMaterialIter != RenderMaterialsMap.end()) {
			return RenderMaterialIter->second;
		}

		FRenderMaterialRef Ref = eastl::make_shared<FRenderMaterial>(ShaderNameFinal);
		RenderMaterialsMap[Key] = Ref;
		return Ref;
	}
};

FRenderMaterialManager GRenderMaterialManager;

FRenderMaterial::FRenderMaterial(eastl::string InShaderName) :
	ShaderName(InShaderName)
{
}

FRenderMaterialInstance::FRenderMaterialInstance(FRenderMaterialRefParam InRenderMaterial) :
	RenderMaterial(InRenderMaterial)
{
}

void FRenderMaterialInstance::SetTexture(FMaterialShaderParam Param, FTextureAssetRefParam Texture) {

}

FRenderPass_MaterialInstance::FRenderPass_MaterialInstance(FRenderPass * InRenderPass, FRenderMaterialInstanceRefParam InRenderMaterialInstance) :
	RenderPass(InRenderPass),
	RenderMaterialInstance(InRenderMaterialInstance)
{
}

FSceneRenderPass_MaterialInstance::FSceneRenderPass_MaterialInstance(FSceneRenderPass * InSceneRenderPass, FRenderMaterialInstanceRefParam InRenderMaterialInstance) :
	SceneRenderPass(InSceneRenderPass)
{
	// todo: caching, owned by renderpass?
	RenderPass_MaterialInstance = eastl::make_shared<FRenderPass_MaterialInstance>(InSceneRenderPass->RenderPass, InRenderMaterialInstance);
}

template<typename T>
bool HasFlag(T A, T B) {
	return u32(A & B) > 0;
}

void FRenderPass_MaterialInstance::Prepare() {
	if (!ShaderState.get()) {
		FShaderCompilationEnvironment CompilationEnv;

		RenderPass->SetCompilationEnv(CompilationEnv);

		FShaderRef VertexShader;
		FShaderRef PixelShader;
		if (HasFlag(PipelineShaders, EPipelineShadersUsage::Vertex)) {
			VertexShader = GetGlobalShader(
				RenderMaterialInstance->RenderMaterial->ShaderName,
				"VertexMain",
				"vs_5_1",
				CompilationEnv);
		}
		if (HasFlag(PipelineShaders, EPipelineShadersUsage::Pixel)) {
			PixelShader = GetGlobalShader(
				RenderMaterialInstance->RenderMaterial->ShaderName,
				"PixelMain",
				"ps_5_1",
				CompilationEnv);
		}
		check((u32)PipelineShaders < 4); // not implemented other shaders

		//ShaderState = GetShaderState(VertexShader, PixelShader, PassMaterialRootSignature);
	}
}

void FSceneRenderPass_MaterialInstance::Prepare() {
	RenderPass_MaterialInstance->Prepare();

	// prepare PSO
	if (PSO != nullptr && !PSO->IsOutdated()) {
		return;
	}

	
	
	// FPipelineStateDesc PipelineStateDesc;
	// PipelineStateDesc.SetRenderTargets(FRenderPass->Targets);// SceneRenderPass
	// PipelineStateDesc.SetShaders(ShaderState);

	// GetPipelineState
}

FRenderMaterialInstanceRef GetBasicMaterialInstance(FBasicMaterialDesc const& Desc) {
	auto RenderMaterial = GRenderMaterialManager.GetRenderMaterial(L"shaders/BasicMaterial.hlsl");
	FRenderMaterialInstanceRef RenderMatInst = eastl::make_shared<FRenderMaterialInstance>(RenderMaterial);
	//RenderMatInst->SetTexture(FMaterialShaderParam(EMaterialShaderParam::ShaderResource, "AlbedoTexture"), );
	RenderMatInst->IsTransparent = Desc.bTransparent;
	RenderMatInst->IsAlphaMasked = 0;
	return RenderMatInst;
}

FSceneRenderPass_MaterialInstanceRef GetSceneRenderPass_MaterialInstance(FSceneRenderPass * SceneRenderPass, FRenderMaterialInstanceRefParam RenderMaterialInstance) {
	return eastl::make_shared<FSceneRenderPass_MaterialInstance>(SceneRenderPass, RenderMaterialInstance);
}

//
//eastl::unique_ptr<FRootSignature>	MaterialsRoot;
//
//void CreateMaterialRoot() {
//	MaterialsRoot = eastl::make_unique<FRootSignature>();
//	MaterialsRoot->InitDefault(D3D12_SHADER_VISIBILITY_ALL, STAGE_ALL);
//	// b1
//	MaterialsRoot->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_ALL);
//	MaterialsRoot->AddTableCBVRange(2, 1, 0);
//	// t0-4 b2
//	MaterialsRoot->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_PIXEL);
//	MaterialsRoot->AddTableSRVRange(0, 5, 0);
//	MaterialsRoot->AddTableCBVRange(1, 1, 0);
//	// b0
//	MaterialsRoot->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_ALL);
//	MaterialsRoot->AddTableCBVRange(0, 1, 0);
//	MaterialsRoot->SerializeAndCreate();
//}
//
//#include "Shader.h"
//
//FVertexInterfaceRef GetVertexInterface(eastl::string shaderInclude, EVertexInterfaceFlags flags, std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements) {
//	FVertexInterfaceRef Output = eastl::make_shared<FVertexInterface>();
//	Output->ShaderInclude = std::move(shaderInclude);
//	Output->Layout = GetInputLayout(elements);
//	Output->Flags = flags;
//	return Output;
//}
//
//FVertexInterfaceRef GetRichVertexInterface() {
//	static FVertexInterfaceRef RichVertex;
//	if (RichVertex.get() == nullptr) {
//		RichVertex = GetVertexInterface(eastl::string("Rich"), EVertexInterfaceFlags::Static, 
//		{ 
//			CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
//			CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
//			CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
//			CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
//			CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
//			CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
//			CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0) 
//		});
//	}
//	return RichVertex;
//}
//
//FShader * GetMaterialVertexShader(const char * VertexFactory, const char * Material, const char * Rendering) {
//	eastl::string Filename = "shaders/RenderMaterial_" + eastl::string(Rendering) + ".hlsl";
//	return GetGlobalShader(Filename, "VertexMain", "vs_5_1", {}, 0).get();
//}
//
//FShader * GetMaterialPixelShader(const char * VertexFactory, const char * Material, const char * Rendering) {
//	eastl::string Filename = "shaders/RenderMaterial_" + eastl::string(Rendering) + ".hlsl";
//	return GetGlobalShader(Filename, "PixelMain", "ps_5_1", {}, 0).get();
//}
//
//FRenderMaterialRef CreateRenderMaterial(eastl::string shaderInclude) {
//	FRenderMaterialRef Output = eastl::make_shared<FRenderMaterial>();
//	Output->ShaderInclude = std::move(shaderInclude);
//	return Output;
//}
//
//FRenderMaterialRef GetStandardMaterial() {
//	static FRenderMaterialRef StandardMaterial;
//
//	if (StandardMaterial.get() == nullptr) {
//		StandardMaterial = CreateRenderMaterial("Standard");
//	}
//
//	return StandardMaterial;
//}
//
//FRenderPassRef CreateRenderPass(eastl::string shaderInclude) {
//	FRenderPassRef Output = eastl::make_shared<FRenderPass>();
//	Output->ShaderInclude = std::move(shaderInclude);
//	return Output;
//}
//
//FRenderMaterialInstanceRef CreateRenderMaterialInstance(FRenderMaterialRef RenderMaterial, ERenderMaterialInstanceFlags Flags) {
//	FRenderMaterialInstanceRef Output = eastl::make_shared<FRenderMaterialInstance>();
//	Output->Material = RenderMaterial;
//	Output->Flags = Flags;
//	return Output;
//}
//
