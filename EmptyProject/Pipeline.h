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


class FPipelineFactory {
public:
	eastl::hash_map<u64, FPipelineState*>		Cached;

	EPipelineType							PipelineType;
	FShaderState *								ShaderState;
	FInputLayout *								InputLayout;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC			PipelineDesc;
	D3D12_COMPUTE_PIPELINE_STATE_DESC			ComputePipelineDesc;

	u32											Dirty : 1;
	FPipelineState *							CurrentPipelineState;

	FPipelineFactory();

	void SetInputLayout(FInputLayout * inInputLayout);
	void SetShaderState(FShaderState * inShaderState);
	void SetRenderTarget(DXGI_FORMAT Format, u32 Index);
	void SetDepthStencil(DXGI_FORMAT Format);
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY inTopology);
	void SetRasterizerState(D3D12_RASTERIZER_DESC const& RasterizerState);
	void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC const& DepthStencilState);
	void SetBlendState(D3D12_BLEND_DESC const& BlendState);
	FPipelineState * GetPipelineState();
	void Reset();
};

constexpr const DXGI_FORMAT NULL_FORMAT = DXGI_FORMAT_UNKNOWN;