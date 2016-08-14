#include "ShaderCommon.inl"

ConstantBuffer<FFrameConstants> Frame : register(b0);

struct FMotionVectorsParams {
	// for tile size of 2*N+1 this is N
	uint2 MVecTileCenter;
	uint2 BinningTileSize;
	u32 BinningTilesInRow; 
};
ConstantBuffer<FMotionVectorsParams> Params : register(b1);

struct FVector {
	float2 P0; 
	float2 P1;
};

struct FNode {
	u32 Next;
	u32 Index;
};

RWTexture2D<int> TileListBegin : register(u0);
RWStructuredBuffer<FNode> Nodes : register(u1);
RWStructuredBuffer<FVector> Vectors : register(u2);

Texture2D<float> 	DepthBuffer : register(t0);
Texture2D<float4> 	GBuffer2 : register(t3);

void AppendToTile(uint2 TileCoord, u32 VectorIndex) {
	uint Next = Nodes.IncrementCounter();
	uint OldNext;
	InterlockedExchange(TileListBegin[TileCoord], Next, OldNext);
	Nodes[Next].Next = OldNext;
	Nodes[Next].Index = VectorIndex;
}

float copysign(float x, float y) {
	return y < 0.0 ? -x : x;
}

void RaymarchTiles(float2 P, float2 PEnd, float2 Dir, u32 Index) {
	const float FltMax = 3.402823466e+38f;
	const float Eps = exp2(-50.0f);

	if(abs(Dir.x) < Eps) Dir.x = copysign(Eps, Dir.x);
	if(abs(Dir.y) < Eps) Dir.y = copysign(Eps, Dir.y);

	float2 InvDir = 1.f / Dir;

	int2 CellStep = (Dir < 0.f) ? -1 : 1;

	int2 Cell;
	Cell = floor(P);

	int2 EndCell = floor(PEnd.xy);
	int Steps = 1 + dot(abs(EndCell - Cell), 1);

	InvDir = abs(InvDir);

	float2 TMax = FltMax;
	if(Dir.x > 0.0) TMax.x = (Cell.x + 1 - P.x) * InvDir.x;
	if(Dir.x < 0.0) TMax.x = (P.x - Cell.x) * InvDir.x;
	if(Dir.y > 0.0) TMax.y = (Cell.y + 1 - P.y) * InvDir.y;
	if(Dir.y < 0.0) TMax.y = (P.y - Cell.y) * InvDir.y;

	float T;
	for(int i = 0; i < Steps; i++) {
		T = min(TMax.x, TMax.y);
		if(TMax.x <= T) { TMax.x += InvDir.x; Cell.x += CellStep.x; }
		if(TMax.y <= T) { TMax.y += InvDir.y; Cell.y += CellStep.y; }

		//InterlockedAdd(TileListBegin[Cell], 1);
		AppendToTile(Cell, Index);
	}
}

[numthreads(8, 8, 1)]
void PreprocessMain(uint3 DTid : SV_DispatchThreadID) {
	// calculate screen position from, to for DTid tile
	uint2 Texel = DTid.xy * (Params.MVecTileCenter * 2 + 1) + Params.MVecTileCenter;

	if(DepthBuffer[Texel] == 1) {
		return;
	}

	u32 Index = dot(DTid.xy, uint2(1, Params.BinningTilesInRow));

	float2 MotionVector = GBuffer2[Texel].xy;
	float2 P1 = float2(Texel) + 0.5f;
	float2 P0 = P1 + MotionVector * Frame.ScreenResolution;
	// raymarch through screen tiles, append counter
	// foreach touched node

	Vectors[Index].P0 = P0;
	Vectors[Index].P1 = P1;

	RaymarchTiles(P0 / Params.BinningTileSize, P1 / Params.BinningTileSize, normalize(P1 - P0), Index);
}