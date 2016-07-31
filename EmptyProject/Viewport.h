#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"
#include <EASTL\array.h>

class FGPUResource;
class FCamera;

struct FRenderViewport {
	Vec2i Resolution;
	float3 CameraPosition;
	float3 CameraUp;
	float3 CameraDirection;
	float4x4 ViewMatrix;
	float4x4 InvViewMatrix;
	float4x4 ProjectionMatrix;
	float4x4 InvProjectionMatrix;
	float4x4 ViewProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
	float4x4 TViewMatrix;
	float4x4 TInvViewMatrix;
	float4x4 TProjectionMatrix;
	float4x4 TInvProjectionMatrix;
	float4x4 TViewProjectionMatrix;
	float4x4 TInvViewProjectionMatrix;
};

void GenerateViewport(FRenderViewport &Viewport, FCamera * Camera, Vec2i Resolution, float FovY = M_PI_4, float NearPlane = 0.1f, float FarPlane = 1000.f);
void GenerateShadowmapViewport(FRenderViewport &Viewport, Vec2i Resolution, float3 Direction);

#include "Resource.h"

struct FRenderTargetDesc {
	FGPUResource* Resource;
	u8 OutputSRGB;

	DXGI_FORMAT GetFormat() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
};

template<u32 RenderTargetsNum>
struct FRenderTargetContext {
	typedef eastl::array<FRenderTargetDesc, RenderTargetsNum> RTArray;
	RTArray RenderTargets;
	FGPUResource * DepthBuffer = nullptr;
	FRenderViewport Viewport;
};

typedef FRenderTargetContext<0> FRT0Context;
typedef FRenderTargetContext<1> FRT1Context;
