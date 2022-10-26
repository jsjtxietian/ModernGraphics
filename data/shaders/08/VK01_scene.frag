//
#version 460

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec4 inShadowCoord;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBuffer
{
	mat4 mvp;
	mat4 model;
	mat4 lightMVP;
	vec4 cameraPos;
	vec4 lightPos;
	float meshScale;
} ubo;

layout (binding = 3) uniform sampler2D textureShadow;
layout (binding = 4) uniform sampler2D textureAlbedo;

// percentage-closer filtering (PCF) technique. 
// PCF is a method to reduce the aliasing of shadow mapping by
// averaging the results of multiple depth comparisons in the fragment shader. 
// this function performs averaging of multiple depth-comparison operations.
// The kernelSize argument is expected to be an odd number and defines the
// dimensions in texels of the kernelSize * kernelSize averaging square. Note
// that we average not the results of depth-map sampling at adjacent locations but the
// results of multiple comparisons of the depth value of the current fragment (in the
// light space) with sampled depth values obtained from the shadow map:
float PCF(int kernelSize, vec2 shadowCoord, float depth)
{
	float size = 1.0 / float( textureSize(textureShadow, 0 ).x );
	float shadow = 0.0;
	int range = kernelSize / 2;
	for ( int v=-range; v<=range; v++ ) 
		for ( int u=-range; u<=range; u++ )
			shadow += (depth >= texture( textureShadow, shadowCoord + size * vec2(u, v) ).r) ? 1.0 : 0.0;
	return shadow / (kernelSize * kernelSize);
}

// hides all the shadowing machinery and returns
// a single shadow factor for the current fragment. The shadowCoord value is the
// position of the current fragment in the light's clip-space, interpolated from the vertex
// shader. We check if the fragment is within the -1.0...+1.0 clip-space Z range
// and call the PCF() function to evaluate the result. The depth value of the fragment is
// adjusted by depthBias to reduce shadow-acne artifacts while self-shadowing scene
// objects, which result from Z-fighting. This parameter is somewhat ad hoc and tricky
// and requires significant fine-tuning in real-world applications. A more sophisticated
// approach to fight shadow acne might be to modify bias values according to the slope
// of the surface.
float shadowFactor(vec4 shadowCoord)
{
	vec4 shadowCoords4 = shadowCoord / shadowCoord.w;

	if (shadowCoords4.z > -1.0 && shadowCoords4.z < 1.0)
	{
		float depthBias = -0.001;
		float shadowSample = PCF( 13, shadowCoords4.xy, shadowCoords4.z + depthBias );
		return mix(1.0, 0.3, shadowSample);
	}

	return 1.0; 
}

void main()
{
	outColor = vec4(shadowFactor(inShadowCoord) * texture(textureAlbedo, uv).rgb, 1.0);
}
