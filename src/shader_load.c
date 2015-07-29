
#include "util.h"
#include "shader_load.h"

GLuint LoadShaders(const char* vertex_shader, const char* fragment_shader)
{

    // Create the shaders
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    GLint result = GL_FALSE;
    int info_log_length;

    // Compile Vertex Shader
    DEBUG("Compiling vertex shader...");

    glShaderSource(vertex_shader_id, 1, &vertex_shader, NULL);
    glCompileShader(vertex_shader_id);

    // Check Vertex Shader
    glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

/*
    if (info_log_length > 1) {
        char vertex_shader_error(info_log_length);
        glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, &vertex_shader_error);
        if (result) {
            DEBUG("%s", &vertex_shader_error);
        } else {
            ERROR("Error compiling vertex shader:\n%s", &vertex_shader_error);
        }
    }
*/

    // Compile Fragment Shader
    DEBUG("Compiling fragment shader...");

    glShaderSource(fragment_shader_id, 1, &fragment_shader, NULL);
    glCompileShader(fragment_shader_id);

    // Check Fragment Shader
    glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

/*
    if (info_log_length > 1) {
        char fragment_shader_error(info_log_length);
        glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, fragment_shader_error);
        if (result) {
            DEBUG("%s", &fragment_shader_error);
        } else {
            ERROR("Error compiling fragment shader:\n%s", &fragment_shader_error);
        }
    }
*/

    // Link the program
    DEBUG("Linking program...");

    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    // Check the program
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);

/*
    if (info_log_length > 1) {
        char program_error(info_log_length);
        glGetProgramInfoLog(program_id, info_log_length, NULL, &program_error);
        if (result) {
            DEBUG("%s", &program_error);
        } else {
            ERROR("Error linking shader:\n%s", &program_error);
        }
    }
*/
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}