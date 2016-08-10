#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"
#include <EASTL\array.h>

class FGPUResource;
class FCamera;
struct FRenderViewport;

void UpdateViewport(FRenderViewport &Viewport, FCamera * Camera, Vec2i Resolution, float FovY = M_PI_4, float NearPlane = 0.1f, float FarPlane = 1000.f);
void UpdateShadowmapViewport(FRenderViewport &Viewport, Vec2i Resolution, float3 Direction);

#include "Resource.h"

struct FRenderTargetDesc {
	FGPUResource* Resource;
	u8 OutputSRGB;

	DXGI_FORMAT GetFormat() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
};

struct FRenderTargetsBundle {
	typedef eastl::vector<FRenderTargetDesc> RTArray;
	RTArray Outputs;
	FGPUResource * DepthBuffer = nullptr;
	D3D12_VIEWPORT Viewport;
	u8 SamplesNum;
};

struct FRenderViewport {
	FRenderTargetsBundle RenderTargets;
	Vec2i Resolution;
	FCamera * Camera;
	float4x4 ViewMatrix;
	float4x4 InvViewMatrix;
	float4x4 ProjectionMatrix;
	float4x4 InvProjectionMatrix;
	float4x4 ViewProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
};


