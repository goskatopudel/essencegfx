#include "MathGeometry.h"
#include <EASTL\algorithm.h>
#include "MathFunctions.h"

FBBox::FBBox(std::initializer_list<float3> Points) {
	*this = CreateInvalidBBox();
	for (float3 P : Points) {
		Inflate(P);
	}
}

void FBBox::Inflate(float3 Point) {
	VMin = min(VMin, Point);
	VMax = max(VMax, Point);
}

void FBBox::Inflate(FBBox const& BBox) {
	VMin = min(VMin, BBox.VMin);
	VMax = max(VMax, BBox.VMax);
}

float3 FBBox::GetCentroid() const {
	return VMin * 0.5f + VMax * 0.5f;
}

float3 FBBox::GetExtent() const {
	return VMax * 0.5f - VMin * 0.5f;
}

float FBBox::SurfaceArea() const {
	float3 E = VMax - VMin;
	return E.x * E.y * E.z;
}

FBBox CreateInvalidBBox() {
	return FBBox(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
}

bool Intersects(FRayInv const & Ray, FBBox const& BBox) {
	float3 T0 = (BBox.VMin - Ray.Origin) * Ray.InvDirection;
	float3 T1 = (BBox.VMax - Ray.Origin) * Ray.InvDirection;

	float3 TMin = min(T0, T1);
	float3 TMax = max(T0, T1);

	float TEnter = eastl::max(TMin.x, eastl::max(TMin.y, TMin.z));
	float TExit = eastl::min(TMax.x, eastl::min(TMax.y, TMax.z));

	return TExit > eastl::max(TEnter, 0);
}

float RayTriangleIntersection(FRay Ray, float3 p0, float3 p1, float3 p2) {
	float3 e1 = p1 - p0;
	float3 e2 = p2 - p0;

	float3 P = cross(Ray.Direction, e2);
	float det = dot(e1, P);

	const float EPSILON = 0.000001f;

	float3 T;
	if (det < 0) {
		return FINF;
	}

	T = Ray.Origin - p0;

	if (det < EPSILON) {
		return FINF;
	}

	float det_rcp = 1.f / det;
	float u = dot(T, P) * det_rcp;

	if (u < 0.f || u > 1.f) {
		return FINF;
	}

	float3 Q = cross(T, e1);
	float v = dot(Ray.Direction, Q) * det_rcp;

	if (v < 0.f || u + v  > 1.f) {
		return FINF;
	}

	float t = dot(e2, Q) * det_rcp;
	if (t > EPSILON) {
		return t;
	}

	return FINF;
}