#version 410

layout(
    std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
  vec3 kd;
  vec3 ks;
  float shininess;
  float transparency;
};

uniform sampler2D texShadow;

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform mat4 lightMVP;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

void main() {
  vec4 fragLightCoord = lightMVP * vec4(fragPosition, 1.0);
  fragLightCoord.xyz /= fragLightCoord.w;
  fragLightCoord.xyz = fragLightCoord.xyz * 0.5 + 0.5;

  float fragLightDepth = fragLightCoord.z;

  vec2 shadowMapCoord = fragLightCoord.xy;
  float shadowMapDepth = texture(texShadow, shadowMapCoord).x;

  vec3 normal = normalize(fragNormal);
  vec3 lightDir = normalize(lightPos - fragPosition);
  float diffuse = max(dot(normal, lightDir), 0.0);
  vec3 diffuseColor = lightColor * kd * diffuse;

  float shadow = 0.0;

  float bias = max(0.0005 * (1.0 - diffuse), 0.0005);

  shadow = (fragLightDepth - bias > shadowMapDepth) ? 1.0 : 0.0;

  if (hasTexCoords) {
    fragColor = vec4(texture(colorMap, fragTexCoord).rgb, 1.0);
  } else if (useMaterial) {
    fragColor = vec4(diffuseColor * (1.0 - shadow), 1.0);
  } else {
    fragColor = vec4(normal, 1.0);
  } // Output color value, change from (1, 0, 0) to something else
}
