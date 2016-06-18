cbuffer FrameConstants : register(b0) {      
    matrix ViewProjectionMatrix;
}

struct VIn {
	float3 	Position : POSITION;
};

struct VOut {
	float4 	Position : SV_POSITION;
};

VOut VShader(VIn Input) {
	VOut Output;
	float4 Position = float4(Input.Position, 1);
	Position = mul(Position, ViewProjectionMatrix);
	Output.Position = Position;
	return Output;
}

void PShader(VOut Interpolated, out float4 OutColor : SV_TARGET0) {
	OutColor = 1;
}