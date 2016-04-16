cbuffer FrameConstants : register(b0)
{       
    matrix ViewProj;
    matrix InvView;
    float3 LightFromDirection;
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
Texture2D MetallicTexture 	: register(t1);
Texture2D NormalmapTexture 	: register(t2);
Texture2D RoughnessTexture 	: register(t3);
Texture2D AlphaTexture 		: register(t4);
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

#define M_PI			3.14159265358979323846
#define M_PI_DIV_2     	1.57079632679489661923
#define M_PI_DIV_4     	0.785398163397448309616
#define M_1_DIV_PI     	0.318309886183790671538
#define M_2_DIV_PI    	0.636619772367581343076

float OrenNayar(float nl, float nv, float r) {
	float r2 = r * r;
	float A = 1. - 0.5 * r2 / (r2 + 0.57);
	float B = 0.45 * r2 / (r2 + 0.09);
	float C = sqrt((1.0 - nv*nv) * (1.0 - nl*nl)) / max(nv, nl);
	return saturate((A + B * C) * M_1_DIV_PI);
}

float3 FSchlick(float3 f0, float vh) 
{
	return f0 + (1-f0)*exp2((-5.55473 * vh - 6.98316)*vh);
}

// = G / (4 * dot(N,L) * dot(N, V))
float VSmithSchlick(float nl, float nv, float a) {
	return 0.25 / ((nl*(1-a)+a) * (nv*(1-a)+a));
}

// = V * 4 * dot(N,L) * dot(N, V)
// original function for CookTorrance
float GSmithSchlick(float nl, float nv, float a) {
	return 4 * nl * nv * VSmithSchlick(nl, nv, a);
}

float DGGX(float nh, float a) 
{
	float a2 = a*a;
	float denom = pow(nh*nh * (a2-1) + 1, 2);
	return a2 * M_1_DIV_PI / denom;
}

void PShader(VOut interpolated, out float4 OutColor : SV_TARGET0)
{
	float3 N = normalize(interpolated.normal);
	float3 V = normalize(InvView[3].xyz - interpolated.wposition);

	float3 BaseColor = BaseColorTexture.Sample(TextureSampler, interpolated.texcoord0).xyz;
	float Roughness = RoughnessTexture.Sample(TextureSampler, interpolated.texcoord0).x;
	float Metallic = MetallicTexture.Sample(TextureSampler, interpolated.texcoord0).x;

	float a = Roughness * Roughness;

	float3 L = normalize(float3(1,2,0.5f));
	float3 LightIntensity = 1.f;

	float3 H = normalize(L+V);
	float NoV = saturate(dot(N, V));
	float NoL = saturate(dot(N, L));
	float NoH = saturate(dot(N, H));
	float VoH = saturate(dot(V, H));

	float3 Light = LightIntensity * NoL * M_PI;

	float3 F0 = lerp(0.05f, BaseColor, Metallic);
	float3 Albedo = lerp(BaseColor, 0, Metallic);

	float3 Diffuse = OrenNayar(NoL, NoV, Roughness) * Albedo;
	// DGF/ (4*dot(N,L)*dot(N,V))
	// Visibility = G / (4*dot(N,L)*dot(N,V))
	float3 Specular = DGGX(NoH, a) * saturate(VSmithSchlick(NoL, NoV, a)) * FSchlick(F0, VoH);
	float3 OutLight = (Diffuse + Specular) * Light + Albedo * 0.005f;

	OutColor = float4(OutLight, 0);
}
