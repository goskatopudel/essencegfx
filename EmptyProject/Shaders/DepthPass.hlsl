cbuffer Constants : register(b0)
{      
    float4x4 WorldViewProjectionMatrix;
}

struct VIn 
{
	float3 	Position : POSITION;
	float3 	Normal : NORMAL;
	float3 	Tangent : TANGENT;
	float3 	Bitangent : BITANGENT;
	float2 	Texcoord0 : TEXCOORD0;
	float2 	Texcoord1 : TEXCOORD1;
	float3 	Color : COLOR;
};

struct VOut
{
	float4 	SvPosition : SV_POSITION;
};

VOut VertexMain(VIn Input, uint VertexId : SV_VertexID)
{
	VOut Output;
	Output.SvPosition = mul(float4(Input.Position, 1), WorldViewProjectionMatrix);
	return Output;
}

// assuming orthogonal matrix
void PixelMain(VOut Interpolated, out float OutRT0 : SV_TARGET0)
{
	float LinearDepth = Interpolated.SvPosition.z;
	float dx = ddx(LinearDepth);
	float dy = ddy(LinearDepth);
	OutRT0 = LinearDepth * LinearDepth;// + 0.25f * dx*dx + dy*dy;
}


