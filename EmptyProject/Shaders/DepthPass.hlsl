cbuffer Constants : register(b0)
{      
    matrix 	ProjectionMatrix;
    matrix 	ViewMatrix;
    matrix  WorldMatrix;
    float2	DepthNormalization; // .x = 1/(MaxDepth - MinDepth); .y = MinDepth / (MaxDepth - MinDepth);
}

struct VIn 
{
	float3 	position : POSITION;
	float3 	normal : NORMAL;
	float3 	tangent : TANGENT;
	float3 	bitangent : BITANGENT;
	float2 	texcoord0 : TEXCOORD0;
	float2 	texcoord1 : TEXCOORD1;
	float3 	color : COLOR;
};

struct VOut
{
	float4 	position : SV_POSITION;
#if defined(VSM)
	float 	linearDepth : LINEAR_DEPTH;
#endif
};

VOut VertexMain(VIn input, uint vertexId : SV_VertexID)
{
	VOut output;

	float4 position = float4(input.position, 1);
	position = mul(position, WorldMatrix);
	float4 viewPosition = mul(position, ViewMatrix);
	output.position = mul(viewPosition, ProjectionMatrix);
#if defined(VSM)
	output.linearDepth = viewPosition.z * DepthNormalization.x + DepthNormalization.y;
#endif
	return output;
}

#if defined(VSM)
void PixelMain(VOut interpolated, out float Out0 : SV_TARGET0, out float Out1 : SV_TARGET0)
{
	Out0 = interpolated.linearDepth;
	Out1 = interpolated.linearDepth * interpolated.linearDepth;
}
#endif

