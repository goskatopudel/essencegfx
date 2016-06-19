cbuffer FrameConstants : register(b0) {      
    matrix ViewProjectionMatrix;
}

struct VS_In {
	float3 	Position : POSITION;
};

struct VS_Out {
	float3 	Position : POSITION;
};

VS_Out VertexMain(VS_In Input) {
	VS_Out Output;
	Output.Position = Input.Position;
	return Output;
}

struct HS_PER_PATCH_OUTPUT {
	float EdgeTessalation[4] : SV_TessFactor;
	float InsideTessalation[2] : SV_InsideTessFactor;
};

HS_PER_PATCH_OUTPUT HullPatchFunc(InputPatch<VS_Out, 4> InPatch, uint PatchID : SV_PrimitiveID) {
	HS_PER_PATCH_OUTPUT Output;

	Output.EdgeTessalation[0] = Output.EdgeTessalation[1] = Output.EdgeTessalation[2] = Output.EdgeTessalation[3] = 2.f;
	Output.InsideTessalation[0] = Output.InsideTessalation[1] = 8.f;

	return Output;
}

struct HS_OUTPUT {
	float3 	Position : Position;
};

[domain("quad")]
[partitioning("pow2")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HullPatchFunc")]
HS_OUTPUT HullMain(InputPatch<VS_Out, 4> InVertices, uint ControlPointID : SV_OutputControlPointID) {
	HS_OUTPUT Output;

	Output.Position = InVertices[ControlPointID].Position;

	return Output;
}

struct DS_OUTPUT {
	float4 Position : SV_Position;
};

[domain("quad")]
DS_OUTPUT DomainMain(HS_PER_PATCH_OUTPUT Patch, const OutputPatch<HS_OUTPUT, 4> PatchPoints, float2 DomainUV : SV_DomainLocation) {
	DS_OUTPUT Output;

	float3 T = PatchPoints[1].Position - PatchPoints[0].Position;
	float3 B = PatchPoints[3].Position - PatchPoints[0].Position;

	float4 Position = float4(PatchPoints[0].Position + T * DomainUV.x + B * DomainUV.y, 1);
	Position = mul(Position, ViewProjectionMatrix);
	Output.Position = Position;

	return Output;
}

struct PS_In {
	float4 	Position : SV_POSITION;
};

void PixelMain(DS_OUTPUT Interpolated, out float4 OutColor : SV_TARGET0) {
	OutColor = 1;
}