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

float vec4_dot(vec4 v, vec4 u)
{
	return (v.v[0] * u.v[0]) + (v.v[1] * u.v[1]) + (v.v[2] * u.v[2]);
}


vec3 vec3_cross(vec3 a, vec3 b)
{
	vec3 t;
	t.x = a.y*b.z - a.z*b.y;
	t.y = a.z*b.x - a.x*b.z;
	t.z = a.x*b.y - a.y*b.x;
    return t;
}

vec3_int vec3_int_cross1(vec3_int a, vec3_int b)
{
	vec3_int t;
	t.v[0] = a.v[1]*b.v[2] - a.v[2]*b.v[1];
	t.v[1] = a.v[2]*b.v[0] - a.v[0]*b.v[2];
	t.v[2] = a.v[0]*b.v[1] - a.v[1]*b.v[0];
	return t;
}

_vec3_int vec3_int_cross2(_vec3_int a, _vec3_int b)
{
    _vec3_int t;
    t.x = a.y*b.z - a.z*b.y;
    t.y = a.z*b.x - a.x*b.z;
    t.z = a.x*b.y - a.y*b.x;
    return t;
}

vec3_int make_vec3_int_from_vec2_fix(vec2_Fix12P4 xy, int z)
{
    vec3_int m = { { xy.x, xy.y, z } };
    return m;
}

clov4 make_vec4_u8_from_vec3_u8(clov3 xyz, unsigned char w)
{
	clov4 m = { { xyz.v[0], xyz.v[1], xyz.v[2], w } };
	return m;
}

