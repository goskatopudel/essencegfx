cbuffer Constants : register(b0)
{      
    matrix 	ViewProj;
    matrix 	World;
    matrix 	InvView;
    uint 	CustomUint0;
}


Texture2D 		UVTexture	: register(t0);
SamplerState 	TextureSampler : register(s0);


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
	float3 	wposition : POSITION;
	float3 	normal : NORMAL;
	float2 	texcoord0 : TEXCOORD0;
	float2 	texcoord1 : TEXCOORD1;
	float3 	color : COLOR;
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
	output.texcoord1 = input.texcoord1;
	output.color = input.color;
	return output;
}

void PSDebug(VOut interpolated, out float4 OutColor : SV_TARGET0)
{
	float3 N = normalize(interpolated.normal);
	float3 V = normalize(InvView[3].xyz - interpolated.wposition);
	float NoV = saturate(dot(N, V));

	if(CustomUint0 == 0) {
		OutColor = NoV;
	}
	else if(CustomUint0 == 1) {
		float3 N = normalize(interpolated.normal);
		OutColor = float4(N * 0.5f + 0.5f, 0);
	}
	else if(CustomUint0 == 2 || CustomUint0 == 3 ) {
		float4 sample = UVTexture.Sample(TextureSampler, CustomUint0 == 2 ? interpolated.texcoord0 : interpolated.texcoord1 );
		OutColor = sample;
	}
	else {
		OutColor = float4(1, 0.5f, 0.5f, 1.f);
	}
}

