#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"

#include "Shaders\ShaderCommon.inl"

struct FObjectConstants {
	float4x4 WorldMatrix;
	float4x4 PrevWorldMatrix;
	float4x4 ShadowMatrix;
};

class FScene;
class FCamera;

struct FSceneContext {
	FScene * Scene;
	FCamera * Camera;
};