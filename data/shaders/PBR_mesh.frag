#version 460

// we require per-frame data from the CPU side using a uniform buffer.
// Texture coordinates, the normal vector, and the fragment's world position are
// obtained from the vertex shader
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec2 tc;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform UniformBuffer
{
	mat4 mvp;
	mat4 mv;
	mat4 m;
	vec4 cameraPos;
} ubo;

layout(binding = 3) uniform sampler2D texAO;
layout(binding = 4) uniform sampler2D texEmissive;
layout(binding = 5) uniform sampler2D texAlbedo;
layout(binding = 6) uniform sampler2D texMetalRoughness;
layout(binding = 7) uniform sampler2D texNormal;

layout(binding = 8) uniform samplerCube texEnvMap;
layout(binding = 9) uniform samplerCube texEnvMapIrradiance;

layout(binding = 10) uniform sampler2D texBRDF_LUT;

#include <data/shaders/PBR.sp>

void main()
{
	// ambient occlusion, emissive color, albedo, metallic
	// factor, and roughness. The last two values are packed into a single texture
	vec4 Kao = texture(texAO, tc);
	vec4 Ke  = texture(texEmissive, tc);
	vec4 Kd  = texture(texAlbedo, tc);
	vec2 MeR = texture(texMetalRoughness, tc).yz;

	vec3 normalSample = texture(texNormal, tc).xyz;

	// To calculate the proper normal mapping effect according to the normal
	// map, we evaluate the normal vector per pixel. We do this in world space. The
	// perturbNormal() function calculates the tangent space per pixel using the
	// derivatives of the texture coordinates, and it is implemented in PBR.sp
	// If you want to disable normal mapping and use only
	// per-vertex normals, just comment out the second line here
	vec3 n = normalize(normal);

	// normal mapping
	n = perturbNormal(n, normalize(ubo.cameraPos.xyz - worldPos), normalSample, tc);

	vec4 mrSample = texture(texMetalRoughness, tc);

	// PBRInfo structure encapsulates multiple inputs used by the various functions in the PBR shading equation
	PBRInfo pbrInputs;
	Ke.rgb = SRGBtoLINEAR(Ke).rgb;
	// image-based lighting
	vec3 color = calculatePBRInputsMetallicRoughness(Kd, n, ubo.cameraPos.xyz, worldPos, mrSample, pbrInputs);
	// one hardcoded light source
	color += calculatePBRLightContribution( pbrInputs, normalize(vec3(-1.0, -1.0, -1.0)), vec3(1.0) );
	// ambient occlusion, Use 1.0 in case there is no ambient occlusion texture available
	color = color * ( Kao.r < 0.01 ? 1.0 : Kao.r );
	// Add the emissive color contribution. Make sure the input emissive texture is
	// converted into the linear color space before use. Convert the resulting color back
	// into the standard RGB (sRGB) color space before writing it into the framebuffer
	color = pow( Ke.rgb + color, vec3(1.0/2.2) );

	out_FragColor = vec4(color, 1.0);
}
