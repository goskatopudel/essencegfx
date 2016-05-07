#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathFunctions.h"

#include <limits>

const float FINF = std::numeric_limits<float>::infinity();

struct FRay {
	float3	Origin;
	float3	Direction;

	FRay() = default;
	FRay(float3 inOrigin, float3 inDirection) : Origin(inOrigin), Direction(inDirection) {}
};

struct FRayInv {
	float3	Origin;
	float3	InvDirection;

	FRayInv() = default;
	FRayInv(float3 inOrigin, float3 inInvDirection) : Origin(inOrigin), InvDirection(inInvDirection) {}

	explicit FRayInv(FRay const & Ray) : FRayInv(Ray.Origin, 1.f / Ray.Direction) {}
};

struct FBBox {
	float3	VMin;
	float3	VMax;

	FBBox() = default;
	FBBox(float3 inVMin, float3 inVMax) : VMin(inVMin), VMax(inVMax) {}
	FBBox(std::initializer_list<float3> Points);

	void Inflate(float3 Point);
	void Inflate(FBBox const& BBox);
	float3 GetCentroid() const;
	float3 GetExtent() const;
	float SurfaceArea() const;
};

struct FBBoxCentroid {
	float3 Centroid;
	float3 Extent;

	FBBoxCentroid() = default;
	FBBoxCentroid(float3 inCentorid, float3 inExtent) : Centroid(inCentorid), Extent(inExtent) {}
	explicit FBBoxCentroid(FBBox const & BBox) : FBBoxCentroid(BBox.GetCentroid(), BBox.GetExtent()) {}
};

FBBox CreateInvalidBBox();
FBBox Union(FBBox const& A, FBBox const& B);
bool Intersects(FRayInv const & Ray, FBBox const& BBox);
float RayTriangleIntersection(FRay Ray, float3 p0, float3 p1, float3 p2);