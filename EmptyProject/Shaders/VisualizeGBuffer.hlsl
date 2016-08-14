#include "ShaderCommon.inl"

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
Texture2D<float2> 	GBuffer2 : register(t3);

Texture2D<int> MotionVectorsListBegin : register(t4);
struct FVector {
	float2 P0; 
	float2 P1;
};

struct FNode {
	u32 Next;
	u32 Index;
};
StructuredBuffer<FNode> Nodes : register(t5);
StructuredBuffer<FVector> Vectors : register(t6);

float LinearizeDepth(float PostProjectionDepth, float Projection_33, float Projection_43) {
	return Projection_43 / (PostProjectionDepth - Projection_33);
}

float LinearizeDepth(float PostProjectionDepth, matrix Projection) {
	return LinearizeDepth(PostProjectionDepth, Projection._33, Projection._43);
}

struct GBufferData {
	float4 SvPosition;
	float3 Albedo;
	float3 Normal;
	float2 MotionVector;
	float PostProjectionDepth;
	float LinearDepth;
	float IsFarPlane;
};

void FetchGBuffer(inout GBufferData GBuffer) {
	GBuffer.Albedo = GBuffer0[GBuffer.SvPosition.xy].xyz;
	GBuffer.PostProjectionDepth = DepthBuffer[GBuffer.SvPosition.xy].x;
	GBuffer.LinearDepth = LinearizeDepth(GBuffer.PostProjectionDepth, Frame.ProjectionMatrix);
	GBuffer.Normal = GBuffer1[GBuffer.SvPosition.xy].xyz * 2 - 1;
	GBuffer.MotionVector = GBuffer2[GBuffer.SvPosition.xy].xy;
	GBuffer.IsFarPlane = GBuffer.PostProjectionDepth == 1.f;
}

float4 VisualizeDepth(float4 SvPosition, float2 Texcoord)
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return GBuffer.LinearDepth / 100.f;
}

float4 VisualizeAlbedo(float4 SvPosition, float2 Texcoord)
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return float4(GBuffer.Albedo, 1);
}

float4 VisualizeNormals(float4 SvPosition, float2 Texcoord)
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return float4(GBuffer.Normal * 0.5f + 0.5f, 0);
}

#include "DrawHelpers.inc"

float4 DrawMotionVector(float4 SvPosition, float2 Texcoord) {
	float4 Acc = 0;

	uint2 RenderTile = SvPosition.xy / 8;
	int NodeIndex = MotionVectorsListBegin[RenderTile];
	uint Safety = 256;
	while(NodeIndex != -1 && Safety > 0) {
		FNode Node = Nodes[NodeIndex];
		FVector Vector = Vectors[Node.Index];

		Acc += HelperArrow(SvPosition.xy, Vector.P0, Vector.P1, float4(1,0,0,1));

		NodeIndex = Node.Next;
		--Safety;
	}
	return Acc;
}

float4 VisualizeMotionVectors(float4 SvPosition, float2 Texcoord) 
{
	GBufferData GBuffer;
	GBuffer.SvPosition = SvPosition;
	FetchGBuffer(GBuffer);
	return DrawMotionVector(SvPosition, Texcoord);
	return MotionVectorsListBegin[GBuffer.SvPosition.xy / 8] + 1;
	return float4(GBuffer2[GBuffer.SvPosition.xy].xy, 0, 0);
}

#ifndef SHOW_ID
#define SHOW_ID 0
#endif

float4 PixelMain(float4 SvPosition : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_TARGET {
	#if SHOW_ID == 0
		return VisualizeAlbedo(SvPosition, Texcoord);
	#elif SHOW_ID == 1
		return VisualizeNormals(SvPosition, Texcoord);
	#elif SHOW_ID == 2
		return VisualizeDepth(SvPosition, Texcoord);
	#elif SHOW_ID == 3
		return VisualizeMotionVectors(SvPosition, Texcoord);
	#endif
}