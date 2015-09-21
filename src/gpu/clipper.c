/*
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
* Copyright (C) 2014 - Normmatt
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//this is based on citra, copyright (C) 2014 Tony Wasserka.

#include "util.h"
#include "arm11.h"
#include "handles.h"
#include "mem.h"
#include "gpu.h"
#include "vec4.h"

void InitScreenCoordinates(struct OutputVertex *vtx)
{
    struct {
        int corner_x;
        int corner_y;
        float width;
        float height;
    } viewport;

    struct {
        float scale;
        float offset;
    } depthmap;

    viewport.corner_x = 1.0 * (GPU_Regs[GL_VIEWPORT] & 0xFFFF);
    viewport.corner_y = 1.0 * ((GPU_Regs[GL_VIEWPORT] >> 16) & 0xFFFF);
    f24to32(GPU_Regs[VIEWPORT_WIDTH], &viewport.width);
    f24to32(GPU_Regs[VIEWPORT_HEIGHT], &viewport.height);

    f24to32(GPU_Regs[DEPTH_SCALE], &depthmap.scale);
    f24to32(GPU_Regs[DEPTH_OFFSET], &depthmap.offset);

    float inv_w = (1.f) / vtx->position.v[3];
    vtx->color.v[0] *= inv_w;
    vtx->color.v[1] *= inv_w;
    vtx->color.v[2] *= inv_w;
    vtx->color.v[3] *= inv_w;

    vtx->texcoord0.x *= inv_w;
    vtx->texcoord0.y *= inv_w;
    vtx->texcoord1.x *= inv_w;
    vtx->texcoord1.y *= inv_w;
    vtx->texcoord2.x *= inv_w;
    vtx->texcoord2.y *= inv_w;
    vtx->position.v[3] = inv_w;

	// TODO: Not sure why the viewport width needs to be divided by 2 but the viewport height does not
    vtx->screenpos.x = (vtx->position.v[0] + inv_w + 1.0f) * viewport.width + viewport.corner_x;
    vtx->screenpos.y = (vtx->position.v[1] + inv_w + 1.0f) * viewport.height + viewport.corner_y;
    vtx->screenpos.z = depthmap.offset + vtx->position.v[2] * inv_w * depthmap.scale;
}

#define max_vertices 10

#define EPSILON 0.000001
#define zero 0
#define one 1.0

struct OutputVertex buffer_a[max_vertices];

struct OutputVertex buffer_b[max_vertices];

//#define edgesnumb 8
#define edgesnumb 7
vec4 edges[edgesnumb] = {
    { one, zero, zero, -one }, 
    { -one, zero, zero, -one },
    { zero, one, zero, -one },
    { zero, -one, zero, -one },
    { zero, zero, one, zero },
    { zero, zero, -one, -one },
    { zero, zero, zero, -one },
    //{ zero, zero, zero, EPSILON } //this is not how it works at last some times
};

bool PointIsOnLine(vec4* vLineStart, vec4* vLineEnd, vec4* vPoint)
{
    float fSX;
    float fSY;
    float fSZ;

    fSX = (vPoint->v[0] - vLineStart->v[0]) / vLineEnd->v[0];
    fSY = (vPoint->v[1] - vLineStart->v[1]) / vLineEnd->v[1];
    fSZ = (vPoint->v[2] - vLineStart->v[2]) / vLineEnd->v[2];

    if (fSX == fSY && fSY == fSZ)
        return true;

    return false;
}

#define Lerp(factor,v0,v1,output)                                                                             \
    output.position.v[0] = v0.position.v[0] * (1.f - factor) + v1.position.v[0] * factor;                     \
    output.position.v[1] = v0.position.v[1] * (1.f - factor) + v1.position.v[1] * factor;                     \
    output.position.v[2] = v0.position.v[2] * (1.f - factor) + v1.position.v[2] * factor;                     \
    output.position.v[3] = v0.position.v[3] * (1.f - factor) + v1.position.v[3] * factor;                     \
    output.color.v[0] = v0.color.v[0] * (1.f - factor) + v1.color.v[0] * factor;                              \
    output.color.v[1] = v0.color.v[1] * (1.f - factor) + v1.color.v[1] * factor;                              \
    output.color.v[2] = v0.color.v[2] * (1.f - factor) + v1.color.v[2] * factor;                              \
    output.color.v[3] = v0.color.v[3] * (1.f - factor) + v1.color.v[3] * factor;                              \
    output.texcoord0.x = v0.texcoord0.x * (1.f - factor) + v1.texcoord0.x * factor;                  \
    output.texcoord0.y = v0.texcoord0.y * (1.f - factor) + v1.texcoord0.y * factor;                  \
    output.texcoord1.x = v0.texcoord1.x * (1.f - factor) + v1.texcoord1.x * factor;                  \
    output.texcoord1.y = v0.texcoord1.y * (1.f - factor) + v1.texcoord1.y * factor;                  \
    output.texcoord0_w = v0.texcoord0_w * (1.f - factor) + v1.texcoord0_w * factor;                           \
    output.View.x = v0.View.x * (1.f - factor) + v1.View.x * factor;                                 \
    output.View.y = v0.View.y * (1.f - factor) + v1.View.y * factor;                                 \
    output.View.z = v0.View.z * (1.f - factor) + v1.View.z * factor;                                 \
    output.texcoord2.x = v0.texcoord2.x * (1.f - factor) + v1.texcoord2.x * factor;                  \
    output.texcoord2.y = v0.texcoord2.y * (1.f - factor) + v1.texcoord2.y * factor;

#define GetIntersection(v0, v1,edge,output)                                                                                        \
    float dp = (v0.position.v[0] * edge.v[0] + v0.position.v[1] * edge.v[1] + v0.position.v[2] * edge.v[2] + v0.position.v[3] * edge.v[3]); /*DOT*/    \
    float dp_prev = (v1.position.v[0] * edge.v[0] + v1.position.v[1] * edge.v[1] + v1.position.v[2] * edge.v[2] + v0.position.v[3] * edge.v[3]);       \
    float factor = dp_prev / (dp_prev - dp);                                                                                       \
    Lerp(factor, v0, v1,output);                                                                 

#define IsInsidev4(v1,v2) ((v1.position.v[0]*v2.v[0] + v1.position.v[1]*v2.v[1] + v1.position.v[2]*v2.v[2] + v1.position.v[3]*v2.v[3]) <= 0.f)
#define IsOutsidev4(v1,v2) ((v1.position.v[0]*v2.v[0] + v1.position.v[1]*v2.v[1] + v1.position.v[2]*v2.v[2] + v1.position.v[3]*v2.v[3]) > 0.f)
void Clipper_ProcessTriangle(struct OutputVertex *v0, struct OutputVertex *v1, struct OutputVertex *v2)
{
    if (PointIsOnLine(&v0->position, &v1->position, &v2->position)) //the algo dose not work for them
        return;
    // Simple implementation of the Sutherland-Hodgman clipping algorithm.
    u32 input_list_num = 0;
    struct OutputVertex *output_list = &buffer_a;
    struct OutputVertex *input_list = &buffer_b;
    struct OutputVertex *input_list_temp;
    memcpy(&output_list[0], v0, sizeof(struct OutputVertex));
    memcpy(&output_list[1], v1, sizeof(struct OutputVertex));
    memcpy(&output_list[2], v2, sizeof(struct OutputVertex));
    u32 output_list_num = 3;
    for (int i = 0; i < edgesnumb; i++)
    {
        //ouput is the new input
        input_list_temp = input_list;
        input_list = output_list;
        output_list = input_list_temp;
        input_list_num = output_list_num;
        output_list_num = 0;

        struct OutputVertex* reference_vertex = &input_list[input_list_num - 1]; //back
        for (u32 j = 0; j < input_list_num; j++)
        {
            // NOTE: This algorithm changes vertex order in some cases!
            float test = input_list[j].position.v[0] * edges[i].v[0] + input_list[j].position.v[1]
            * edges[i].v[1] + input_list[j].position.v[2] * edges[i].v[2] + input_list[j].position.v[3] * edges[i].v[3];
            if (IsInsidev4(input_list[j], edges[i])) {
                if (IsOutsidev4((*reference_vertex), edges[i])) {
                    GetIntersection(input_list[j], (*reference_vertex), edges[i], output_list[output_list_num]);
                    output_list_num++;
                }
                memcpy(&output_list[output_list_num], &input_list[j], sizeof(struct OutputVertex));
                output_list_num++;
            }
            else if (IsInsidev4((*reference_vertex), edges[i])) {
                GetIntersection(input_list[j], (*reference_vertex), edges[i], output_list[output_list_num]);
                output_list_num++;
            }

            reference_vertex = &input_list[j];
        }
        // Need to have at least a full triangle to continue...
        if (output_list_num < 3)
            return;
    }
    InitScreenCoordinates(&output_list[0]);
    InitScreenCoordinates(&output_list[1]);

    if (output_list_num > 2) {
        for (u32 i = 0; i < output_list_num - 2; i++) {
            struct OutputVertex* vtx0 = &output_list[0];
            struct OutputVertex* vtx1 = &output_list[i + 1];
            struct OutputVertex* vtx2 = &output_list[i + 2];
            InitScreenCoordinates(vtx2);
            GPUDEBUG(
                "Triangle %d/%d at position (%.3f, %.3f, %.3f, %.3f), "
                "(%.3f, %.3f, %.3f, %.3f), (%.3f, %.3f, %.3f, %.3f) and "
                "screen position (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)\n",
                i, output_list_num,
                vtx0->position.v[0],  vtx0->position.v[1],  vtx0->position.v[2], vtx0->position.v[3],
                vtx1->position.v[0],  vtx1->position.v[1],  vtx1->position.v[2], vtx1->position.v[3],
                vtx2->position.v[0],  vtx2->position.v[1],  vtx2->position.v[2], vtx2->position.v[3],
                vtx0->screenpos.x, vtx0->screenpos.y, vtx0->screenpos.z,
                vtx1->screenpos.x, vtx1->screenpos.y, vtx1->screenpos.z,
                vtx2->screenpos.x, vtx2->screenpos.y, vtx2->screenpos.z);
            rasterizer_ProcessTriangle(vtx0, vtx1, vtx2);

            //HACK
            //rasterizer_ProcessTriangle(vtx2, vtx1, vtx0);
        }
    }
}