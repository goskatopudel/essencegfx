#include "ShaderCommon.inl"

ConstantBuffer<FFrameConstants> Frame : register(b0);

struct FVector {
	float2 P0; 
	float2 P1;
};

RWTexture2D<uint> ListBegin;
StructuredBuffer<uint> Nodes;
StructuredBuffer<FVector> Vectors;

void AppendToTile(uint2 TileCoord, FVector Vector) {
	uint Next = Nodes.IncrementCounter();
	uint Prev = ListBegin[TileCoord].InterlockedExchange(Next);
	Nodes[Next] = Prev;
	ListBegin[TileCoord] = Next;
	Vectors[Next] = Vector;
}

[numthreads(8, 8, 1)]
void GatherMain(uint3 DTid : SV_DispatchThreadID) {
	// calculate screen position from, to for DTid tile

	// raymarch through screen tiles, append counter
	// foreach touched node
	

}

void VisualizeMotionVectorsMain() {
	// read tile index
	// look over vectors, draw
}