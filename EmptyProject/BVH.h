#pragma once
#include "Essence.h"
#include "MathMatrix.h"
#include "MathGeometry.h"
class FEditorMesh;
class FGPUContext;
struct FRenderViewport;

struct FBVHNode {
	FBBox	Bounds;
	u32		SecondChild;
	u32		PrimitivesNum;
	u32		PrimitivesOffset;
	u32		SplitAxis;
};

class FLinearBVH {
public:
	eastl::vector<FBVHNode>	Nodes;
	eastl::vector<u32>		Primitives;
	float3 *				Positions;
	u32 *					Indices;

	u32 GetDepth() const;
	bool CastRay(FRay const& Ray, float &MinT, u32 &PrimitiveId);
	bool CastShadowRay(FRay const& Ray);
};

void BuildBVH(FEditorMesh * Mesh, FLinearBVH * BVH);