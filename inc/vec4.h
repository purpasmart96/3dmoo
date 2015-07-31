#ifndef VEC4_H
#define VEC4_H

typedef float scalar;
/*
struct vec4 {
    scalar v[4];//x, y, z, w;
};
struct vec3 {
    scalar v[3];//x, y, z;
};
struct vec2 {
    scalar v[2];//u, v;
};
*/

typedef struct {
	scalar v[4];// x, y, z, w;
} vec4;

typedef struct {
	scalar v[0]; //
} float32;

vec4 _vec4(scalar, scalar, scalar, scalar);
vec4 vec4_zero();
vec4 vec4_add(struct vec4, struct vec4);
vec4 vec4_sub(struct vec4, struct vec4);
vec4 vec4_mul(vec4, vec4);
vec4 vec4_muls(scalar, struct vec4);

vec4 vec4_set(int num, scalar val, vec4 v);
float vec4_get(int num, struct vec4 v);
float *vec4_getp(int num, struct vec4 v);


/*
typedef struct vec4 {
	float x;
	float y;
	float z;
	float w;
};
*/

struct mat4
{
	vec4 c[4];//columns
};


struct vec4_old {
	scalar v[4];//x, y, z, w;
};

typedef struct {
	float x, y, z;
} vec3;

typedef struct {
    float x, y;
} vec2;


#endif