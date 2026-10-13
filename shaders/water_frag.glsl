#version 410

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 waterColor = vec3(0.05, 0.35, 0.55);
    fragColor = vec4(waterColor, 1.0);
}
