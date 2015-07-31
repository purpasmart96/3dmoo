#include "vec4.h"

vec4 _vec4(scalar x, scalar y, scalar z, scalar w)
{
	vec4 v;
	v.v[0] = x;
	v.v[1] = y;
	v.v[2] = z;
	v.v[3] = w;
	return v;
}

vec4 vec4_zero()
{
	vec4 v;
	v.v[0] = 0;
	v.v[1] = 0;
	v.v[2] = 0;
	v.v[3] = 0;
	return v;
}

vec4 vec4_add(vec4 v, vec4 u)
{
	vec4 t;
	t.v[0] = v.v[0] + u.v[0];
	t.v[1] = v.v[1] + u.v[1];
	t.v[2] = v.v[2] + u.v[2];
	t.v[3] = v.v[3] + u.v[3];
	return t;
}

vec4 vec4_sub(vec4 v, vec4 u)
{
	vec4 t;
	t.v[0] = v.v[0] - u.v[0];
	t.v[1] = v.v[1] - u.v[1];
	t.v[2] = v.v[2] - u.v[2];
	t.v[3] = v.v[3] - u.v[3];
	return t;
}

vec4 vec4_mul(vec4 v, vec4 u)
{
	vec4 t;
	t.v[0] = v.v[0] * u.v[0];
	t.v[1] = v.v[1] * u.v[1];
	t.v[2] = v.v[2] * u.v[2];
	t.v[3] = v.v[3] * u.v[3];
	return t;
}

vec4 vec4_muls(scalar s, vec4 v)
{
	vec4 t;
	t.v[0] = s * v.v[0];
	t.v[1] = s * v.v[1];
	t.v[2] = s * v.v[2];
	t.v[3] = s * v.v[3];
	return t;
}
