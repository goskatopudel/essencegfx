#include "ShaderCommon.inl"

ConstantBuffer<FFrameConstants> Frame : register(b0);

struct VOut
{
	float4 Position : SV_POSITION;
	float2 Texcoord : TEXCOORD;
};

VOut VertexMain(uint VertexId : SV_VertexID)
{
	VOut Output;
	Output.Position.x = (float)(VertexId / 2) * 4 - 1;
	Output.Position.y = (float)(VertexId % 2) * 4 - 1;
	Output.Position.z = 1;
	Output.Position.w = 1;

	Output.Texcoord.x = (float)(VertexId / 2) * 2;
	Output.Texcoord.y= 1 - (float)(VertexId % 2) * 2;

	return Output;
}

Texture2D<float4> 	OldColor : register(t0);
Texture2D<float4> 	CurrentColor : register(t1);

float4 PixelMain(float4 Position : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_Target0
{
	return lerp(OldColor[Position.xy], CurrentColor[Position.xy], 0.01f);
}