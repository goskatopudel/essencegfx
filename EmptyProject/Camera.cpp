#include "Camera.h"
#include "MathFunctions.h"
#include <DirectXMath.h>
using namespace DirectX;

float4 QuaternionRotationNormal(float3 normalAxis, float angle) {
	float SinV, CosV;

	XMScalarSinCos(&SinV, &CosV, 0.5f * angle);

	return float4(normalAxis * SinV, CosV);
}

float4 QuaternionMul(float4 Q1, float4 Q2) {
	return float4((Q2.w * Q1.x) + (Q2.x * Q1.w) + (Q2.y * Q1.z) - (Q2.z * Q1.y),
		(Q2.w * Q1.y) - (Q2.x * Q1.z) + (Q2.y * Q1.w) + (Q2.z * Q1.x),
		(Q2.w * Q1.z) + (Q2.x * Q1.y) - (Q2.y * Q1.x) + (Q2.z * Q1.w),
		(Q2.w * Q1.w) - (Q2.x * Q1.x) - (Q2.y * Q1.y) - (Q2.z * Q1.z));
}

template<typename T, i32 n>
Vector<T, n> operator -(Vector<T, n>const& rhs) {
	Vector<T, n> v;
	for (i32 i = 0; i < n; ++i) {
		v[i] = -rhs[i];
	}
	return v;
}

float3 QuaternionRotate(float3 v, float4 q) {
	float4 qc = float4(-q.xyz, q.w);
	float4 a = float4(v, 0);
	return QuaternionMul(QuaternionMul(qc, a), q).xyz;
}

void	FCamera::Strafe(float value) {
	float3 right = cross(Up, Direction);
	Position += right * value;
}

void	FCamera::Dolly(float value) {
	Position += Direction * value;
}

void	FCamera::Rotate(float dx, float dy) {
	float3 right = cross(Up, Direction);
	float4 pitch = QuaternionRotationNormal(right, dx);
	float4 yaw = QuaternionRotationNormal(Up, dy);

	auto dir = QuaternionRotate(Direction, QuaternionMul(pitch, yaw));
	right = QuaternionRotate(right, yaw);
	auto up = cross(dir, right);

	Direction = normalize(dir);
	Up = normalize(up);
}

void	FCamera::Roll(float value) {
	Up = QuaternionRotate(Up, QuaternionRotationNormal(Direction, -value));
}

void	FCamera::Climb(float value) {
	Position += Up * value;
}

DirectX::XMVECTOR ToSimd(float3 v) {
	return XMLoadFloat3((XMFLOAT3*)&v);
}