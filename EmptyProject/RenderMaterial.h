#pragma once
#include "Essence.h"
#include "Resource.h"
#include "Pipeline.h"

enum ERootParamType {
	Table
};

struct FRootParamSetupCmd {
	u32 Index;
	union {
		struct {
			D3D12_GPU_DESCRIPTOR_HANDLE Handle;
		} Table;
	};
	ERootParamType Type;
};

enum class ERenderPass {
	Depth,
	Base,
	Forward
};

class FRenderPass;
class FSceneActor;
class FRenderMaterial_Pass;

class FRenderMaterial {
public:
	eastl::wstring ShaderName;

	//virtual void UpdateDescriptors(FSceneActor *, FRenderMaterial_Pass *) = 0;

	FRenderMaterial(eastl::wstring ShaderName);
};
DECORATE_CLASS_REF(FRenderMaterial);

class FRootSignature;

class FRenderMaterial_Pass {
public:
	FRenderMaterialRef RenderMaterial;
	FShaderStateRef ShaderState;
	FRenderPass * RenderPass;
	FPipelineState * PSO;

	void UpdateDescriptors(FSceneActor *);
};
DECORATE_CLASS_REF(FRenderMaterial_Pass);

enum class EMaterialShaderParam {
	ConstantBuffer,
	ShaderResource,
	UnorderedAccess
};

const u64 ShaderParamNameBitsNum = 62;

// todo: more general? use for buffers etc.
// indirection between final texture used and requested texture
// useful for special textures, errors etc
class FTextureAsset {
public:
	FGPUResourceRef Resource;
	eastl::wstring TextureName;
};
DECORATE_CLASS_REF(FTextureAsset);

struct FMaterialShaderParam {
	u64 Input;
	FMaterialShaderParam() = default;

	inline FMaterialShaderParam(EMaterialShaderParam Type, u64 NameHash) {
		u64 NameBitsMask = (1ull << (ShaderParamNameBitsNum - 1)) - 1;
		Input = ((u64)Type << ShaderParamNameBitsNum) | (NameHash & NameBitsMask);
	}

	inline FMaterialShaderParam(EMaterialShaderParam Type, const char * Name, u64 NameLen) :
		FMaterialShaderParam(Type, MurmurHash2_64(Name, NameLen, 0))
	{
	}
};

class FRenderMaterialInstance {
public:
	FRenderMaterialRef RenderMaterial;
	u32 IsTransparent : 1;
	u32 IsAlphaMasked : 1;
	// vertex type
	// textures

	// can cache material specific data (textures table?)
	struct FRenderParamBinding {
		FMaterialShaderParam Param;
	};
	eastl::vector<FRenderParamBinding> SRVs;
	
	bool IsRenderedWithPass(FRenderPass * ) const { return true; }

	FRenderMaterialInstance(FRenderMaterialRefParam RenderMaterial);

	void SetTexture(FMaterialShaderParam Param, FTextureAssetRefParam Texture);
};
DECORATE_CLASS_REF(FRenderMaterialInstance);

class FRenderPass_MaterialInstance {
public:
	// can cache material specific data (textures table?)
	FRenderMaterialInstanceRef RenderMaterialInstance;
	// render pass or scene render pass? scene could store specific formats of rtvs if changeable
	FRenderPass * RenderPass;
	FPipelineState * PSO = nullptr;
	FShaderStateRef ShaderState;

	FRenderPass_MaterialInstance(FRenderPass * InRenderPass, FRenderMaterialInstanceRefParam InRenderMaterialInstance);

	void Prepare();
};
DECORATE_CLASS_REF(FRenderPass_MaterialInstance);

FRenderPass_MaterialInstanceRef GetRenderPass_MaterialInstance(FRenderPass *, FRenderMaterialInstanceRefParam);

struct FBasicMaterialDesc {
	float3 Diffuse;
	eastl::wstring DiffuseTexturePath;
	bool bTransparent;
};

FRenderMaterialInstanceRef GetBasicMaterialInstance(FBasicMaterialDesc const& Desc);

//#include <EASTL\string.h>
//#include <EASTL\shared_ptr.h>
//#include "Pipeline.h"
//
//class FShader;
//
//enum class EVertexInterfaceFlags {
//	Static,
//	Skinned
//};
//
//class FVertexInterface {
//public:
//	eastl::string ShaderInclude;
//	FInputLayout * Layout;
//	EVertexInterfaceFlags Flags;
//	u64 Hash;
//};
//
//typedef eastl::shared_ptr<FVertexInterface> FVertexInterfaceRef;
//typedef eastl::shared_ptr<FVertexInterface> & FVertexInterfaceRefParam;
//
//FVertexInterfaceRef CreateVertexInterface(eastl::string shaderInclude, EVertexInterfaceFlags flags, std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements);
//FVertexInterfaceRef GetRichVertexInterface();
//
//class FRenderMaterial {
//public:	
//	eastl::string ShaderInclude;
//	u64 Hash;
//};
//
//typedef eastl::shared_ptr<FRenderMaterial> FRenderMaterialRef;
//typedef eastl::shared_ptr<FRenderMaterial> & FRenderMaterialRefParam;
//
//FRenderMaterialRef CreateRenderMaterial(eastl::string shaderInclude);
//FRenderMaterialRef GetStandardMaterial();
//
//enum class ERenderMaterialInstanceFlags {
//	Solid,
//	AlphaMasked,
//	Transparent
//};
//
//#include "VideoMemory.h"
//
//class FRenderMaterialInstance {
//public:
//	FRenderMaterialRef Material;
//	ERenderMaterialInstanceFlags Flags;
//	/*bool UseWithRenderPass(FPRenderPassType);
//	u32 UsageMask;*/
//
//	struct FTextureBinding {
//		FSRVParam Param;
//		FGPUResourceRef Texture;
//	};
//
//	struct FConstantBufferBinding {
//		FCBVParam Param;
//		eastl::vector<u8> Data;
//	};
//
//	eastl::vector<FTextureBinding> Textures;
//	eastl::vector<FConstantBufferBinding> Constants;
//};
//
//typedef eastl::shared_ptr<FRenderMaterialInstance> FRenderMaterialInstanceRef;
//typedef eastl::shared_ptr<FRenderMaterialInstance> & FRenderMaterialInstanceRefParam;
//
//FRenderMaterialInstanceRef CreateRenderMaterialInstance(FRenderMaterialRef RenderMaterial, ERenderMaterialInstanceFlags Flags);
//
//class FRenderPass {
//public:
//	eastl::string ShaderInclude;
//	u64 Hash;
//};
//
//typedef eastl::shared_ptr<FRenderPass> FRenderPassRef;
//typedef eastl::shared_ptr<FRenderPass> & FRenderPassRefParam;
//
//FRenderPassRef CreateRenderPass(eastl::string shaderInclude);
//
//class FMaterialShader : public FShader {
//public:
//	FRenderPassRef RenderPass;
//	FRenderMaterialRef RenderMaterial;
//	FVertexInterfaceRef VertexInterface;
//
//	eastl::string							Func;
//	const char*								Target;
//	u32										Flags;
//
//	void Compile() override;
//	eastl::wstring	GetDebugName() const override;
//};
//
//enum EShaderType {
//	VertexShader,
//	PixelShader
//};
//
//FShaderRef GetMaterialShader(FRenderPassRefParam, FRenderMaterialRefParam, FVertexInterfaceRefParam, EShaderType);
//
//struct FMaterialDesc {
//
//};