#pragma once
#include "Essence.h"
#include "MathVector.h"


class FGPUResource;
class FCamera;
class FGPUContext;

enum class EViewMode {
	Default,
	Normals
};

struct FRenderViewport {
	FGPUResource *	RenderTarget;
	FGPUResource *	DepthBuffer;
	Vec2i			Resolution;
	FCamera *		Camera;
};

struct FViewParams {
	EViewMode	Mode;
	bool		DrawNormals;
	bool		Wireframe;
};

struct FTransformation {
	float3		Position;
	float		Scale;
};

class FEditorModel;

void RenderModel(FGPUContext &, FEditorModel *, FTransformation, FRenderViewport const&, FViewParams const&);