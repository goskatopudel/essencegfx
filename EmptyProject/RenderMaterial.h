#pragma once
#include "Essence.h"

class FShader;

enum class FRenderMaterialPasses {
	GBuffer,
	DepthBuffer,
	ShadowMap,
	Count
};

struct FRenderMaterialSortKey {
	u64   Alphamasked : 1;
};

class FRenderMaterial {
public:	
	struct FRenderPassData {
		FRenderMaterialSortKey		SortKey;
		FShader *					VS;
		FShader *					PS;
	} PassData[(i32)FRenderMaterialPasses::Count];

	// allocate table with pass data
};
