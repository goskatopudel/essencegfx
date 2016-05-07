#pragma once
#include "MathVector.h"
#include "MathMatrix.h"

template<typename T, i32 n>
Vector<T, n> operator +(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] += rhs.data[i];
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator +=(Vector<T, n>& lhs, Vector<T, n>const& rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] += rhs.data[i];
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator +(Vector<T, n>const& lhs, T rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] += rhs;
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator +=(Vector<T, n>& lhs, T rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] += rhs;
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator -(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] -= rhs.data[i];
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator -=(Vector<T, n>& lhs, Vector<T, n>const& rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] -= rhs.data[i];
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator -(Vector<T, n>const& lhs, T rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] -= rhs;
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator -=(Vector<T, n>& lhs, T rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] -= rhs;
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator *(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] *= rhs.data[i];
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator *=(Vector<T, n>& lhs, Vector<T, n>const& rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] *= rhs.data[i];
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator *(Vector<T, n>const& lhs, T rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] *= rhs;
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator *=(Vector<T, n>& lhs, T rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] *= rhs;
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator /(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> v = lhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] /= rhs.data[i];
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator /=(Vector<T, n>& lhs, Vector<T, n>const& rhs) {
	for (i32 i = 0; i < n; ++i) {
		lhs[i] /= rhs.data[i];
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator /(Vector<T, n>const& lhs, T rhs) {
	Vector<T, n> v = lhs;
	T rcp = T(1) / rhs;
	for (i32 i = 0; i < n; ++i) {
		v[i] *= rcp;
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n>& operator /=(Vector<T, n>& lhs, T rhs) {
	T rcp = T(1) / rhs;
	for (i32 i = 0; i < n; ++i) {
		lhs[i] *= rcp;
	}
	return lhs;
}

template<typename T, i32 n>
Vector<T, n> operator /(T lhs, Vector<T, n>const& rhs) {
	Vector<T, n> v;
	for (i32 i = 0; i < n; ++i) {
		v[i] = lhs / rhs[i];
	}
	return v;
}

template<typename T, i32 n>
T dot(Vector<T, n>& lhs, Vector<T, n>const& rhs) {
	T v = 0;
	for (i32 i = 0; i < n; ++i) {
		v += lhs[i] * rhs[i];
	}
	return v;
}

template<typename T, i32 n>
Vector<T, n> cross(Vector<T, n>const& lhs, Vector<T, n>const& rhs);

template<i32 n>
Vector<float, n> normalize(Vector<float, n>const& v) {
	float rcp = 0.f;
	for (i32 i = 0; i < n; ++i) {
		rcp += v[i] * v[i];
	}
	rcp = sqrtf(1.f / rcp);

	Vector<float, n> o = v;
	for (i32 i = 0; i < n; ++i) {
		o.data[i] *= rcp;
	}
	return o;
}

template<i32 n>
float length(Vector<float, n> const& v) {
	float len = 0.f;
	for (i32 i = 0; i < n; ++i) {
		len += v[i] * v[i];
	}
	return sqrtf(len);
}

inline Vec2f LinesIntersection2D(Vec3f L0, Vec3f L1) {
	Vec3f c = cross(L0, L1);
	return c.xy / c.z;
}

inline Vec3f LineFromPoints2D(Vec2f P0, Vec2f P1) {
	Vec3f c = cross(Vec3f(P0, -1), Vec3f(P1, -1));
	return c;
}

template<typename T, i32 rows, i32 cols, i32 cols1>
Matrix<T, rows, cols1> operator*(Matrix<T, rows, cols> const& lhs, Matrix<T, cols, cols1> const& rhs) {
	Matrix<T, rows, cols1> o;
	for (i32 c = 0; c < cols1; ++c) {
		for (i32 r = 0; r < rows; ++r) {
			o.data[r][c] = T(0);
			for (i32 i = 0; i < cols; ++i) {
				o.data[r][c] += lhs.data[r][i] * rhs.data[i][c];
			}
		}
	}
	return o;
}


template<typename T, i32 rows, i32 cols>
Vector<T, rows> operator*(Matrix<T, rows, cols> const& lhs, Vector<T, cols> const& rhs) {
	Vector<T, rows> o;
	for (i32 r = 0; r < rows; ++r) {
		o[r] = T(0);
		for (i32 c = 0; c < cols; ++c) {
			o[r] += lhs.data[r][c] * rhs[c];
		}
	}
	return o;
}

template<typename T, i32 rows, i32 cols>
Vector<T, cols> operator*(Vector<T, rows> const& lhs, Matrix<T, rows, cols> const& rhs) {
	Vector<T, cols> o;
	for (i32 c = 0; c < cols; ++c) {
		o[c] = T(0);
		for (i32 r = 0; r < rows; ++r) {
			o[c] += lhs[r] * rhs.data[r][c];
		}
	}
	return o;
}

template<>
inline Matrix<float, 2, 2> Matrix<float, 2, 2>::Rotation(float angle) {
	Matrix<float, 2, 2> o;
	o._11 = cosf(angle);
	o._12 = -sinf(angle);
	o._21 = -o._12;
	o._22 = o._11;
	return o;
}

template<typename T> T TCos(T v);
template<typename T> T TSin(T v);

template<> inline float TCos(float v) {
	return cosf(v);
}

template<> inline float TSin(float v) {
	return sinf(v);
}

template<typename T, i32 n>
Vector<T, n> min(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> result;
	for (i32 i = 0; i < n; ++i) {
		result[i] = lhs[i] < rhs[i] ? lhs[i] : rhs[i];
	}
	return result;
}

template<typename T, i32 n>
Vector<T, n> max(Vector<T, n>const& lhs, Vector<T, n>const& rhs) {
	Vector<T, n> result;
	for (i32 i = 0; i < n; ++i) {
		result[i] = lhs[i] > rhs[i] ? lhs[i] : rhs[i];
	}
	return result;
}



// specialization for float3


template<typename T>
Vector<T, 3> operator /(T lhs, Vector<T, 3>const& rhs) {
	return Vector<T, 3>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z);
}

template<typename T> Vector<T, 3> operator *(Vector<T, 3>const& lhs, T rhs) {
	return Vector<T, 3>(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs);
}

template<typename T> Vector<T, 3> operator *(Vector<T, 3>const& lhs, Vector<T, 3>const&  rhs) {
	return Vector<T, 3>(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
}

template<typename T> Vector<T, 3> operator /(Vector<T, 3>const& lhs, T rhs) {
	float rcp = 1 / rhs;
	return lhs * rcp;
}

template<typename T> Vector<T, 3> operator /(Vector<T, 3>const& lhs, Vector<T, 3>const&  rhs) {
	return Vector<T, 3>(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
}

template<typename T> Vector<T, 3> operator +(Vector<T, 3>const& lhs, T rhs) {
	return Vector<T, 3>(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs);
}

template<typename T> Vector<T, 3> operator +(Vector<T, 3>const& lhs, Vector<T, 3>const&  rhs) {
	return Vector<T, 3>(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

template<typename T> Vector<T, 3> operator -(Vector<T, 3>const& lhs, T rhs) {
	return Vector<T, 3>(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs);
}

template<typename T> Vector<T, 3> operator -(Vector<T, 3>const& lhs, Vector<T, 3>const&  rhs) {
	return Vector<T, 3>(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

template<typename T> Vector<T, 3> min(Vector<T, 3>const& lhs, Vector<T, 3>const& rhs) {
	return Vector<T, 3>(eastl::min(lhs.x, rhs.x), eastl::min(lhs.y, rhs.y), eastl::min(lhs.z, rhs.z));
}

template<typename T> Vector<T, 3> max(Vector<T, 3>const& lhs, Vector<T, 3>const& rhs) {
	return Vector<T, 3>(eastl::max(lhs.x, rhs.x), eastl::max(lhs.y, rhs.y), eastl::max(lhs.z, rhs.z));
}

template<typename T>
T dot(Vector<T, 3>const& lhs, Vector<T, 3>const& rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

template<typename T>
Vector<T, 3> cross(Vector<T, 3>const& lhs, Vector<T, 3>const& rhs) {
	return{ lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x };
}