#include "RenderMaterial.h"
#include "Pipeline.h"

eastl::unique_ptr<FRootSignature>	MaterialsRoot;

void CreateMaterialRoot() {
	MaterialsRoot = eastl::make_unique<FRootSignature>();
	MaterialsRoot->InitDefault(D3D12_SHADER_VISIBILITY_ALL);
	// b1
	MaterialsRoot->AddTableParam(PARAM_0, D3D12_SHADER_VISIBILITY_ALL);
	MaterialsRoot->AddTableCBVRange(2, 1, 0);
	// t0-4 b2
	MaterialsRoot->AddTableParam(PARAM_1, D3D12_SHADER_VISIBILITY_PIXEL);
	MaterialsRoot->AddTableSRVRange(0, 5, 0);
	MaterialsRoot->AddTableCBVRange(1, 1, 0);
	// b0
	MaterialsRoot->AddTableParam(PARAM_2, D3D12_SHADER_VISIBILITY_ALL);
	MaterialsRoot->AddTableCBVRange(0, 1, 0);
	MaterialsRoot->SerializeAndCreate();
}
