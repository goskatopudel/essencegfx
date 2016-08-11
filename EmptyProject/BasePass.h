#pragma once
#include "Essence.h"
#include "Frame.h"
#include "Viewport.h"
#include "Scene.h"
#include "Rendering.h"

struct FForwardRenderContext : public FRenderViewport {
	float4x4 WorldToShadowmap;
	FGPUResource * Shadowmap;
	FGPUResource * ShadowmapM2;
};

void PreRender_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene);
void RenderModel_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene, FSceneStaticMesh * StaticMesh);

void Render_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene);

struct FGBufferRenderContext : public FRenderViewport {
};

void PreRender_GBuffer(FCommandsStream & Commands, FGBufferRenderContext * Viewport, FSceneRenderingFrame * Scene);
void RenderModel_GBuffer(FCommandsStream & Commands, FGBufferRenderContext * Viewport, FSceneRenderingFrame * Scene, FSceneStaticMesh * StaticMesh);

void Render_GBuffer(FCommandsStream & Commands, FGBufferRenderContext * Viewport, FSceneRenderingFrame * Scene);

#include "Pipeline.h"

class FStaticModelShaderState_GBuffer : public FShaderState {
public:
	FConstantBuffer FrameCB;
	FConstantBuffer ObjectCB;

	FStaticModelShaderState_GBuffer();
	void InitParams() override final;
};