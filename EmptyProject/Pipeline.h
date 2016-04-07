#pragma once
#include "Essence.h"
#include <EASTL/initializer_list.h>
#include <d3d12.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/array.h>
#include "Commands.h"
#include "Print.h"

class FPipelineState;
class FShader;
class FShaderBindings;
class FInputLayout;
class FGraphicsRootLayout;
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

D3D12_GRAPHICS_PIPELINE_STATE_DESC	GetDefaultPipelineStateDesc();
FInputLayout*			GetInputLayout(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements);
FPipelineState*			GetGraphicsPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC const *desc, FRootSignature const* rootsig, FShader const* vs, FShader const* ps, FInputLayout const* inputLayout);
FGraphicsRootLayout*	GetRootLayout(FShader const* vs, FShader const* ps);
FRootSignature*			GetRootSignature(FGraphicsRootLayout const*);

ID3D12RootSignature*	GetRawRootSignature(FGraphicsRootLayout const*);
ID3D12PipelineState*	GetRawPSO(FPipelineState const*);

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

struct FShaderParam {
	GlobalBindId	BindId;
};

struct FConstantParam;

struct FConstantBufferParam {
	struct alignas(16) DATA_PACK_t {
		u8 _DATA[256];
	};

	GlobalBindId				BindId;
	eastl::vector<DATA_PACK_t>	Data;
	u32							Size;

	D3D12_CPU_DESCRIPTOR_HANDLE	CPUHandle;

	inline void Reserve(u64 size) {
		Data.resize((size + 255) / 256);
	}

	inline void Set(FConstantParam const * Param, void const * src, u64 size) {
		// why memcpy fails on release?
		memcpy_s(Data.data(), sizeof(Data[0]) * Data.size(), src, size);
	}

	template<typename T> void	Set(FConstantParam const * Param, T const&srcRef) {
		Set(Param, &srcRef, sizeof(T));
	}

	void Serialize();
};
struct FConstantParam {
	GlobalBindId	BindId;
	u32				CbIndex;
	u32				Offset;
	u32				Size;
};

struct FConstantBuffer {

	GlobalBindId				BindId;
	u32							Size;
	FGraphicsRootLayout const *	Layout;

	void	CreateConstantParam(const char *, FConstantParam & outParam);
	void	CreateConstantBufferVersion(FConstantBufferParam & outParam);
};

struct FRootParam {
	u32											TableLen;
	eastl::vector<D3D12_CPU_DESCRIPTOR_HANDLE>	NullHandles;
};

class FGraphicsRootLayout {
public:
	FRootSignature*	RootSignature;
	eastl::hash_map<GlobalBindId, BindDesc_t>				Textures;
	eastl::hash_map<GlobalBindId, ConstantBufferBinding_t>	ConstantBuffers;
	eastl::hash_map<GlobalBindId, BindDesc_t>				RWTextures;
	eastl::hash_map<GlobalBindId, BindDesc_t>				Samplers;
	eastl::hash_map<GlobalBindId, ConstatsBinding_t>		Constants;
	eastl::array<FRootParam, MAX_ROOT_PARAMS>				RootParams;
	u32														RootParamsNum;

	void		FillBindings(FShaderBindings* bindings);

	void		CreateConstantBuffer(char const * name, FConstantBuffer& outParam);
	void		CreateTextureParam(char const * name, FShaderParam& outParam);
	void		CreateRWTextureParam(char const * name, FShaderParam& outParam);
};

inline bool operator == (GlobalBindId a, GlobalBindId b) { return a.hash == b.hash; };
inline bool operator != (GlobalBindId a, GlobalBindId b) { return a.hash != b.hash; };