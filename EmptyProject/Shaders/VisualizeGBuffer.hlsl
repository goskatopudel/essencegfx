#include "Common.inl"

ConstantBuffer<FFrameConstants> Frame : register(b0);

struct VOut
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VOut VertexMain(uint vertexId : SV_VertexID)
{
	VOut output;
	output.position.x = (float)(vertexId / 2) * 4 - 1;
	output.position.y = (float)(vertexId % 2) * 4 - 1;
	output.position.z = 1;
	output.position.w = 1;

	output.texcoord.x = (float)(vertexId / 2) * 2;
	output.texcoord.y= 1 - (float)(vertexId % 2) * 2;

	return output;
}

Texture2D<float> 	DepthBuffer : register(t0);
Texture2D<float4> 	GBuffer0 : register(t1);
Texture2D<float4> 	GBuffer1 : register(t2);

float LinearizeDepth(float PostProjectionDepth, float Projection_33, float Projection_43) {
	return Projection_43 / (PostProjectionDepth - Projection_33);
}

float LinearizeDepth(float PostProjectionDepth, matrix Projection) {
	return LinearizeDepth(PostProjectionDepth, Projection._33, Projection._43);
}

struct GBufferData {
	float4 SvPosition;
	float3 Normal;
	float PostProjectionDepth;
	float LinearDepth;
};

void FetchGBuffer(GBufferData inout GBuffer) {
	GBuffer.PostProjectionDepth = DepthBuffer[GBuffer.SvPosition.xy].x;
	GBuffer.LinearDepth = LinearizeDepth(GBuffer.PostProjectionDepth, Frame.ProjectionMatrix);
	GBuffer.Normal = GBuffer0[GBuffer.SvPosition.xy].xyz * 2 - 1;
}

float4 VisualizeDepth(float4 SvPosition, float2 Texcoord)
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return GBuffer.LinearDepth / 100.f;
}

float4 VisualizeNormals(float4 SvPosition, float2 Texcoord)
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return GBuffer.Normal * 0.5f + 0.5f;
}

float4 VisualizeMotionVectors(float4 SvPosition, float2 Texcoord) 
{
	return 0;
}

#ifndef SHOW_ID
#define SHOW_ID 0
#endif

float4 PixelMain(float4 SvPosition : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_TARGET {
	#if SHOW_ID == 0
		return VisualizeDepth(SvPosition, Texcoord);
	#elif SHOW_ID == 1
		return VisualizeNormals(SvPosition, Texcoord);
	#endif
}