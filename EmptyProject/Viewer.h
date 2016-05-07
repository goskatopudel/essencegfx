#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"
#include "Viewport.h"

class FGPUResource;
class FCamera;
class FGPUContext;

enum class EViewMode {
	Default,
	Normals,
	Texcoord0,
	Texcoord1
};

struct FViewParams {
	EViewMode	Mode;
	bool		DrawNormals;
	bool		Wireframe;
	bool		DrawAtlas;
};

struct FTransformation {
	float3		Position;
	float		Scale;
};

class FEditorModel;

void RenderModel(FGPUContext &, FEditorModel *, FTransformation, FRenderViewport const&, FViewParams const&);