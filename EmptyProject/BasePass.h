#pragma once
#include "Essence.h"
#include "Frame.h"
#include "Viewport.h"
#include "Scene.h"

struct FForwardRenderContext : public FRenderViewport {
	float4x4 WorldToShadowmap;
	FGPUResource * Shadowmap;
	FGPUResource * ShadowmapM2;
};

void PreRender_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene);
void RenderModel_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene, FSceneStaticMesh * StaticMesh);

void Render_Forward(FCommandsStream & Commands, FForwardRenderContext * Viewport, FScene * Scene);