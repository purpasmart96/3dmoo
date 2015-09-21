#ifndef VEC4_H
#define VEC4_H

typedef float scalar;

typedef struct  {
	unsigned char v[4];
} clov4;

typedef struct {
	unsigned char v[3];
} clov3;

typedef struct {
    scalar v[4];//x, y, z, w;
} vec4;

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
	int v[3];
} vec3_int;

typedef struct {
	int x,y,z;
} _vec3_int;

typedef struct {
	int v[4];
} vec4_int;

typedef struct {
	signed short x, y, z;
} vec3_Fix12P4;

typedef struct {
    float x, y;
} vec2;

typedef struct {
	signed short x, y;
} vec2_Fix12P4;

vec4 _vec4(scalar, scalar, scalar, scalar);
vec4 vec4_zero();
vec4 vec4_add(vec4, vec4);
vec4 vec4_sub(vec4, vec4);
vec4 vec4_mul(vec4, vec4);
vec4 vec4_muls(scalar, vec4);
float vec4_dot(vec4 v, vec4 u);
vec3 vec3_cross(vec3 a, vec3 b);
vec3_int vec3_int_cross1(vec3_int a, vec3_int b);
_vec3_int vec3_int_cross2(_vec3_int a, _vec3_int b);

vec3_int make_vec3_int_from_vec2_fix(vec2_Fix12P4 xy, int z);
clov4 make_vec4_u8_from_vec3_u8(clov3 xyz, unsigned char w);

vec4 vec4_set(int num, scalar val, vec4 v);
float vec4_get(int num, vec4 v);
float *vec4_getp(int num, vec4 v);


#endif