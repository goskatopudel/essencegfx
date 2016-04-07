#pragma once
#include "Essence.h"
#include "Device.h"
#include "PointerMath.h"

enum class SlotEnum {
	TEXTURE2D,
	TEXTURE2DMS
};

enum class ProcessEnum {
	GRAPHICS,
	COMPUTE
};

struct SetPSOCommand {
	struct Data {
		ID3D12PipelineState* PSO;
	};

	u64 GetSize() {
		return sizeof(Data) + sizeof(void*);
	}
	
	void* Execute(ID3D12GraphicsCommandList* commandList, void* pData) {
		commandList->SetPipelineState(((Data*)pData)->PSO);
		return pointer_add(pData, GetSize());
	}
};
