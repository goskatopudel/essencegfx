#pragma once
#include "Essence.h"
#include "MathVector.h"
#include "MathMatrix.h"

struct FFrameConstants {
	float4x4 ViewProjectionMatrix;
	float4x4 ViewMatrix;
	float4x4 ProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjectionMatrix;
	float2 ScreenResolution;
};

struct FObjectConstants {
	float4x4 WorldMatrix;
	float4x4 ShadowMatrix;
};

class FScene;
class FCamera;

struct FSceneContext {
	FScene * Scene;
	FCamera * Camera;
};