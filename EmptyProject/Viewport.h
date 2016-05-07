#pragma once
#include "Essence.h"
#include "MathVector.h"

class FGPUResource;
class FCamera;

struct FRenderViewport {
	u32				OutputSRGB : 1;
	FGPUResource *	RenderTarget;
	FGPUResource *	DepthBuffer;
	Vec2i			Resolution;
	FCamera *		Camera;
	float4x4		ViewProjectionMatrix;
	float4x4		InvViewProjectionMatrix;
	float4x4		TViewProjectionMatrix;
	float4x4		TInvViewProjectionMatrix;
};