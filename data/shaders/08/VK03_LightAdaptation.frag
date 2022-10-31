/**/
#version 460

layout(location = 0) in  vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBuffer { float exposure; float maxWhite; float bloomStrength; float adaptationSpeed; } ubo;

layout(binding = 1) uniform sampler2D currentLuminance;
layout(binding = 2) uniform sampler2D adaptedLuminance;

void main()
{
   float currentLum = texture(currentLuminance, vec2(0.5, 0.5)).x;
   float adaptedLum = texture(adaptedLuminance, vec2(0.5, 0.5)).x;

   // oads a single texel from each input image and computes the adapted luminance using
   // https://google.github.io/filament/Filament.md.html#mjx-eqn-adaptation
   // dt=30.0 is the delta time since the previous frame,
   // which we hardcoded for the sake of simplicity, and adaptationSpeed is a
   // parameter that controls the light-adaptation rate. 
   float newAdaptation = adaptedLum + (currentLum - adaptedLum) * (1.0 - pow(0.98, 30.0 * ubo.adaptationSpeed));

   outColor = vec4(vec3(newAdaptation), 1.0);
}
