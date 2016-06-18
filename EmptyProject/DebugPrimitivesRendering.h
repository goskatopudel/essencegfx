#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"
#include <EASTL\vector.h>
#include "Viewport.h"

float4 LinearToSRGB(float4 Color);
Color4b ToColor4b(float4 Color, bool ConvertToSRGB);

struct FDebugVertex {
	float3	Position;
	Color4b Color;
};

enum class EPrimitive {
	Line,
	Polygon
};

struct FBBox;
class FEditorMesh;
class FGPUContext;

struct FPrettyColorFactory {
	float	R;
	FPrettyColorFactory(float Random);
	Color4b GetNext(float Sat = 0.99f, float Val = 0.99f, bool bSRGB = true);
};

class FDebugPrimitivesAccumulator {
public:
	eastl::vector<FDebugVertex>		Vertices;

	struct FBatch {
		EPrimitive	Type;
		u32			Num;
	};

	eastl::vector<FBatch>			Batches;

	// basic shapes
	void AddLine(float3 P0, float3 P1, Color4b Color);
	void AddPolygon(float3 P0, float3 P1, float3 P2, Color4b Color);

	// helpers
	void AddBBox(FBBox const& BBox, Color4b Color);
	void AddMeshWireframe(FEditorMesh * Mesh, Color4b Color);
	void AddMeshPolygons(FEditorMesh * Mesh, Color4b Color);
	void AddMeshNormals(FEditorMesh * Mesh, float Scale = 1.f, Color4b Color = Color4b(255, 0, 0, 255));

	// draw
	void FlushToViewport(FGPUContext & Context, FRenderViewport const& Viewport);
};