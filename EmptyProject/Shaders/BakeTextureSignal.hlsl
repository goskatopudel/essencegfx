Texture2D PositionsTexture : register(t0);
Texture2D NormalsTexture : register(t1);

cbuffer Constants : register(b0)
{      
    float2 	Sequence[16];
}


typedef uint u32;
typedef int i32;

struct BVHNode {
    float3 	VMin;
    u32		SecondChild;
    float3 	VMax;
	u32		PrimitivesNum;
	u32		PrimitivesOffset;
	u32		SplitAxis;
};

static const i32 MAX_DEPTH = 32;

StructuredBuffer<BVHNode>	BVHNodes : register(t2);
StructuredBuffer<uint>		Primitives : register(t3);
StructuredBuffer<float3>	PositionsBuffer : register(t4);
StructuredBuffer<uint>		IndicesBuffer : register(t5);

Texture2D BlendTexture : register(t6);

RWTexture2D<float4> BakedSignal : register(u0);

struct FRay {
	float3	Origin;
	float3	Direction;
};

static const float FINF = pow(2.f, 64);

static const u32 SamplesNum = 16;

bool Intersects(float3 RayOrigin, float3 RayInvDir, float3 BBoxMin, float3 BBoxMax) {
	float3 T0 = (BBoxMin - RayOrigin) * RayInvDir;
	float3 T1 = (BBoxMax - RayOrigin) * RayInvDir;

	float3 TMin = min(T0, T1);
	float3 TMax = max(T0, T1);

	float TEnter = max(TMin.x, max(TMin.y, TMin.z));
	float TExit = min(TMax.x, min(TMax.y, TMax.z));

	return TExit > max(TEnter, 0);
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

bool CastShadowRay(FRay Ray) {
	float3 RayInvDir = 1 / Ray.Direction;
	bool DirIsNeg[3] = { Ray.Direction.x < 0, Ray.Direction.y < 0, Ray.Direction.z < 0 };

	u32 Stack[MAX_DEPTH];
	i32 StackIndex = -1;

	u32 Index = 0;
	while (1) {
		if (Intersects(Ray.Origin, RayInvDir, BVHNodes[Index].VMin, BVHNodes[Index].VMax)) {

			u32 PrimitivesNum = BVHNodes[Index].PrimitivesNum;
			if (PrimitivesNum) {
				for (u32 PolygonIndex = 0; PolygonIndex < PrimitivesNum; ++PolygonIndex) {
					u32 Primitive = Primitives[BVHNodes[Index].PrimitivesOffset + PolygonIndex];
					float3 P0 = PositionsBuffer[IndicesBuffer[Primitive * 3]];
					float3 P1 = PositionsBuffer[IndicesBuffer[Primitive * 3 + 1]];
					float3 P2 = PositionsBuffer[IndicesBuffer[Primitive * 3 + 2]];

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
				if (DirIsNeg[BVHNodes[Index].SplitAxis]) {
					if(StackIndex == MAX_DEPTH) {
						return false;
					}

					++StackIndex;
					Stack[StackIndex] = BVHNodes[Index].SecondChild;

					Index = Index + 1;
				}
				else {
					if(StackIndex == MAX_DEPTH) {
						return false;
					}

					++StackIndex;
					Stack[StackIndex] = Index + 1;

					Index = BVHNodes[Index].SecondChild;
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

static const float MATH_PI = 3.141593f;
static const float MATH_PI_INV = 1.f / MATH_PI;

float4 CosineWeightedSample(float2 Xi, float3 N) {
	float3 UpVec = abs(N.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
	float3 TangentX = normalize(cross(UpVec, N));
	float3 TangentY = cross( N, TangentX );
	
	float SinPhi = sqrt(1 - Xi.y);
    float X = cos(2 * MATH_PI * Xi.x) * SinPhi;
    float Y = sin(2 * MATH_PI * Xi.x) * SinPhi;
    float Z = sqrt(Xi.y);
    float P = Z * MATH_PI;
    
    return float4(X * TangentX + Y * TangentY + Z * N, P);
}

[numthreads(8, 8, 1)]
void BakeAO(uint3 DTid : SV_DispatchThreadID) {
	float3 Position = PositionsTexture[DTid.xy].xyz;
	float3 Normal = NormalsTexture[DTid.xy].xyz;

	if(length(Normal) == 0.f) {
		BakedSignal[DTid.xy] = 0;
		return;
	}
	
	FRay OutRay;
	OutRay.Origin = Position + Normal * 0.01f;
	OutRay.Direction = Normal;

	float V = 0;
	for(u32 I = 0; I < SamplesNum; ++I) {
		float4 Sample = CosineWeightedSample(Sequence[I], Normal);
		OutRay.Direction = Sample.xyz;
		bool hit = CastShadowRay(OutRay);

		if(Sample.w) {
			V += hit;
		}
	}

	BakedSignal[DTid.xy] = lerp(1 - V / (float)SamplesNum, BlendTexture[DTid.xy], 0.5f);
}