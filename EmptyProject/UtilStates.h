#pragma once
#include "Pipeline.h"

class FCopyPSShaderState : public FShaderState {
public:
	FTextureParam			SourceTexture;

	FCopyPSShaderState();
	void InitParams() override final;
};

template<typename T>
T* GetInstance() {
	static T Instance;
	return &Instance;
}