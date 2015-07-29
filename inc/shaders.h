


const char *g_vertex_shader = "#version 150 core\n"

                              "in vec2 vert_position;\n"
                              "in vec2 vert_tex_coord;\n"
                              "out vec2 frag_tex_coord;\n"

                            // This is a truncated 3x3 matrix for 2D transformations:
                            // The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
                            // The third column performs translation.
                            // The third row could be used for projection, which we don't need in 2D. It hence is assumed to
                            // implicitly be [0, 0, 1]
                              "uniform mat3x2 modelview_matrix;\n"

                              "void main()\n"
                              "{\n"
                            // Multiply input position by the rotscale part of the matrix and then manually translate by
                            // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
                            // to `vec3(vert_position.xy, 1.0)`
                              "gl_Position = vec4(mat2(modelview_matrix)"
                              "* vert_position + modelview_matrix[2], 0.0, 1.0);\n"
                              "frag_tex_coord = vert_tex_coord;\n"
                              "}\n";


const char *g_fragment_shader = "#version 150 core\n"

                                "in vec2 frag_tex_coord;\n"
                                "out vec4 color;\n"

                                "uniform sampler2D color_texture;\n"

                                "void main()\n"
                                "{\n"
                                "color = texture(color_texture, frag_tex_coord);\n"
                                "}\n";