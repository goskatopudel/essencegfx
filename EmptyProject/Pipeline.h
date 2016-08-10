#pragma once
#include "Essence.h"
#include <EASTL/initializer_list.h>
#include <d3d12.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/array.h>
#include <EASTL/map.h>
#include "Commands.h"
#include "Print.h"

class FPipelineState;
class FShader;
class FShaderBindings;
class FInputLayout;
class FRootLayout;
class FRootSignature;
struct D3D12_GRAPHICS_PIPELINE_STATE_DESC;


const u16 ROOT_STATIC_SAMPLER = 0xFFFF;
const u16 ROOT_VIEW_OFFSET = 0xFFFF;

struct BindDesc_t {
	u16		RootParam;
	u16		DescOffset;

	BindDesc_t() = default;
	BindDesc_t(u16 rootParam) : RootParam(rootParam), DescOffset(ROOT_VIEW_OFFSET) {}
	BindDesc_t(u16 rootParam, u16 descOffset) : RootParam(rootParam), DescOffset(descOffset) {}
};

struct GlobalBindId {
	u64 hash;

	GlobalBindId() = default;
	GlobalBindId(u64 v) : hash(v) {}
};
bool operator == (GlobalBindId a, GlobalBindId b);
bool operator != (GlobalBindId a, GlobalBindId b);
template<>
struct eastl::hash<GlobalBindId> { u64 operator()(GlobalBindId h) const { return h.hash; } };

class FInputLayout {
public:
	eastl::unique_ptr<D3D12_INPUT_ELEMENT_DESC[]>	Elements;
	u32 ElementsNum;
	u64 ValueHash;

	FInputLayout() = default;
	FInputLayout(u64 hash, std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements) : ValueHash(hash) {
		Elements = eastl::make_unique<D3D12_INPUT_ELEMENT_DESC[]>(elements.size());
		u32 index = 0;
		for (auto &element : elements) {
			Elements[index++] = element;
		}
		ElementsNum = (u32)elements.size();
	}
};

class FShaderState {
public:
	EPipelineType		Type;
	union {
		struct {
			FShader *		VertexShader;
			FShader *		HullShader;
			FShader *		DomainShader;
			FShader *		GeometryShader;
			FShader *		PixelShader;
		};
		FShader *			ComputeShader;
	};
	FRootSignature *		RootSignature;
	FRootLayout*			Root;
	bool					FixedRootSignature;
	u64						ShadersCompilationVersion = 0;
	u64						ContentHash;

	FShaderState() = default;
	FShaderState(FShader * inComputeShader, FRootSignature * inRootSignature = nullptr);
	FShaderState(FShader * inVertexShader, FShader * inPixelShader, FRootSignature * inRootSignature = nullptr);
	FShaderState(FShader * inVertexShader, FShader * inGeometryShader, FShader * inPixelShader, FRootSignature * inRootSignature = nullptr);
	FShaderState(FShader * inVertexShader, FShader * inHullShader, FShader * inDomainShader, FShader * inPixelShader, FRootSignature * inRootSignature = nullptr);
	FShaderState(FShader * inVertexShader, FShader * inHullShader, FShader * inDomainShader, FShader * inGeometryShader, FShader * inPixelShader, FRootSignature * inRootSignature = nullptr);

	virtual void InitParams() = 0;

	void Compile();
	bool IsOutdated() const;

	eastl::wstring	GetDebugName() const;
};

class FPipelineState {
public:
	EPipelineType					Type;
	union {
		struct {
			D3D12_COMPUTE_PIPELINE_STATE_DESC   Desc;
		} Compute;
		struct {
			D3D12_GRAPHICS_PIPELINE_STATE_DESC	Desc;
			FInputLayout const*					InputLayout;
		} Graphics;
	};
	FShaderState *						ShaderState;

	unique_com_ptr<ID3D12PipelineState>	D12PipelineState;
	u64									BlobVersion = 0;
	// last version of shaders we compiled with
	u64									ShadersCompilationVersion = 0;

	void Compile();
	bool IsOutdated() const;
};

D3D12_GRAPHICS_PIPELINE_STATE_DESC	GetDefaultPipelineStateDesc();
FInputLayout*			GetInputLayout(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements);
FPipelineState*			GetGraphicsPipelineState(FShaderState * ShaderState, D3D12_GRAPHICS_PIPELINE_STATE_DESC const *Desc, FInputLayout const * InputLayout);
FPipelineState*			GetComputePipelineState(FShaderState * ShaderState, D3D12_COMPUTE_PIPELINE_STATE_DESC const *Desc);
FRootLayout*			GetRootLayout(FShader const* VS, FShader const* HS, FShader const* DS, FShader const* GS, FShader const* PS, FRootSignature * RootSignature = nullptr);
FRootLayout*			GetRootLayout(FShader const* CS, FRootSignature * RootSignature = nullptr);
FRootSignature*			GetRootSignature(FRootLayout const*);
u32						GetPSOsNum();

ID3D12RootSignature*	GetRawRootSignature(FRootLayout const*);
ID3D12PipelineState*	GetRawPSO(FPipelineState const*);

void					RecompileChangedPipelines();

inline D3D12_INPUT_ELEMENT_DESC CreateInputElement(const char* SemanticName, DXGI_FORMAT Format, u32 SemanticIndex = 0, u32 InputSlot = 0) {
	D3D12_INPUT_ELEMENT_DESC Element;
	Element.SemanticName = SemanticName;
	Element.SemanticIndex = SemanticIndex;
	Element.Format = Format;
	Element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	Element.InputSlot = InputSlot;
	Element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	Element.InstanceDataStepRate = 0;
	return Element;
}

struct ConstantBufferBinding_t {
	BindDesc_t				Bind;
	u16						Index;
	u16						Space;
	u32						Size;
	eastl::unique_ptr<u8[]>	DefaultValue;
};

struct ConstatsBinding_t {
	GlobalBindId			ConstantBuffer;
	u32						BufferOffset;
	u32						Size;
};

struct FTextureParam {
	GlobalBindId	BindId;
};

struct FRWTextureParam {
	GlobalBindId	BindId;
};

struct FConstantBuffer {
	GlobalBindId				BindId;
	u32							Size;
	FRootLayout const *	Layout;
};

#include "PointerMath.h"

struct FRootParam {
	u32											TableLen;
	eastl::vector<D3D12_CPU_DESCRIPTOR_HANDLE>	NullHandles;
};

enum RootParamIndexEnum {
	PARAM_0,
	PARAM_1,
	PARAM_2,
	PARAM_3,
	PARAM_4,
	PARAM_5,
	PARAM_6,
	PARAM_7,
};

enum class RootSlotType {
	CBV,
	SRV,
	UAV,
	SAMPLER
};

struct SlotsRange {
	RootSlotType type;
	u32 baseRegister;
	u32 space;
	u32 len;
	D3D12_SHADER_VISIBILITY visibility;
};


enum EShaderStageFlag {
	STAGE_NONE = 0,
	STAGE_VERTEX = 1,
	STAGE_HULL = 2,
	STAGE_DOMAIN = 4,
	STAGE_GEOMETRY = 8,
	STAGE_PIXEL = 16,
	STAGE_ALL = 31,
	STAGE_COMPUTE = STAGE_ALL
};
DEFINE_ENUM_FLAG_OPERATORS(EShaderStageFlag);

class FRootSignature {
public:
	unique_com_ptr<ID3D12RootSignature>			D12RootSignature;
	u64											ValueHash;
	eastl::vector<D3D12_ROOT_PARAMETER>			Params;
	eastl::vector<D3D12_DESCRIPTOR_RANGE>		Ranges;
	eastl::vector<D3D12_STATIC_SAMPLER_DESC>	StaticSamplers;
	D3D12_ROOT_SIGNATURE_FLAGS					Flags;
	eastl::vector<u32>							ParamRangesOffset;

	u32											RootSize = 0;
	u32											CurrentParamIndex = 0;

	void InitDefault(D3D12_SHADER_VISIBILITY samplersVisibility, EShaderStageFlag stagesUsed);

	eastl::map<SlotsRange, BindDesc_t>			Slots;

	bool		ContainsSlot(RootSlotType type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility, EShaderStageFlag usedStages);
	BindDesc_t	GetSlotBinding(RootSlotType type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility);
	void		AddRootViewParam(u32 rootParam, D3D12_ROOT_PARAMETER_TYPE type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility);
	void		AddCBVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility);
	void		AddSRVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility);
	void		AddUAVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility);
	void		AddTableParam(u32 rootParam, D3D12_SHADER_VISIBILITY visibility);
	u16			AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE type, u32 baseRegister, u32 len, u32 space);
	void		AddTableCBVRange(u32 baseRegister, u32 len, u32 space);
	void		AddTableSRVRange(u32 baseRegister, u32 len, u32 space);
	void		AddTableUAVRange(u32 baseRegister, u32 len, u32 space);
	void		AddTableSamplerRange(u32 baseRegister, u32 len, u32 space);
	void		SerializeAndCreate();
};

class FRootLayout {
public:
	FRootSignature*	RootSignature;
	eastl::hash_map<GlobalBindId, BindDesc_t>				Textures;
	eastl::hash_map<GlobalBindId, ConstantBufferBinding_t>	ConstantBuffers;
	eastl::hash_map<GlobalBindId, BindDesc_t>				RWTextures;
	eastl::hash_map<GlobalBindId, BindDesc_t>				Samplers;
	eastl::hash_map<GlobalBindId, ConstatsBinding_t>		Constants;
	eastl::array<FRootParam, MAX_ROOT_PARAMS>				RootParams;
	u32														RootParamsNum;

	void				FillBindings(FShaderBindings* bindings);

	FConstantBuffer		CreateConstantBuffer(FShaderState *, char const * name);
	FTextureParam		CreateTextureParam(FShaderState *, char const * name);
	FRWTextureParam		CreateRWTextureParam(FShaderState *, char const * name);
};

inline bool operator == (GlobalBindId a, GlobalBindId b) { return a.hash == b.hash; };
inline bool operator != (GlobalBindId a, GlobalBindId b) { return a.hash != b.hash; };


void SetD3D12StateDefaults(D3D12_RASTERIZER_DESC *pDest);
void SetD3D12StateDefaults(D3D12_DEPTH_STENCIL_DESC *pDest);
void SetD3D12StateDefaults(D3D12_BLEND_DESC *pDest);
bool IsDepthReadOnly(D3D12_GRAPHICS_PIPELINE_STATE_DESC const* desc);
bool IsStencilReadOnly(D3D12_GRAPHICS_PIPELINE_STATE_DESC const* desc);
D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimitiveTopologyType(D3D_PRIMITIVE_TOPOLOGY topology);

class FPipelineCache {
public:
	eastl::hash_map<u64, FPipelineState*> Cached;

	FPipelineCache();
};

template<typename T>
struct FPipelineContext {
	FPipelineCache * PipelineCache = nullptr;
	T * Recorder = nullptr;

	EPipelineType PipelineType;
	FShaderState * ShaderState;
	FInputLayout * InputLayout;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc;
	D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePipelineDesc;

	u32 Dirty : 1;
	FPipelineState * CurrentPipelineState;

	FPipelineContext() {
		Reset();
	}

	void Bind(T * Stream, FPipelineCache * Cache) {
		Recorder = Stream;
		PipelineCache = Cache;
	}
	void Reset();

	void ClearRenderTargets();
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, DXGI_FORMAT Format, u32 Index = 0);
	void SetDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE DSV, DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN);
	void SetInputLayout(FInputLayout * InInputLayout);
	void SetShaderState(FShaderState * InShaderState);
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY InTopology);
	void SetRasterizerState(D3D12_RASTERIZER_DESC const& RasterizerState);
	void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC const& DepthStencilState);
	void SetBlendState(D3D12_BLEND_DESC const& BlendState);
	void SetRenderTargetsBundle(struct FRenderTargetsBundle const * RenderTargets);

	void ApplyState();
};

constexpr const DXGI_FORMAT NULL_FORMAT = DXGI_FORMAT_UNKNOWN;


#include "Viewport.h"
#include "Hash.h"

template<typename T>
void FPipelineContext<T>::SetRasterizerState(D3D12_RASTERIZER_DESC const& RasterizerState) {
	PipelineDesc.RasterizerState = RasterizerState;
	Dirty = 1;
}

template<typename T>
void FPipelineContext<T>::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC const& DepthStencilState) {
	PipelineDesc.DepthStencilState = DepthStencilState;
	Dirty = 1;
}

template<typename T>
void FPipelineContext<T>::SetBlendState(D3D12_BLEND_DESC const& BlendState) {
	PipelineDesc.BlendState = BlendState;
	Dirty = 1;
}

template<typename T>
void FPipelineContext<T>::Reset() {
	PipelineType = EPipelineType::Graphics;
	CurrentPipelineState = nullptr;

	ShaderState = nullptr;
	InputLayout = nullptr;

	PipelineDesc = {};
	PipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	PipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	PipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	PipelineDesc.SampleMask = UINT_MAX;
	PipelineDesc.SampleDesc.Count = 1;
	PipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	ComputePipelineDesc = {};
}

template<typename T>
void FPipelineContext<T>::ClearRenderTargets() {
	for (u32 Index = 0; Index < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++Index) {
		SetRenderTarget({}, DXGI_FORMAT_UNKNOWN, Index);
		Recorder->SetRenderTarget({}, Index);
	}
}

template<typename T>
void FPipelineContext<T>::SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, DXGI_FORMAT Format, u32 Index) {
	if (PipelineDesc.RTVFormats[Index] != Format) {
		Dirty = 1;
		PipelineDesc.RTVFormats[Index] = Format;

		if (Format != DXGI_FORMAT_UNKNOWN) {
			PipelineDesc.NumRenderTargets = eastl::max(PipelineDesc.NumRenderTargets, Index + 1);
		}
		else {
			i32 maxIndex = PipelineDesc.NumRenderTargets;
			for (i32 i = (i32)PipelineDesc.NumRenderTargets - 1; i >= 0; --i) {
				if (PipelineDesc.RTVFormats[i] == DXGI_FORMAT_UNKNOWN) {
					--PipelineDesc.NumRenderTargets;
					check(i > 0 || PipelineDesc.NumRenderTargets == 0);
				}
				else {
					break;
				}
			}
		}
	}

	Recorder->SetRenderTarget(RTV, Index);
}

template<typename T>
void FPipelineContext<T>::SetDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE DSV, DXGI_FORMAT Format) {
	if (Format != PipelineDesc.DSVFormat) {
		PipelineDesc.DSVFormat = Format;
		Dirty = 1;
	}
	Recorder->SetDepthStencil(DSV);
}

template<typename T>
void FPipelineContext<T>::SetInputLayout(FInputLayout * InInputLayout) {
	if (InputLayout != InInputLayout) {
		InputLayout = InInputLayout;
		Dirty = 1;
	}
}

template<typename T>
void FPipelineContext<T>::SetShaderState(FShaderState * InShaderState) {
	if (ShaderState != InShaderState) {
		ShaderState = InShaderState;
		PipelineType = ShaderState->Type;
		Dirty = 1;
	}
}

template<typename T>
void FPipelineContext<T>::SetTopology(D3D_PRIMITIVE_TOPOLOGY InTopology) {
	if (GetPrimitiveTopologyType(InTopology) != PipelineDesc.PrimitiveTopologyType) {
		PipelineDesc.PrimitiveTopologyType = GetPrimitiveTopologyType(InTopology);
		Dirty = 1;
	}
	Recorder->SetTopology(InTopology);
}

template<typename T>
void FPipelineContext<T>::SetRenderTargetsBundle(struct FRenderTargetsBundle const * RenderTargets) {
	if (RenderTargets->DepthBuffer) {
		SetDepthStencil(RenderTargets->DepthBuffer->GetDSV(), RenderTargets->DepthBuffer->GetWriteFormat());
	}

	u32 Index = 0;
	for (auto & RT : RenderTargets->Outputs) {
		SetRenderTarget(RenderTargets->Outputs[Index].GetRTV(), RenderTargets->Outputs[Index].GetFormat(), Index);
		Index++;
	}
	Recorder->SetRenderTargetsBundle(RenderTargets);
}

template<typename T>
void FPipelineContext<T>::ApplyState() {
	if (Dirty) {
		if (PipelineType == EPipelineType::Graphics) {
			u64 Hash = MurmurHash2_64(&PipelineDesc, sizeof(PipelineDesc), 0);
			Hash = HashCombine64(Hash, ShaderState->ContentHash);
			Hash = HashCombine64(Hash, InputLayout->ValueHash);

			auto Iter = PipelineCache->Cached.find(Hash);
			if (Iter == PipelineCache->Cached.end()) {
				CurrentPipelineState = GetGraphicsPipelineState(ShaderState, &PipelineDesc, InputLayout);

				PipelineCache->Cached[Hash] = CurrentPipelineState;
			}
			else {
				CurrentPipelineState = Iter->second;
			}
		}
		else {
			u64 Hash = MurmurHash2_64(&PipelineDesc, sizeof(PipelineDesc), 0);

			auto Iter = PipelineCache->Cached.find(Hash);
			if (Iter == PipelineCache->Cached.end()) {
				CurrentPipelineState = GetComputePipelineState(ShaderState, &ComputePipelineDesc);

				PipelineCache->Cached[Hash] = CurrentPipelineState;
			}
			else {
				CurrentPipelineState = Iter->second;
			}
		}

		Dirty = 0;
	}

	Recorder->SetPipelineState(CurrentPipelineState);
}