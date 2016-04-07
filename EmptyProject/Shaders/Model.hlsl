cbuffer FrameConstants : register(b0)
{       
    matrix ViewProj;
}

cbuffer MaterialConstants : register(b1)
{       
    uint	MaterialIndex;
}

cbuffer ObjectConstants : register(b2)
{       
    matrix World;
}

Texture2D BaseColorTexture 	: register(t0);
SamplerState TextureSampler : register(s0);

struct VIn 
{
	float3 	position : POSITION;
	float2 	texcoord0 : TEXCOORD;
	float3 	normal : NORMAL;
};

struct VOut
{
	float4 	position : SV_POSITION;
	float3 	wposition : POSITION;
	float3 	normal : NORMAL;
	float2 	texcoord0 : TEXCOORD;
};

VOut VShader(VIn input, uint vertexId : SV_VertexID)
{
	VOut output;

	matrix objectMatrix = World;

	float4 position = float4(input.position, 1);
	position = mul(position, objectMatrix);
	output.wposition = position.xyz;
	position = mul(position, ViewProj);
	output.position = position;
	output.normal = normalize(mul(input.normal, (float3x3) objectMatrix));
	output.texcoord0 = input.texcoord0;
	return output;
}

void PShader(VOut interpolated, out float4 OutColor : SV_TARGET0)
{
	OutColor = BaseColorTexture.Sample(TextureSampler, interpolated.texcoord0);
}