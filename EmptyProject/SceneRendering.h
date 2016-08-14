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
	float4x4 ViewProjection;

	FGPUResource * DepthBuffer;
	FGPUResource * GBuffer0;
	FGPUResource * GBuffer1;

	Vec2u PrevResolution;
	float4x4 PrevViewProjection;

	D3D12_CPU_DESCRIPTOR_HANDLE FrameCBV;
};

void AllocateGBuffer();
void RenderScene(FCommandsStream & Commands, FScene * Scene, FCamera * Camera, FSceneRenderingFrame & PrevFrame);

enum class EGBufferView {
	Albedo,
	Normals,
	Depth,
	MotionVectors
};

void VisualizeGBufferDebug(FCommandsStream & Commands, EGBufferView Visualize, FSceneRenderingFrame * Frame);