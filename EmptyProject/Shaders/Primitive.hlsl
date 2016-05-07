cbuffer Constants : register(b0)
{      
    matrix ViewProj;
};

struct VIn 
{
	float3 	position : POSITION;
	float4 	color : COLOR;
};

struct VOut
{
	float4 	position : SV_POSITION;
	float4 	color : COLOR;
};

VOut VShader(VIn input)
{
	VOut output;
	output.position = mul(float4(input.position, 1), ViewProj);
	output.color = input.color;
	return output;
}


void PShader(VOut interpolated, out float4 OutColor : SV_TARGET0)
{
	OutColor = float4(interpolated.color);
}

