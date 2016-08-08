#pragma once
#include "Essence.h"
#include "Frame.h"
#include "Viewport.h"

class FSceneStaticMesh;

struct FDepthRenderContext : public FRenderViewport {
	bool OutputVSM = false;
};

void PreRender_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene);
void RenderModel_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene, FSceneStaticMesh * StaticMesh);

void Render_Depth(FCommandsStream & Commands, FDepthRenderContext * Viewport, FScene * Scene);