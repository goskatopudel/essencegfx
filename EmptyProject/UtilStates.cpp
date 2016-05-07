#include "UtilStates.h"
#include "Shader.h"

FCopyPSShaderState::FCopyPSShaderState() :
	FShaderState(
		GetShader("Shaders/Utility.hlsl", "VShader", "vs_5_0", {}, 0),
		GetShader("Shaders/Utility.hlsl", "CopyPS", "ps_5_0", {}, 0)) {}

void FCopyPSShaderState::InitParams() {
	SourceTexture = Root->CreateTextureParam("Image");
}