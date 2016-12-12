struct FMaterialSurfaceInterface {
	float3 WorldPosition;
    float3 WorldNormal;
    float3 Albedo;
    float2 MotionVector;
};

void FMaterialSurfaceInterface_construct(inout FMaterialSurfaceInterface MaterialSurfaceInterface) {
	MaterialSurfaceInterface.WorldPosition = 0.f;
	MaterialSurfaceInterface.WorldNormal = 0.f;
	MaterialSurfaceInterface.Albedo = 0.f;
	MaterialSurfaceInterface.MotionVector = 0.f;
}