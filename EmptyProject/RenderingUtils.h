#pragma once
#include "Essence.h"
#include "MathVector.h"
class FCommandsStream;
class FGPUResource;
struct FRenderTargetContext;

enum class ETextureFiltering {
	Point,
	Bilinear
};

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, ETextureFiltering Filtering, u32 Mipmap, FRenderTargetContext & RTContext);
void GenerateMipmaps(FCommandsStream & Context, FGPUResource * Texture);

void BlurTexture(FCommandsStream & Context, FGPUResource * SrcTexture, FGPUResource * OutTexture);