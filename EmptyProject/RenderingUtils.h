#pragma once
#include "Essence.h"
#include "MathVector.h"
class FCommandsStream;
class FGPUResource;
struct FRenderTargetContext;

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, u32 Mipmap, FRenderTargetContext & RTContext);
void GenerateMipmaps(FCommandsStream & Context, FGPUResource * Texture);