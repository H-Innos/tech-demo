#version 410

uniform mat4 mvp;
uniform float uTime;
uniform float waveSpeed;
uniform float waveStrength;


layout(location = 0) in vec3 position;

void main() {
    // time * wavespeed + x
    // tulemusest sin
    // * wavestrength
    // ja see y komponenti ja liida positionile
    float wave = sin(uTime * waveSpeed + position.x + position.z) * waveStrength;
    vec3 waveOffset = vec3(0.0, wave, 0.0);
    gl_Position = mvp * vec4(position + waveOffset, 1);
}
