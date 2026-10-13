#pragma once

#include <framework/shader.h>
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()

// The shader must be bound before setting its uniforms.
// Uniform setters, so call sites don't repeat the glUniform* boilerplate.
inline void setUniform(const Shader& shader, const char* name, int value)
{
    glUniform1i(shader.getUniformLocation(name), value);
}
inline void setUniform(const Shader& shader, const char* name, float value)
{
    glUniform1f(shader.getUniformLocation(name), value);
}
inline void setUniform(const Shader& shader, const char* name, const float* value)
{
    glUniform1fv(shader.getUniformLocation(name), 1, value);
}
inline void setUniform(const Shader& shader, const char* name, const glm::vec2& value)
{
    glUniform2fv(shader.getUniformLocation(name), 1, glm::value_ptr(value));
}
inline void setUniform(const Shader& shader, const char* name, const glm::vec4& value)
{
    glUniform4fv(shader.getUniformLocation(name), 1, glm::value_ptr(value));
}
inline void setUniform(const Shader& shader, const char* name, const glm::vec3& value)
{
    glUniform3fv(shader.getUniformLocation(name), 1, glm::value_ptr(value));
}
inline void setUniform(const Shader& shader, const char* name, const glm::mat4& value)
{
    glUniformMatrix4fv(shader.getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}
