#pragma once
#include "Essence.h"
#include "MathVector.h"
#include <DirectXMath.h>

class FCamera {
public:
	float3	Position;
	float3	Up;
	float3	Direction;

	void	Strafe(float value);
	void	Dolly(float value);
	void	Rotate(float dx, float dy);
	void	Roll(float value);
	void	Climb(float value);
};

DirectX::XMVECTOR ToSimd(float3);