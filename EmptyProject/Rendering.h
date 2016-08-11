#pragma once
#include "Essence.h"
#include "Device.h"
#include "MathVector.h"
#include "MathMatrix.h"

class FScene;
class FCamera;
class FCommandsStream;

struct FSceneRenderingFrame {
	u64 FrameNum;
	FScene * Scene;
	FCamera * Camera;
	Vec2u Resolution;

	FGPUResource * DepthBuffer;
	FGPUResource * GBuffer0;
	FGPUResource * GBuffer1;

	float4x4 PrevViewProjection;
	Vec2u PrevResolution;

	D3D12_CPU_DESCRIPTOR_HANDLE FrameCBV;
};

void AllocateGBuffer();
void RenderScene(FCommandsStream & Commands, FScene * Scene, FCamera * Camera, FSceneRenderingFrame * PrevFrame);