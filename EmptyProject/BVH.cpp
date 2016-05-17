#include "Essence.h"
#include "MathVector.h"
#include "MathFunctions.h"
#include <EASTL\vector.h>
#include <EASTL\sort.h>
#include "BVH.h"
#include "AssertionMacros.h"

u32 FLinearBVH::GetDepth() const {
	struct FStackElem {
		u32 Node;
		u32 Depth;
	};

	FStackElem Stack[64];
	i32 StackIndex = -1;

	u32 Index = 0;
	u32 Depth = 1;

	u32 MaxDepth = 1;

	while (1) {
		if (Nodes[Index].PrimitivesNum) {
			Depth--;

			if (StackIndex >= 0) {
				Index = Stack[StackIndex].Node;
				Depth = Stack[StackIndex].Depth;
				StackIndex--;
			}
			else {
				break;
			}
		}
		else {
			StackIndex++;
			Stack[StackIndex].Node = Nodes[Index].SecondChild;
			Stack[StackIndex].Depth = Depth + 1;

			Depth++;
			Index = Index + 1;

			MaxDepth = eastl::max(Depth, MaxDepth);
		}
	}

	return MaxDepth;
}

bool FLinearBVH::CastRay(FRay const& Ray, float &MinT, u32 &PrimitiveId) {
	FRayInv RayInv = FRayInv(Ray);
	bool DirIsNeg[3] = { Ray.Direction.x < 0, Ray.Direction.y < 0, Ray.Direction.z < 0 };

	u32 Stack[64];
	i32 StackIndex = -1;
	bool bHit = false;

	u32 Index = 0;
	while (1) {

		if (Intersects(RayInv, Nodes[Index].Bounds)) {

			u32 PrimitivesNum = Nodes[Index].PrimitivesNum;
			if (PrimitivesNum) {
				for (u32 PolygonIndex = 0; PolygonIndex < PrimitivesNum; ++PolygonIndex) {
					u32 Primitive = Primitives[Nodes[Index].PrimitivesOffset + PolygonIndex];
					float3 P0 = Positions[Indices[Primitive * 3]];
					float3 P1 = Positions[Indices[Primitive * 3 + 1]];
					float3 P2 = Positions[Indices[Primitive * 3 + 2]];

					float t = RayTriangleIntersection(Ray, P0, P1, P2);
					if (t < MinT) {
						MinT = t;
						PrimitiveId = Primitive;
						bHit = true;
					}
				}

				if (StackIndex >= 0) {
					Index = Stack[StackIndex];
					StackIndex--;
				}
				else {
					break;
				}
			}
			else {
				if (DirIsNeg[Nodes[Index].SplitAxis]) {
					++StackIndex;
					Stack[StackIndex] = Nodes[Index].SecondChild;

					Index = Index + 1;
				}
				else {
					++StackIndex;
					Stack[StackIndex] = Index + 1;

					Index = Nodes[Index].SecondChild;
				}
			}
		}
		else {
			if (StackIndex >= 0) {
				Index = Stack[StackIndex];
				StackIndex--;
			}
			else {
				break;
			}
		}
	}

	return bHit;
}

bool FLinearBVH::CastShadowRay(FRay const& Ray) {
	FRayInv RayInv = FRayInv(Ray);
	bool DirIsNeg[3] = { Ray.Direction.x < 0, Ray.Direction.y < 0, Ray.Direction.z < 0 };

	u32 Stack[64];
	i32 StackIndex = -1;

	u32 Index = 0;
	while (1) {
		if (Intersects(RayInv, Nodes[Index].Bounds)) {

			u32 PrimitivesNum = Nodes[Index].PrimitivesNum;
			if (PrimitivesNum) {
				for (u32 PolygonIndex = 0; PolygonIndex < PrimitivesNum; ++PolygonIndex) {
					u32 Primitive = Primitives[Nodes[Index].PrimitivesOffset + PolygonIndex];
					float3 P0 = Positions[Indices[Primitive * 3]];
					float3 P1 = Positions[Indices[Primitive * 3 + 1]];
					float3 P2 = Positions[Indices[Primitive * 3 + 2]];

					if (RayTriangleIntersection(Ray, P0, P1, P2) != FINF) {
						return true;
					}
				}

				if (StackIndex >= 0) {
					Index = Stack[StackIndex];
					StackIndex--;
				}
				else {
					break;
				}
			}
			else {
				if (DirIsNeg[Nodes[Index].SplitAxis]) {
					++StackIndex;
					Stack[StackIndex] = Nodes[Index].SecondChild;

					Index = Index + 1;
				}
				else {
					++StackIndex;
					Stack[StackIndex] = Index + 1;

					Index = Nodes[Index].SecondChild;
				}
			}
		}
		else {
			if (StackIndex >= 0) {
				Index = Stack[StackIndex];
				StackIndex--;
			}
			else {
				break;
			}
		}
	}

	return false;
}

enum class EBVHSplitMethod {
	EQUAL_COUNT,
	SAH
};

class FBVHBuilder {
public:
	struct FPrimitiveInfo {
		FBBox	BBox;
		u32		Index;
	};

	struct FNode {
		FBBox	Bounds;
		u32		Children[2];
		u32		FirstPrimitiveOffset;
		u32		PrimitivesNum;
		u32		SplitAxis;
	};

	eastl::vector<FNode>			Nodes;
	eastl::vector<FPrimitiveInfo>	Primitives;
	eastl::vector<FPrimitiveInfo>	LeafPrimitives;

	u32 CreateNode() {
		Nodes.push_back({});
		return (u32)Nodes.size() - 1;
	}

	void InitLeafNode(u32 Node) {
		Nodes[Node].Children[0] = Nodes[Node].Children[1] = 0xFFFFFFFF;
	}

	u32 RecursiveBuild(u32 Begin, u32 End) {
		check(End > Begin);

		FBBox Bounds = Primitives[Begin].BBox;
		for (u32 Index = Begin + 1; Index < End; ++Index) {
			Bounds.Inflate(Primitives[Index].BBox);
		}

		u32 Node = CreateNode();
		Nodes[Node].Bounds = Bounds;

		if (End - Begin == 1) {
			InitLeafNode(Node);
			Nodes[Node].FirstPrimitiveOffset = (u32)LeafPrimitives.size();
			Nodes[Node].PrimitivesNum = 1;
			LeafPrimitives.push_back(Primitives[Begin]);
		}
		else {
			FBBox CentroidBounds = CreateInvalidBBox();
			for (u32 Index = Begin; Index < End; ++Index) {
				CentroidBounds.Inflate(Primitives[Index].BBox.GetCentroid());
			}
			float3 CentroidsExtent = CentroidBounds.GetExtent();
			float MaxExtent = eastl::max(CentroidsExtent.x, eastl::max(CentroidsExtent.y, CentroidsExtent.z));
			u32 SplitAxis = 0;
			if (MaxExtent == 0) {
				InitLeafNode(Node);
				Nodes[Node].FirstPrimitiveOffset = (u32)LeafPrimitives.size();
				Nodes[Node].PrimitivesNum = End - Begin;

				for (u32 Index = Begin; Index < End; ++Index) {
					LeafPrimitives.push_back(Primitives[Index]);
				}
			}
			else if (CentroidsExtent.y == MaxExtent) {
				SplitAxis = 1;
			}
			else if (CentroidsExtent.z == MaxExtent) {
				SplitAxis = 2;
			}

			EBVHSplitMethod SplitMethod = EBVHSplitMethod::SAH;

			u32 PrimitivesNum = End - Begin;
			if (PrimitivesNum <= 4) {
				SplitMethod = EBVHSplitMethod::EQUAL_COUNT;
			}

			// compare to mid
			u32 SplitIndex;
			if (SplitMethod == EBVHSplitMethod::SAH) {
				const float RAY_BBOX_RATIO = 0.25f;
				const float RAY_TRI_RATIO = 1.f;

				const u32 BucketsNum = 16;
				struct FBucket {
					u32		Count;
					FBBox	Bounds;
				};
				FBucket Buckets[BucketsNum];

				float RangeLen = CentroidBounds.VMax[SplitAxis] - CentroidBounds.VMin[SplitAxis];
				float Cost[BucketsNum];

				for (u32 Index = 0; Index < BucketsNum; ++Index) {
					Buckets[Index].Count = 0;
					Buckets[Index].Bounds = CreateInvalidBBox();
					Cost[Index] = 0;
				}

				for (u32 Index = Begin; Index < End; ++Index) {
					u32 B = (u32)(BucketsNum * (Primitives[Index].BBox.GetCentroid()[SplitAxis] - CentroidBounds.VMin[SplitAxis]) / RangeLen);
					B = B >= BucketsNum ? BucketsNum - 1 : B;
					Buckets[B].Count++;
					Buckets[B].Bounds.Inflate(Primitives[Index].BBox);
				}

				for (u32 SplitIndex = 0; SplitIndex < BucketsNum - 1; ++SplitIndex) {
					FBBox B0 = CreateInvalidBBox();
					FBBox B1 = CreateInvalidBBox();
					u32 Count0 = 0;
					u32 Count1 = 0;
					for (u32 i = 0; i <= SplitIndex; ++i) {
						B0.Inflate(Buckets[i].Bounds);
						Count0 += Buckets[i].Count;
					}
					for (u32 i = SplitIndex + 1; i < BucketsNum; ++i) {
						B1.Inflate(Buckets[i].Bounds);
						Count1 += Buckets[i].Count;
					}

					Cost[SplitIndex] = RAY_BBOX_RATIO + RAY_TRI_RATIO * (Count0 * B0.SurfaceArea() + Count1 * B1.SurfaceArea()) / Bounds.SurfaceArea();
				}

				float MinCost = Cost[0];
				u32 SplitBucket = 0;

				for (u32 Index = 1; Index < BucketsNum - 1; ++Index) {
					if (Cost[Index] < MinCost) {
						MinCost = Cost[Index];
						SplitBucket = Index;
					}
				}

				if (MinCost < RAY_TRI_RATIO * PrimitivesNum) {
					FPrimitiveInfo * Mid = eastl::partition(Primitives.data() + Begin, Primitives.data() + End,
						[BucketsNum, SplitBucket, SplitAxis, CentroidBounds, RangeLen](FPrimitiveInfo const& A) {
						u32 B = (u32)(BucketsNum * (A.BBox.GetCentroid()[SplitAxis] - CentroidBounds.VMin[SplitAxis]) / RangeLen);
						return B <= SplitBucket;
					});

					SplitIndex = (u32)(Mid - Primitives.data());
					check(SplitIndex > Begin);
					check(SplitIndex < End);

					Nodes[Node].SplitAxis = SplitAxis;
					Nodes[Node].Children[0] = RecursiveBuild(Begin, SplitIndex);
					Nodes[Node].Children[1] = RecursiveBuild(SplitIndex, End);
				}
				else {
					InitLeafNode(Node);
					Nodes[Node].FirstPrimitiveOffset = (u32)LeafPrimitives.size();
					Nodes[Node].PrimitivesNum = End - Begin;

					for (u32 Index = Begin; Index < End; ++Index) {
						LeafPrimitives.push_back(Primitives[Index]);
					}
				}
			}
			else if (SplitMethod == EBVHSplitMethod::EQUAL_COUNT) {
				SplitIndex = (End - Begin) / 2 + Begin;
				eastl::nth_element(Primitives.data() + Begin, Primitives.data() + SplitIndex, Primitives.data() + End,
					[SplitAxis](FPrimitiveInfo const& A, FPrimitiveInfo const& B) { return A.BBox.GetCentroid()[SplitAxis] < B.BBox.GetCentroid()[SplitAxis]; });

				Nodes[Node].SplitAxis = SplitAxis;
				Nodes[Node].Children[0] = RecursiveBuild(Begin, SplitIndex);
				Nodes[Node].Children[1] = RecursiveBuild(SplitIndex, End);
			}
			else {
				check(0);
			}
		}

		return Node;
	}

	void Build(float3 * Positions, u32 PositionsNum, u32 * Indices, u32 IndicesNum, u32 PrimitivesNum, FLinearBVH & LBVH) {
		eastl::vector<FBBox> PrimitivesBBoxes;
		PrimitivesBBoxes.reserve(PrimitivesNum);

		for (u32 PrimitiveIndex = 0; PrimitiveIndex < PrimitivesNum; ++PrimitiveIndex) {
			FPrimitiveInfo Primitive = {};
			Primitive.Index = PrimitiveIndex;
			Primitive.BBox = FBBox({ Positions[Indices[PrimitiveIndex * 3]], Positions[Indices[PrimitiveIndex * 3 + 1]], Positions[Indices[PrimitiveIndex * 3 + 2]] });
			Primitives.push_back(Primitive);
		}

		RecursiveBuild(0, PrimitivesNum);

		LBVH.Positions = Positions;
		LBVH.PositionsNum = PositionsNum;
		LBVH.Indices = Indices;
		LBVH.IndicesNum = IndicesNum;
		LBVH.Nodes.clear();
		LBVH.Primitives.clear();

		struct FStack {
			u32 InnerIndex;
			u32 OuterIndex;
		};

		FStack Stack[64];
		i32 StackIndex = -1;

		u32 InnerIndex = 0;
		u32 OuterIndex = 0;
		while (1) {
			// leaf
			if (Nodes[InnerIndex].PrimitivesNum) {
				FBVHNode Node = {};
				Node.PrimitivesNum = Nodes[InnerIndex].PrimitivesNum;
				Node.PrimitivesOffset = (u32)LBVH.Primitives.size();
				Node.Bounds = Nodes[InnerIndex].Bounds;
				LBVH.Nodes.push_back(Node);

				u32 Offset = Nodes[InnerIndex].FirstPrimitiveOffset;
				for (u32 Index = 0; Index < Node.PrimitivesNum; Index++) {
					LBVH.Primitives.push_back(LeafPrimitives[Offset + Index].Index);
				}

				if (StackIndex >= 0) {
					InnerIndex = Stack[StackIndex].InnerIndex;
					OuterIndex = Stack[StackIndex].OuterIndex;
					--StackIndex;
				}
				else {
					break;
				}
			}
			// new node
			else if (OuterIndex == LBVH.Nodes.size()) {
				FBVHNode Node = {};
				Node.PrimitivesNum = 0;
				Node.SecondChild = 0xFFFFFFFF;
				Node.Bounds = Nodes[InnerIndex].Bounds;
				Node.SplitAxis = Nodes[InnerIndex].SplitAxis;
				LBVH.Nodes.push_back(Node);

				++StackIndex;
				Stack[StackIndex].InnerIndex = InnerIndex;
				Stack[StackIndex].OuterIndex = OuterIndex;
				check(StackIndex < _countof(Stack));

				// left subtree
				InnerIndex = Nodes[InnerIndex].Children[0];
				OuterIndex = OuterIndex + 1;
			}
			// right subtree
			else {
				check(OuterIndex < LBVH.Nodes.size());
				LBVH.Nodes[OuterIndex].SecondChild = (u32)LBVH.Nodes.size();
				InnerIndex = Nodes[InnerIndex].Children[1];
				OuterIndex = (u32)LBVH.Nodes.size();
			}
		}
	}
};

/////////////////////////////////////////

#include "ModelHelpers.h"

void BuildBVH(FEditorMesh * Mesh, FLinearBVH * BVH) {
	FBVHBuilder Builder;
	Builder.Build(
		Mesh->Positions.data(), (u32)Mesh->Positions.size(), 
		Mesh->Indices.data(), (u32)Mesh->Indices.size(),
		Mesh->GetIndicesNum() / 3, *BVH);
}



////////////////////
//
//
//#include "ImGui/imgui.h"
//#include "Viewer.h"
//#include "Camera.h"
//
//// pixel is from left top corner
//FRay CreatePickingRay(float2 ScreenRes, float2 Pixel, float4x4 InvViewProjection, float3 CameraPosition) {
//	float2 UV = ((Pixel + 0.5f) / ScreenRes) * float2(2, -2) + float2(-1, 1);
//	float4 CSPos = float4(UV.x, UV.y, 1, 1);
//	float4 PointedPosition = CSPos * InvViewProjection;
//	PointedPosition /= PointedPosition.w;
//
//	return FRay(CameraPosition, normalize(PointedPosition.xyz - CameraPosition));
//}
////
////bool FLinearBVH::CastShadowViz(FRay const& Ray, FLinesBatch & LinesBatch, FPolygonsBatch & PolygonsBatch) {
////	static bool Swap = true;
////	if (ImGui::GetIO().KeysDown['T'] && ImGui::GetIO().KeysDownDuration['T'] == 0) {
////		Swap = !Swap;
////	}
////
////	FRayInv RayInv = FRayInv(Ray);
////	bool DirIsNeg[3] = { Ray.Direction.x < 0, Ray.Direction.y < 0, Ray.Direction.z < 0 };
////
////	u32 Stack[64];
////	i32 StackIndex = -1;
////
////	u32 Ctr = 0;
////	bool bHit = false;
////
////	u32 Index = 0;
////	while (1) {
////		Ctr++;
////
////		if (Intersects(RayInv, Nodes[Index].Bounds)) {
////			LinesBatch.DrawBBox(Nodes[Index].Bounds, Color4b(255, 0, 0, 60));
////
////			u32 PrimitivesNum = Nodes[Index].PrimitivesNum;
////			if (PrimitivesNum) {
////				for (u32 PolygonIndex = 0; PolygonIndex < PrimitivesNum; ++PolygonIndex) {
////					u32 Primitive = Primitives[Nodes[Index].PrimitivesOffset + PolygonIndex];
////					float3 P0 = Positions[Indices[Primitive * 3]];
////					float3 P1 = Positions[Indices[Primitive * 3 + 1]];
////					float3 P2 = Positions[Indices[Primitive * 3 + 2]];
////
////					if (RayTriangleIntersection(Ray, P0, P1, P2) != NO_INTERSECTION) {
////						PolygonsBatch.DrawPolygon(P0, P1, P2, Color4b(0, 255, 80, 128));
////						bHit = true;
////						break;
////					}
////
////					PolygonsBatch.DrawPolygon(P0, P1, P2, Color4b(255, 0, 80, 40));
////				}
////
////				if (bHit) {
////					break;
////				}
////
////				if (StackIndex >= 0) {
////					Index = Stack[StackIndex];
////					StackIndex--;
////				}
////				else {
////					break;
////				}
////			}
////			else {
////				if (Swap ? DirIsNeg[Nodes[Index].SplitAxis] : !DirIsNeg[Nodes[Index].SplitAxis]) {
////					++StackIndex;
////					Stack[StackIndex] = Nodes[Index].SecondChild;
////
////					Index = Index + 1;
////				}
////				else {
////					++StackIndex;
////					Stack[StackIndex] = Index + 1;
////
////					Index = Nodes[Index].SecondChild;
////				}
////			}
////		}
////		else {
////			if (StackIndex >= 0) {
////				Index = Stack[StackIndex];
////				StackIndex--;
////			}
////			else {
////				break;
////			}
////		}
////	}
////
////	static u32 Depth = GetDepth();
////
////	ImGui::Text("Depth: %u", Depth);
////	ImGui::Text("RayCast ctr: %u", Ctr);
////
////	return bHit;
////}
////
////void RenderBVH(FEditorMesh * Mesh, FGPUContext * Context, FRenderViewport const & Viewport) {
////	static FBVHBuilder Builder;
////	static bool init = true;
////	static FLinearBVH BVH;
////	if (init) {
////		Builder.Build(Mesh->Positions.data(), Mesh->Indices.data(), Mesh->GetIndicesNum() / 3, BVH);
////		init = false;
////	}
////
////	static FLinesBatch LinesBatch;
////	static FPolygonsBatch PolygonsBatch;
////	static FDebugRenderer DebugRenderer;
////
////	//LinesBatch.DrawLine(float3(0, 0, 0), float3(100, 0, 0), Color4b(255, 0, 0, 255));
////
////	typedef eastl::pair<u32, u32> StackEntry;
////
////	eastl::vector<StackEntry> NodesStack;
////	NodesStack.push_back(StackEntry(0,0));
////	u32 Depth = 0;
////	while (!NodesStack.empty()) {
////		break;
////		StackEntry Node = NodesStack.back();
////		NodesStack.pop_back();
////
////		LinesBatch.DrawBBox(BVH.Nodes[Node.first].Bounds, Color4b(0, 0, 255, 80));
////
////		if (Node.second < 8 && BVH.Nodes[Node.first].PrimitivesNum == 0) {
////			NodesStack.push_back(StackEntry(Node.first + 1, Node.second + 1));
////			NodesStack.push_back(StackEntry(BVH.Nodes[Node.first].SecondChild, Node.second + 1));
////		}
////	}
////
////	FRay PickingRay = CreatePickingRay((float2)Viewport.Resolution, float2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y), Viewport.InvViewProjectionMatrix, Viewport.Camera->Position);
////
////	ImGui::Text("Ray: %f %f %f", PickingRay.Direction.x, PickingRay.Direction.y, PickingRay.Direction.z);
////
////	/*bool bHit = BVH.CastShadowViz(PickingRay, LinesBatch, PolygonsBatch);*/
////	float T = NO_INTERSECTION;
////	u32 Primitive;
////	bool bHit = BVH.CastRay(PickingRay, T, Primitive);
////
////	if (bHit) {
////		float3 P0 = BVH.Positions[BVH.Indices[Primitive * 3]];
////		float3 P1 = BVH.Positions[BVH.Indices[Primitive * 3 + 1]];
////		float3 P2 = BVH.Positions[BVH.Indices[Primitive * 3 + 2]];
////
////		PolygonsBatch.DrawPolygon(P0, P1, P2, Color4b(255, 60, 60, 189));
////	}
////
////	ImGui::Text("Hit: %d", (i32)bHit);
////
////
////	LinesBatch.Flush(&DebugRenderer);
////	PolygonsBatch.Flush(&DebugRenderer);
////	DebugRenderer.Render(*Context, Viewport.TViewProjectionMatrix);
////
////	//true
////	bool b0 = Intersects(FRayInv(FRay(float3(0, 0, -10), float3(0, 0, 1))), FBBox(float3(-1, -1, -1), float3(1, 1, 1)));
////	//true
////	bool b1 = Intersects(FRayInv(FRay(float3(0, 0, 0), float3(0, 0, 1))), FBBox(float3(-1, -1, -1), float3(1, 1, 1)));
////	//false
////	bool b2 = Intersects(FRayInv(FRay(float3(0, 0, 10), float3(0, 0, 1))), FBBox(float3(-1, -1, -1), float3(1, 1, 1)));
////	//true
////	bool b3 = Intersects(FRayInv(FRay(float3(0, 0, 10), float3(0, 0, -1))), FBBox(float3(-1, -1, -1), float3(1, 1, 1)));
////
////	//true?
////	bool t0 = RayTriangleIntersection(FRay(float3(0, 0, 0), float3(0, 0, 1)), float3(-1, -1, 10), float3(2, 0, 10), float3(0, 2, 10)) != NO_INTERSECTION;
////	//false?
////	bool t1 = RayTriangleIntersection(FRay(float3(0, 0, 0), float3(0, 0, 1)), float3(-1, -1, 10), float3(0, 2, 10), float3(2, 0, 10)) != NO_INTERSECTION;
////	//false
////	bool t2 = RayTriangleIntersection(FRay(float3(0, 0, 0), float3(0, 0, -1)), float3(-1, -1, 10), float3(1, 0, 10), float3(0, 1, 10)) != NO_INTERSECTION;
////}
//
//
//// on click create picking ray, collide with bvh
//// display last hit info (triangle, time)