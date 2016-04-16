#include "Pipeline.h"
#include "Device.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/initializer_list.h>
#include <EASTL/hash_map.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/map.h>

#include "Hash.h"
#include "Shader.h"
#include "Print.h"

#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

D3D12_GRAPHICS_PIPELINE_STATE_DESC	GetDefaultPipelineStateDesc() {
	static D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	static bool Initialized = false;
	if (!Initialized) {
		Initialized = true;

		ZeroMemory(&Desc, sizeof(Desc));
		Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		Desc.SampleMask = UINT_MAX;
		Desc.SampleDesc.Count = 1;
		Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		Desc.DepthStencilState.DepthEnable = false;
	}

	return Desc;
}

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

eastl::hash_map<u64, eastl::shared_ptr<FInputLayout>> InputLayoutsLookup;

FInputLayout* GetInputLayout(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elements) {
	u64 hash = 0;
	for (auto e : elements) {
		D3D12_INPUT_ELEMENT_DESC element = e;
		hash = MurmurHash2_64(e.SemanticName, strlen(e.SemanticName), hash);
		e.SemanticName = nullptr;
		hash = MurmurHash2_64(&element, sizeof(element), hash);
	}

	auto iter = InputLayoutsLookup.find(hash);
	if (iter != InputLayoutsLookup.end()) {
		return iter->second.get();
	}

	InputLayoutsLookup[hash] = eastl::make_shared<FInputLayout>(hash, elements);
	return InputLayoutsLookup[hash].get();
}

#include <EASTL/set.h>

bool Intersects(SlotsRange const& lhs, SlotsRange const& rhs) {
	return lhs.type == rhs.type
		&& (lhs.visibility == rhs.visibility || lhs.visibility == D3D12_SHADER_VISIBILITY_ALL || rhs.visibility == D3D12_SHADER_VISIBILITY_ALL)
		&& (lhs.space == rhs.space)
		&& (lhs.baseRegister + lhs.len > rhs.baseRegister && lhs.baseRegister < rhs.baseRegister + rhs.len);
}

bool operator< (SlotsRange const& lhs, SlotsRange const& rhs) {
	if (lhs.type != rhs.type) {
		return lhs.type < rhs.type;
	}
	if (lhs.visibility != rhs.visibility) {
		return lhs.visibility < rhs.visibility;
	}
	if (lhs.space != rhs.space) {
		return lhs.space < rhs.space;
	}
	if (lhs.baseRegister != rhs.baseRegister) {
		return lhs.baseRegister < rhs.baseRegister;
	}
	return false;
}

bool FRootSignature::ContainsSlot(RootSlotType type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {

	SlotsRange range = {};
	range.type = type;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = visibility;
	range.len = 1;

	SlotsRange lookupRange = {};
	lookupRange.type = type;
	lookupRange.baseRegister = baseRegister;
	lookupRange.space = space;
	lookupRange.visibility = D3D12_SHADER_VISIBILITY_ALL;
	lookupRange.len = 1;

	auto lbIter = Slots.lower_bound(lookupRange);

	if (lbIter != Slots.end() && Intersects(lbIter->first, range)) {
		return true;
	}

	if (visibility != D3D12_SHADER_VISIBILITY_ALL) {
		lookupRange.visibility = visibility;

		lbIter = Slots.lower_bound(lookupRange);

		if (lbIter != Slots.end()) {
			return Intersects(lbIter->first, range);
		}
	}

	return false;
}

BindDesc_t FRootSignature::GetSlotBinding(RootSlotType type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {
	SlotsRange range = {};
	range.type = type;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = visibility;
	range.len = 1;

	auto lbIter = Slots.lower_bound(range);
	if (!Intersects(lbIter->first, range)) {
		range.visibility = D3D12_SHADER_VISIBILITY_ALL;
		lbIter = Slots.lower_bound(range);
	}
	check(Intersects(lbIter->first, range));

	auto desc = lbIter->second;
	if (desc.DescOffset == ROOT_VIEW_OFFSET) {
		check((baseRegister - lbIter->first.baseRegister) == 0);
	}
	desc.DescOffset += baseRegister - lbIter->first.baseRegister;

	return desc;
}

void FRootSignature::AddRootViewParam(u32 rootParam, D3D12_ROOT_PARAMETER_TYPE type, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {
	D3D12_ROOT_PARAMETER parameter = {};
	parameter.ParameterType = type;
	parameter.ShaderVisibility = visibility;
	parameter.Descriptor.ShaderRegister = baseRegister;
	parameter.Descriptor.RegisterSpace = space;

	if (Params.size() > rootParam) {
		// check for overwrites
		check(Params[rootParam].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
		check(Params[rootParam].DescriptorTable.NumDescriptorRanges == 0);
		check(ParamRangesOffset[rootParam] == 0);
	}

	Params.resize(eastl::max((u32)Params.size(), rootParam + 1));
	Params[rootParam] = parameter;
	ParamRangesOffset.resize(eastl::max((u32)ParamRangesOffset.size(), rootParam + 1));
	ParamRangesOffset[rootParam] = 0xFFFFFFFF;

	RootSize += 2;
	check(RootSize <= 64);

	CurrentParamIndex = rootParam;
}

void FRootSignature::AddCBVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {
	check(!ContainsSlot(RootSlotType::CBV, baseRegister, space, visibility));
	AddRootViewParam(rootParam, D3D12_ROOT_PARAMETER_TYPE_CBV, baseRegister, space, visibility);

	SlotsRange range = {};
	range.type = RootSlotType::CBV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(rootParam);
}

void FRootSignature::AddSRVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {
	check(!ContainsSlot(RootSlotType::SRV, baseRegister, space, visibility));
	AddRootViewParam(rootParam, D3D12_ROOT_PARAMETER_TYPE_SRV, baseRegister, space, visibility);

	SlotsRange range = {};
	range.type = RootSlotType::SRV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(rootParam);
}

void FRootSignature::AddUAVParam(u32 rootParam, u32 baseRegister, u32 space, D3D12_SHADER_VISIBILITY visibility) {
	check(!ContainsSlot(RootSlotType::UAV, baseRegister, space, visibility));
	AddRootViewParam(rootParam, D3D12_ROOT_PARAMETER_TYPE_UAV, baseRegister, space, visibility);

	SlotsRange range = {};
	range.type = RootSlotType::UAV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(rootParam);
}

void FRootSignature::AddTableParam(u32 rootParam, D3D12_SHADER_VISIBILITY visibility) {
	D3D12_ROOT_PARAMETER parameter = {};
	parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameter.ShaderVisibility = visibility;

	if (Params.size() > rootParam) {
		// check for overwrites
		check(Params[rootParam].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
		check(Params[rootParam].DescriptorTable.NumDescriptorRanges == 0);
		check(ParamRangesOffset[rootParam] == 0);
	}

	Params.resize(eastl::max((u32)Params.size(), rootParam + 1));
	Params[rootParam] = parameter;
	ParamRangesOffset.resize(eastl::max((u32)ParamRangesOffset.size(), rootParam + 1));
	ParamRangesOffset[rootParam] = (u32)Ranges.size();

	RootSize += 1;
	check(RootSize <= 64);

	CurrentParamIndex = rootParam;
}

u16 FRootSignature::AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE type, u32 baseRegister, u32 len, u32 space) {
	check(Params[CurrentParamIndex].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);

	D3D12_DESCRIPTOR_RANGE range;
	range.BaseShaderRegister = baseRegister;
	range.RegisterSpace = space;
	range.NumDescriptors = len;
	range.RangeType = type;
	range.OffsetInDescriptorsFromTableStart = 0;

	if (Params[CurrentParamIndex].DescriptorTable.NumDescriptorRanges) {
		u32 lastRangeIndex = ParamRangesOffset[CurrentParamIndex] + Params[CurrentParamIndex].DescriptorTable.NumDescriptorRanges - 1;
		range.OffsetInDescriptorsFromTableStart = Ranges[lastRangeIndex].OffsetInDescriptorsFromTableStart + Ranges[lastRangeIndex].NumDescriptors;
	}

	Ranges.push_back(range);
	Params[CurrentParamIndex].DescriptorTable.NumDescriptorRanges++;

	return (u16)range.OffsetInDescriptorsFromTableStart;
}

void FRootSignature::AddTableCBVRange(u32 baseRegister, u32 len, u32 space) {
	for (u32 i = 0; i < len; ++i) {
		check(!ContainsSlot(RootSlotType::CBV, baseRegister + i, space, Params[CurrentParamIndex].ShaderVisibility));
	}
	u16 descTableOffset = AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, baseRegister, len, space);

	SlotsRange range = {};
	range.type = RootSlotType::CBV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = Params[CurrentParamIndex].ShaderVisibility;
	range.len = len;
	Slots[range] = BindDesc_t(CurrentParamIndex, descTableOffset);
}

void FRootSignature::AddTableSRVRange(u32 baseRegister, u32 len, u32 space) {
	for (u32 i = 0; i < len; ++i) {
		check(!ContainsSlot(RootSlotType::SRV, baseRegister + i, space, Params[CurrentParamIndex].ShaderVisibility));
	}
	u16 descTableOffset = AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, baseRegister, len, space);

	SlotsRange range = {};
	range.type = RootSlotType::SRV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = Params[CurrentParamIndex].ShaderVisibility;
	range.len = len;
	Slots[range] = BindDesc_t(CurrentParamIndex, descTableOffset);
}

void FRootSignature::AddTableUAVRange(u32 baseRegister, u32 len, u32 space) {
	for (u32 i = 0; i < len; ++i) {
		check(!ContainsSlot(RootSlotType::UAV, baseRegister + i, space, Params[CurrentParamIndex].ShaderVisibility));
	}
	u16 descTableOffset = AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, baseRegister, len, space);

	SlotsRange range = {};
	range.type = RootSlotType::UAV;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = Params[CurrentParamIndex].ShaderVisibility;
	range.len = len;
	Slots[range] = BindDesc_t(CurrentParamIndex, descTableOffset);
}

void FRootSignature::AddTableSamplerRange(u32 baseRegister, u32 len, u32 space) {
	for (u32 i = 0; i < len; ++i) {
		check(!ContainsSlot(RootSlotType::SAMPLER, baseRegister + i, space, Params[CurrentParamIndex].ShaderVisibility));
	}
	u16 descTableOffset = AddTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, baseRegister, len, space);

	SlotsRange range = {};
	range.type = RootSlotType::SAMPLER;
	range.baseRegister = baseRegister;
	range.space = space;
	range.visibility = Params[CurrentParamIndex].ShaderVisibility;
	range.len = len;
	Slots[range] = BindDesc_t(CurrentParamIndex, descTableOffset);
}

void FRootSignature::InitDefault(D3D12_SHADER_VISIBILITY visibility) {
	Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;


	enum StaticSamplersEnum {
		ANISO_STATIC_SAMPLER,
		BILINEAR_WRAP_STATIC_SAMPLER,
		POINT_WRAP_STATIC_SAMPLER,
		BILINEAR_CLAMP_STATIC_SAMPLER,
		POINT_CLAMP_STATIC_SAMPLER,

		STATIC_SAMPLERS_COUNT
	};

	StaticSamplers.resize(STATIC_SAMPLERS_COUNT);

	check(!ContainsSlot(RootSlotType::SAMPLER, 0, ANISO_STATIC_SAMPLER, visibility));

	const u32 ANISO_STATIC_SAMPLER_SPACE = 0;
	StaticSamplers[ANISO_STATIC_SAMPLER].ShaderRegister = 0;
	StaticSamplers[ANISO_STATIC_SAMPLER].RegisterSpace = ANISO_STATIC_SAMPLER;
	StaticSamplers[ANISO_STATIC_SAMPLER].Filter = D3D12_FILTER_ANISOTROPIC;
	StaticSamplers[ANISO_STATIC_SAMPLER].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[ANISO_STATIC_SAMPLER].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[ANISO_STATIC_SAMPLER].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[ANISO_STATIC_SAMPLER].MipLODBias = 0;
	StaticSamplers[ANISO_STATIC_SAMPLER].MaxAnisotropy = 16;
	StaticSamplers[ANISO_STATIC_SAMPLER].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	StaticSamplers[ANISO_STATIC_SAMPLER].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplers[ANISO_STATIC_SAMPLER].MinLOD = 0.f;
	StaticSamplers[ANISO_STATIC_SAMPLER].MaxLOD = D3D12_FLOAT32_MAX;
	StaticSamplers[ANISO_STATIC_SAMPLER].ShaderVisibility = visibility;

	SlotsRange range = {};
	range.type = RootSlotType::SAMPLER;
	range.baseRegister = 0;
	range.space = ANISO_STATIC_SAMPLER;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(ROOT_STATIC_SAMPLER);

	check(!ContainsSlot(RootSlotType::SAMPLER, 0, BILINEAR_WRAP_STATIC_SAMPLER, visibility));

	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].ShaderRegister = 0;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].RegisterSpace = BILINEAR_WRAP_STATIC_SAMPLER;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].MipLODBias = 0;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].MaxAnisotropy = 16;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].MinLOD = 0.f;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].MaxLOD = D3D12_FLOAT32_MAX;
	StaticSamplers[BILINEAR_WRAP_STATIC_SAMPLER].ShaderVisibility = visibility;

	range.type = RootSlotType::SAMPLER;
	range.baseRegister = 0;
	range.space = BILINEAR_WRAP_STATIC_SAMPLER;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(ROOT_STATIC_SAMPLER);

	check(!ContainsSlot(RootSlotType::SAMPLER, 0, POINT_WRAP_STATIC_SAMPLER, visibility));

	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].ShaderRegister = 0;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].RegisterSpace = POINT_WRAP_STATIC_SAMPLER;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].MipLODBias = 0;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].MaxAnisotropy = 16;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].MinLOD = 0.f;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].MaxLOD = D3D12_FLOAT32_MAX;
	StaticSamplers[POINT_WRAP_STATIC_SAMPLER].ShaderVisibility = visibility;

	range.type = RootSlotType::SAMPLER;
	range.baseRegister = 0;
	range.space = POINT_WRAP_STATIC_SAMPLER;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(ROOT_STATIC_SAMPLER);

	check(!ContainsSlot(RootSlotType::SAMPLER, 0, BILINEAR_CLAMP_STATIC_SAMPLER, visibility));

	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].ShaderRegister = 0;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].RegisterSpace = BILINEAR_CLAMP_STATIC_SAMPLER;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].MipLODBias = 0;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].MaxAnisotropy = 16;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].MinLOD = 0.f;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].MaxLOD = D3D12_FLOAT32_MAX;
	StaticSamplers[BILINEAR_CLAMP_STATIC_SAMPLER].ShaderVisibility = visibility;

	range.type = RootSlotType::SAMPLER;
	range.baseRegister = 0;
	range.space = BILINEAR_CLAMP_STATIC_SAMPLER;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(ROOT_STATIC_SAMPLER);

	check(!ContainsSlot(RootSlotType::SAMPLER, 0, POINT_CLAMP_STATIC_SAMPLER, visibility));

	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].ShaderRegister = 0;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].RegisterSpace = POINT_CLAMP_STATIC_SAMPLER;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].MipLODBias = 0;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].MaxAnisotropy = 16;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].MinLOD = 0.f;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].MaxLOD = D3D12_FLOAT32_MAX;
	StaticSamplers[POINT_CLAMP_STATIC_SAMPLER].ShaderVisibility = visibility;

	range.type = RootSlotType::SAMPLER;
	range.baseRegister = 0;
	range.space = POINT_CLAMP_STATIC_SAMPLER;
	range.visibility = visibility;
	range.len = 1;
	Slots[range] = BindDesc_t(ROOT_STATIC_SAMPLER);
}

void FRootSignature::SerializeAndCreate() {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

	u64 rootHash = Flags;
	rootHash = MurmurHash2_64(rootSignatureDesc.pStaticSamplers, sizeof(rootSignatureDesc.pStaticSamplers[0]) * rootSignatureDesc.NumStaticSamplers, rootHash);

	u32 paramIndex = 0;
	for (auto &param : Params) {
		rootHash = MurmurHash2_64(&param, sizeof(param), rootHash);

		if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			param.DescriptorTable.pDescriptorRanges = Ranges.data() + ParamRangesOffset[paramIndex];

			rootHash = MurmurHash2_64(Ranges.data() + ParamRangesOffset[paramIndex], sizeof(Ranges[0]) * param.DescriptorTable.NumDescriptorRanges, rootHash);
		}
		++paramIndex;
	}

	rootSignatureDesc.pParameters = Params.data();
	rootSignatureDesc.NumParameters = (u32) Params.size();
	rootSignatureDesc.pStaticSamplers = StaticSamplers.data();
	rootSignatureDesc.NumStaticSamplers = (u32) StaticSamplers.size();
	rootSignatureDesc.Flags = Flags;

	ValueHash = rootHash;

	unique_com_ptr<ID3DBlob> Blob;
	unique_com_ptr<ID3DBlob> ErrorsBlob;
	auto hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, Blob.get_init(), ErrorsBlob.get_init());
	if (ErrorsBlob.get() && ErrorsBlob->GetBufferPointer()) {
		PrintFormated(L"Root serialization errors: %s\n", ConvertToWString((const char*)ErrorsBlob->GetBufferPointer(), ErrorsBlob->GetBufferSize()).c_str());
	}
	VERIFYDX12(hr);
	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_PPV_ARGS(D12RootSignature.get_init())));
}

extern u64 GShadersCompilationVersion;

FShaderState::FShaderState(FShader * inVertexShader, FShader * inPixelShader, FRootSignature * inRootSignature) :
	VertexShader(inVertexShader),
	PixelShader(inPixelShader),
	RootSignature(inRootSignature),
	Root(nullptr)
{
	FixedRootSignature = inRootSignature != nullptr;

	ContentHash = VertexShader->LocationHash;
	if (PixelShader) {
		ContentHash = HashCombine64(ContentHash, PixelShader->LocationHash);
	}
	if (FixedRootSignature) {
		ContentHash = HashCombine64(ContentHash, RootSignature->ValueHash);
	}
}

void FShaderState::Compile() {
	Root = GetRootLayout(VertexShader, PixelShader, RootSignature);
	if (!FixedRootSignature) {
		RootSignature = Root->RootSignature;
	}
	InitParams();
	ShadersCompilationVersion = GShadersCompilationVersion;
}

bool FShaderState::IsOutdated() const {
	return VertexShader->LastChangedVersion > ShadersCompilationVersion || PixelShader->LastChangedVersion > ShadersCompilationVersion || (Root == nullptr);
}

bool FPipelineState::IsOutdated() const {
	return ShaderState->VertexShader->LastChangedVersion > ShadersCompilationVersion || ShaderState->PixelShader->LastChangedVersion > ShadersCompilationVersion;
}

void FPipelineState::Compile() {
	if (ShaderState->IsOutdated()) {
		ShaderState->Compile();
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateDesc = Desc;

	CreateDesc.VS = GetBytecode(ShaderState->VertexShader);
	CreateDesc.PS = GetBytecode(ShaderState->PixelShader);
	CreateDesc.pRootSignature = ShaderState->RootSignature->D12RootSignature.get();
	CreateDesc.InputLayout.pInputElementDescs = InputLayout->ElementsNum > 0 ? InputLayout->Elements.get() : nullptr;
	CreateDesc.InputLayout.NumElements = InputLayout->ElementsNum;

	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateGraphicsPipelineState(&CreateDesc, IID_PPV_ARGS(D12PipelineState.get_init())));

	unique_com_ptr<ID3DBlob> Blob;
	VERIFYDX12(D12PipelineState->GetCachedBlob(Blob.get_init()));
	BlobVersion = MurmurHash2_64(Blob->GetBufferPointer(), Blob->GetBufferSize(), 0);

	ShadersCompilationVersion = GShadersCompilationVersion;
}

eastl::hash_map<u64, eastl::unique_ptr<FPipelineState>> PipelineLookup;

FPipelineState*			GetGraphicsPipelineState(FShaderState * ShaderState, D3D12_GRAPHICS_PIPELINE_STATE_DESC const *Desc, FInputLayout const * InputLayout) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC lookupDesc = *Desc;
	lookupDesc.VS = {};
	lookupDesc.PS = {};
	lookupDesc.pRootSignature = {};
	lookupDesc.InputLayout = {};

	u64 pipelineHash = MurmurHash2_64(&lookupDesc, sizeof(lookupDesc), 0);
	pipelineHash = HashCombine64(pipelineHash, ShaderState->ContentHash);
	if (InputLayout) {
		pipelineHash = HashCombine64(pipelineHash, InputLayout->ValueHash);
	}

	auto iter = PipelineLookup.find(pipelineHash);
	if (iter != PipelineLookup.end()) {
		return iter->second.get();
	}

	FPipelineState* pipeline = new FPipelineState();
	pipeline->ShaderState = ShaderState;
	pipeline->InputLayout = InputLayout;
	pipeline->Desc = lookupDesc;

	*PipelineLookup[pipelineHash].get_init() = pipeline;

	pipeline->Compile();

	return pipeline;
}

u32		GetPSOsNum() {
	return (u32)PipelineLookup.size();
}

void	RecompileChangedPipelines() {
	for (auto & PipelineEntry : PipelineLookup) {
		if (PipelineEntry.second->IsOutdated()) {
			PipelineEntry.second->Compile();
		}
	}
}

eastl::array<eastl::unique_ptr<FRootSignature>, 5> BasicRootSignatures;

void InitBasicRootSignatures() {
	if (BasicRootSignatures[0].get()) {
		return;
	}

	FRootSignature* root;

	// 0 is thin for postprocesses
	BasicRootSignatures[0] = eastl::make_unique<FRootSignature>();
	root = BasicRootSignatures[0].get();
	root->InitDefault(D3D12_SHADER_VISIBILITY_PIXEL);
	root->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_PIXEL);
	// t0-5 b0
	root->AddTableSRVRange(0, 6, 0);
	root->AddTableCBVRange(0, 1, 0);
	root->SerializeAndCreate();

	// 1 is this for postprocess with uavs
	BasicRootSignatures[1] = eastl::make_unique<FRootSignature>();
	root = BasicRootSignatures[1].get();
	root->InitDefault(D3D12_SHADER_VISIBILITY_PIXEL);
	// t0-3 u0-1 b0
	root->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_PIXEL);
	root->AddTableSRVRange(0, 4, 0);
	root->AddTableUAVRange(0, 2, 0);
	root->AddTableCBVRange(0, 1, 0);
	root->SerializeAndCreate();

	// 2 is for vertex-pixel shader work
	BasicRootSignatures[2] = eastl::make_unique<FRootSignature>();
	root = BasicRootSignatures[2].get();
	root->InitDefault(D3D12_SHADER_VISIBILITY_PIXEL);
	// b1
	root->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_VERTEX);
	root->AddTableCBVRange(1, 1, 0);
	// t0-6 b1
	root->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_PIXEL);
	root->AddTableSRVRange(0, 7, 0);
	root->AddTableCBVRange(1, 1, 0);
	// b0
	root->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_ALL);
	root->AddTableCBVRange(0, 1, 0);
	root->SerializeAndCreate();

	// 3 is for fatter vertex-pixel shader work
	BasicRootSignatures[3] = eastl::make_unique<FRootSignature>();
	root = BasicRootSignatures[3].get();
	root->InitDefault(D3D12_SHADER_VISIBILITY_ALL);
	// t0-6 u0-1 b2-3
	root->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_PIXEL);
	root->AddTableSRVRange(0, 7, 0);
	root->AddTableUAVRange(0, 2, 0);
	root->AddTableCBVRange(1, 1, 0);
	root->AddTableCBVRange(3, 1, 0);
	// b1
	root->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_ALL);
	root->AddTableCBVRange(2, 1, 0);
	// b0
	root->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_ALL);
	root->AddTableCBVRange(0, 1, 0);
	root->SerializeAndCreate();

	// 4 is super fat fallback
	BasicRootSignatures[4] = eastl::make_unique<FRootSignature>();
	root = BasicRootSignatures[4].get();
	root->InitDefault(D3D12_SHADER_VISIBILITY_ALL);
	// t4-24 u0-7 b2-4
	root->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_PIXEL);
	root->AddTableSRVRange(4, 21, 0);
	root->AddTableUAVRange(0, 8, 0);
	root->AddTableCBVRange(2, 3, 0);
	// b1 t0-3
	root->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_ALL);
	root->AddTableSRVRange(0, 4, 0);
	root->AddTableCBVRange(1, 1, 0);
	// b0
	root->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_ALL);
	root->AddTableCBVRange(0, 1, 0);
	root->SerializeAndCreate();
}

struct BindingMetadata {
	SlotsRange		Slot;
	eastl::string	Name;
	u64				NameHash;
};

struct ConstantBufferMetadata {
	BindingMetadata			Binding;
	u32						Size;
	eastl::unique_ptr<u8[]>	DefaultValue;
};

struct ConstantMetadata {
	u32						Size;
	u32						Offset;
	eastl::string			Name;
	u64						NameHash;
	u64						ConstantBufferNameHash;
};

class FShaderBindings {
public:
	// key is hash of name
	eastl::hash_map<u64, ConstantBufferMetadata>	CBVs;
	eastl::hash_map<u64, BindingMetadata>			SRVs;
	eastl::hash_map<u64, BindingMetadata>			UAVs;
	eastl::hash_map<u64, BindingMetadata>			Samplers;
	eastl::hash_map<u64, ConstantMetadata>			Constants;

	void GatherShaderBindings(FShader const* shader, D3D12_SHADER_VISIBILITY visibility);
};

enum class BindingResourceType {
	UNKNOWN,
	ConstantBuffer,
	Texture,
	RWTexture,
	Sampler,
	ConstantVar,
};

u64 GetStringHash(eastl::string &str) {
	return MurmurHash2_64(str.data(), str.size(), 0);
}

u64 GetStringHash(const char* str) {
	return MurmurHash2_64(str, strlen(str), 0);
}

GlobalBindId CreateTextureGBID(u64 nameHash) {
	return GlobalBindId(MurmurHash2_64(&nameHash, sizeof(u64), (u64)BindingResourceType::Texture));
};

GlobalBindId CreateTextureGBID(const char * name) {
	return CreateTextureGBID(GetStringHash(name));
};

GlobalBindId CreateRWTextureGBID(u64 nameHash) {
	return GlobalBindId(MurmurHash2_64(&nameHash, sizeof(u64), (u64)BindingResourceType::RWTexture));
};

GlobalBindId CreateRWTextureGBID(const char * name) {
	return CreateRWTextureGBID(GetStringHash(name));
};

GlobalBindId CreateSamplerGBID(u64 nameHash) {
	return GlobalBindId(MurmurHash2_64(&nameHash, sizeof(u64), (u64)BindingResourceType::Sampler));
};

GlobalBindId CreateConstantBufferGBID(u64 nameHash) {
	return GlobalBindId(MurmurHash2_64(&nameHash, sizeof(u64), (u64)BindingResourceType::ConstantBuffer));
};

GlobalBindId CreateConstantBufferGBID(const char * name) {
	return CreateConstantBufferGBID(GetStringHash(name));
};

GlobalBindId CreateConstantGBID(u64 nameHash) {
	return GlobalBindId(MurmurHash2_64(&nameHash, sizeof(u64), (u64)BindingResourceType::ConstantVar));
};

GlobalBindId CreateConstantGBID(const char * name) {
	return CreateConstantGBID(GetStringHash(name));
};

#include "Resource.h"

void FGraphicsRootLayout::FillBindings(FShaderBindings* bindings) {
	InitNullDescriptors();

	u32 rootIndex = 0;
	
	RootParamsNum = (u32) RootSignature->Params.size();
	for (u32 rootIndex = 0; rootIndex < RootSignature->Params.size(); rootIndex++) {
		RootParams[rootIndex].TableLen = 0;

		auto& RawParam = RootSignature->Params[rootIndex];

		if (RawParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			u32 StartRange = RootSignature->ParamRangesOffset[rootIndex];
			u32 EndRange = StartRange + RawParam.DescriptorTable.NumDescriptorRanges;
			for (u32 rangeIndex = StartRange; rangeIndex < EndRange; ++rangeIndex) {
				RootParams[rootIndex].TableLen += RootSignature->Ranges[rangeIndex].NumDescriptors;

				for (u32 descIndex = 0; descIndex < RootSignature->Ranges[rangeIndex].NumDescriptors; descIndex++) {
					if (RootSignature->Ranges[rangeIndex].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
						RootParams[rootIndex].NullHandles.push_back(NULL_TEXTURE2D_VIEW);
					}
					else if (RootSignature->Ranges[rangeIndex].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
						RootParams[rootIndex].NullHandles.push_back(NULL_TEXTURE2D_UAV_VIEW);
					}
					else if (RootSignature->Ranges[rangeIndex].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) {
						RootParams[rootIndex].NullHandles.push_back(NULL_CBV_VIEW);
					}
				}
			}
		}

		RootParams[rootIndex].NullHandles.shrink_to_fit();
	}

	for (auto& cbv : bindings->CBVs) {
		auto globalId = CreateConstantBufferGBID(cbv.second.Binding.NameHash);
		auto iter = ConstantBuffers.insert(globalId).first;
		iter->second.Bind = RootSignature->GetSlotBinding(cbv.second.Binding.Slot.type, cbv.second.Binding.Slot.baseRegister, cbv.second.Binding.Slot.space, cbv.second.Binding.Slot.visibility);
		iter->second.Index = cbv.second.Binding.Slot.baseRegister;
		iter->second.Space = cbv.second.Binding.Slot.space;
		iter->second.Size = cbv.second.Size;
		iter->second.DefaultValue = std::move(cbv.second.DefaultValue);
	}

	for (auto& srv : bindings->SRVs) {
		auto globalId = CreateTextureGBID(srv.second.NameHash);
		auto iter = Textures.insert(globalId).first;
		iter->second = RootSignature->GetSlotBinding(srv.second.Slot.type, srv.second.Slot.baseRegister, srv.second.Slot.space, srv.second.Slot.visibility);
	}

	for (auto& uav : bindings->UAVs) {
		auto globalId = CreateRWTextureGBID(uav.second.NameHash);
		auto iter = RWTextures.insert(globalId).first;
		iter->second = RootSignature->GetSlotBinding(uav.second.Slot.type, uav.second.Slot.baseRegister, uav.second.Slot.space, uav.second.Slot.visibility);
	}

	for (auto& sampler : bindings->Samplers) {
		auto globalId = CreateSamplerGBID(sampler.second.NameHash);
		auto iter = Samplers.insert(globalId).first;
		iter->second = RootSignature->GetSlotBinding(sampler.second.Slot.type, sampler.second.Slot.baseRegister, sampler.second.Slot.space, sampler.second.Slot.visibility);
	}

	for (auto& constant : bindings->Constants) {
		auto globalId = GlobalBindId(MurmurHash2_64(&constant.second.NameHash, sizeof(u64), (u64)BindingResourceType::ConstantVar));
		auto iter = Constants.insert(globalId).first;
		iter->second.ConstantBuffer = GlobalBindId(MurmurHash2_64(&constant.second.ConstantBufferNameHash, sizeof(u64), (u64)BindingResourceType::ConstantBuffer));
		iter->second.BufferOffset = constant.second.Offset;
		iter->second.Size = constant.second.Size;
	}
}

FRootSignature*		GetRootSignature(FGraphicsRootLayout const* layout) {
	return layout->RootSignature;
}

void FShaderBindings::GatherShaderBindings(FShader const* shader, D3D12_SHADER_VISIBILITY visibility) {

	ID3D12ShaderReflection* shaderReflection;
	VERIFYDX12(D3DReflect(GetBytecode(shader).pShaderBytecode, GetBytecode(shader).BytecodeLength, IID_PPV_ARGS(&shaderReflection)));

	D3D12_SHADER_DESC shaderDesc;
	VERIFYDX12(shaderReflection->GetDesc(&shaderDesc));

	u32 constantBuffersNum = shaderDesc.ConstantBuffers;
	u32 resourcesNum = shaderDesc.BoundResources;

	for (u32 i = 0u; i < resourcesNum;++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		VERIFYDX12(shaderReflection->GetResourceBindingDesc(i, &bindDesc));
		// because of memory trashing from GetResourceBindingDesc :(
		ZeroMemory(
			(u8*)&bindDesc + offsetof(D3D12_SHADER_INPUT_BIND_DESC, uID) + sizeof(bindDesc.uID),
			sizeof(bindDesc) - offsetof(D3D12_SHADER_INPUT_BIND_DESC, uID) - sizeof(bindDesc.uID));

		u64 nameHash = GetStringHash(bindDesc.Name);

		SlotsRange range = {};
		range.baseRegister = bindDesc.BindPoint;
		range.space = bindDesc.Space;
		range.visibility = visibility;
		range.len = 1;

		auto UpdateEntry = [](decltype(SRVs) & Map, const char* name, u64 nameHash, SlotsRange& range) {
			auto iter = Map.find(nameHash);
			if (iter == Map.end()) {
				iter = Map.insert(nameHash).first;
				iter->second.Name = eastl::string(name);
				iter->second.Slot = range;
				iter->second.NameHash = nameHash;
			}
			else {
				// check hash name collision
				check(iter->second.Name == eastl::string(name));
				// we collidade between shaders, need shared visibility
				iter->second.Slot.visibility = D3D12_SHADER_VISIBILITY_ALL;
				// slots must match
				check(Intersects(iter->second.Slot, range));
			}
		};

		switch (bindDesc.Type) {
		case D3D_SIT_CBUFFER:
			{
				range.type = RootSlotType::CBV;
				auto iter = CBVs.find(nameHash);
				if (iter == CBVs.end()) {
					iter = CBVs.insert(nameHash).first;
					iter->second.Binding.Name = eastl::string(bindDesc.Name);
					iter->second.Binding.Slot = range;
					iter->second.Binding.NameHash = nameHash;
				}
				else {
					// check hash name collision
					check(iter->second.Binding.Name == eastl::string(bindDesc.Name));
					// we collidade between shaders, need shared visibility
					iter->second.Binding.Slot.visibility = D3D12_SHADER_VISIBILITY_ALL;
					// slots must match
					check(Intersects(iter->second.Binding.Slot, range));
				}
			}
			break;
		case D3D_SIT_SAMPLER:
			{
				range.type = RootSlotType::SAMPLER;
				UpdateEntry(Samplers, bindDesc.Name, nameHash, range);
			}
			break;
		case D3D_SIT_TEXTURE:
			{
				//bindDesc.Dimension;
				range.type = RootSlotType::SRV;
				UpdateEntry(SRVs, bindDesc.Name, nameHash, range);
			}
			break;
		case D3D_SIT_UAV_RWTYPED:
			{
				range.type = RootSlotType::UAV;
				UpdateEntry(UAVs, bindDesc.Name, nameHash, range);
			}
			break;
		}
	}

	for (u32 i = 0u; i < constantBuffersNum;++i) {
		auto cbReflection = shaderReflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC bufferDesc;
		VERIFYDX12(cbReflection->GetDesc(&bufferDesc));

		u64 nameHash = GetStringHash(bufferDesc.Name);

		auto cbIter = CBVs.find(nameHash);

		cbIter->second.DefaultValue = eastl::make_unique<u8[]>(bufferDesc.Size);
		cbIter->second.Size = bufferDesc.Size;
		u8* pDefaultMem = cbIter->second.DefaultValue.get();
		memset(pDefaultMem, 0, bufferDesc.Size);

		if (bufferDesc.Type == D3D_CT_CBUFFER) {
			u32 variablesNum = bufferDesc.Variables;

			for (u32 i = 0u; i < variablesNum; ++i) {
				auto variable = cbReflection->GetVariableByIndex(i);
				D3D12_SHADER_VARIABLE_DESC variableDesc;
				VERIFYDX12(variable->GetDesc(&variableDesc));

				auto constantNameHash = GetStringHash(variableDesc.Name);
				auto iter = Constants.find(constantNameHash);
				if (iter == Constants.end()) {
					auto inIter = Constants.insert(constantNameHash).first;
					inIter->second.Name = eastl::string(variableDesc.Name);
					inIter->second.Offset = variableDesc.StartOffset;
					inIter->second.Size = variableDesc.Size;
					inIter->second.NameHash = constantNameHash;
					inIter->second.ConstantBufferNameHash = nameHash;
				}
				else {
					// check string hash collision
					check(iter->second.Name == eastl::string(variableDesc.Name));
				}
				
				if (variableDesc.DefaultValue) {
					memcpy(pDefaultMem + variableDesc.StartOffset, variableDesc.DefaultValue, variableDesc.Size);
				}
			}
		}
	}
}

bool CanRunWithRootSignature(FShaderBindings* bindings, FRootSignature* signature) {
	for (auto& cbv : bindings->CBVs) {
		if (!signature->ContainsSlot(
			cbv.second.Binding.Slot.type, 
			cbv.second.Binding.Slot.baseRegister, 
			cbv.second.Binding.Slot.space, 
			cbv.second.Binding.Slot.visibility)) {
			return false;
		}
	}
	for (auto& srv : bindings->SRVs) {
		if (!signature->ContainsSlot(srv.second.Slot.type, srv.second.Slot.baseRegister, srv.second.Slot.space, srv.second.Slot.visibility)) {
			return false;
		}
	}
	for (auto& uav : bindings->UAVs) {
		if (!signature->ContainsSlot(uav.second.Slot.type, uav.second.Slot.baseRegister, uav.second.Slot.space, uav.second.Slot.visibility)) {
			return false;
		}
	}
	for (auto& sampler : bindings->Samplers) {
		if (!signature->ContainsSlot(sampler.second.Slot.type, sampler.second.Slot.baseRegister, sampler.second.Slot.space, sampler.second.Slot.visibility)) {
			return false;
		}
	}

	return true;
}

eastl::hash_map<u64, eastl::unique_ptr<FGraphicsRootLayout>> RootLayoutLookup;
#include "PointerMath.h"

FGraphicsRootLayout* GetRootLayout(FShader const* VS, FShader const* PS, FRootSignature * RootSignature) {
	u64 lookupKey = HashCombine64(GetShaderHash(VS), GetShaderHash(PS));
	auto iter = RootLayoutLookup.find(lookupKey);
	if (iter != RootLayoutLookup.end()) {
		return iter->second.get();
	}

	InitBasicRootSignatures();

	FShaderBindings bindings;
	bindings.GatherShaderBindings(VS, D3D12_SHADER_VISIBILITY_VERTEX);
	if (PS) {
		bindings.GatherShaderBindings(PS, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	if (RootSignature == nullptr) {
		for (auto& root : BasicRootSignatures) {
			if (CanRunWithRootSignature(&bindings, root.get())) {
				RootSignature = root.get();
				break;
			}
		}
		check(RootSignature);
	}
	else {
		check(CanRunWithRootSignature(&bindings, RootSignature));
	}

	FGraphicsRootLayout* layout = new FGraphicsRootLayout();
	RootLayoutLookup[lookupKey] = eastl::unique_ptr<FGraphicsRootLayout>(layout);

	layout->RootSignature = RootSignature;
	layout->FillBindings(&bindings);

	return layout;
}

ID3D12RootSignature*	GetRawRootSignature(FGraphicsRootLayout const* root) {
	return root->RootSignature->D12RootSignature.get();
}

ID3D12PipelineState*	GetRawPSO(FPipelineState const* pso) {
	return pso->D12PipelineState.get();
}

#include "Descriptors.h"

FTextureParam FGraphicsRootLayout::CreateTextureParam(char const * name) {
	FTextureParam Param = {};
	Param.BindId = CreateTextureGBID(name);
	return Param;
}

FRWTextureParam FGraphicsRootLayout::CreateRWTextureParam(char const * name) {
	FRWTextureParam Param = {};
	Param.BindId = CreateTextureGBID(name);
	return Param;
}

FConstantBuffer FGraphicsRootLayout::CreateConstantBuffer(char const * name) {
	FConstantBuffer Param = {};
	Param.BindId = CreateConstantBufferGBID(name);
	Param.Layout = this;
	Param.Size = ConstantBuffers[Param.BindId].Size;
	return Param;
}
