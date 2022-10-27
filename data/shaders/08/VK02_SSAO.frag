// this method does not get close to the best SSAO implementations, it is very simple
// with regard to its input parameters and can operate just on a naked depth buffer.
#version 460

layout(location = 0) in vec2 texCoord1;

layout(location = 0) out vec4 outColor;

// defining a table with offsets to 8 points to sample around the current fragment 
const vec3 offsets[8] = vec3[8] 
(
	vec3(-0.5, -0.5, -0.5),
	vec3( 0.5, -0.5, -0.5),
	vec3(-0.5,  0.5, -0.5),
	vec3( 0.5,  0.5, -0.5),
	vec3(-0.5, -0.5,  0.5),
	vec3( 0.5, -0.5,  0.5),
	vec3(-0.5,  0.5,  0.5),
	vec3( 0.5,  0.5,  0.5)
);

layout(binding = 0) uniform UniformBuffer
{
	float scale;
	float bias;
	float zNear;
	float zFar;
	float radius;
	float attScale;
	float distScale;
} params;

layout(binding = 1) uniform sampler2D texDepth;
layout(binding = 2) uniform sampler2D texRotation;

// based on http://steps3d.narod.ru/tutorials/ssao-tutorial.html
void main()
{
	vec2 uv = vec2(texCoord1.x, 1.0 - texCoord1.y);

	const float radius    = params.radius;
	const float attScale  = params.attScale;
	const float distScale = params.distScale;
    
	const float zFarMulzNear   = params.zFar * params.zNear;
	const float zFarMinuszNear = params.zFar - params.zNear;
    
	float size = 1.0 / 512.0; // float(textureSize(texDepth, 0 ).x);

	// get Z in eye space
	float Z     = zFarMulzNear / ( texture( texDepth, uv ).x * zFarMinuszNear - params.zFar );

	// we take the aforementioned random rotation's 4x4 texture, tile it across the
	// size of our entire framebuffer, and sample a vec3 value from it, corresponding to
	// the current fragment. This value becomes a normal vector to a random plane. In the
	// loop, we reflect each of our vec3 offsets from this plane, producing a new rSample
	// sampling point in the neighborhood of our area of interest defined by the radius
	// value. The zSample depth value corresponding to this point is sampled from the
	// depth texture and immediately converted to eye space. After that, this value is zero-
	// clipped and scaled using an ad hoc distScale parameter controllable from ImGui:
	float att   = 0.0;
	vec3  plane = 2.0 * texture( texRotation, uv * size / 4.0 ).xyz - vec3( 1.0 );
  
	for ( int i = 0; i < 8; i++ )
	{
		vec3  rSample = reflect( offsets[i], plane );
		float zSample = texture( texDepth, uv + radius*rSample.xy / Z ).x;

		zSample = zFarMulzNear / ( zSample * zFarMinuszNear - params.zFar );
        
		float dist = max( zSample - Z, 0.0 ) / distScale;
		// The occl distance difference is scaled by an arbitrarily selected weight. Further
		// averaging is done using quadratic attenuation, according to the O(dZ)= (dZ >
		// 0) ? 1/(1+dZ^2) : 0 formula. The final scale factor, attScale, is controlled from ImGui:
		float occl = 15.0 * max( dist * (2.0 - dist), 0.0 );
        
		att += 1.0 / ( 1.0 + occl * occl );
	}
    
	att = clamp( att * att / 64.0 + 0.45, 0.0, 1.0 ) * attScale;
	outColor = vec4 ( vec3(att), 1.0 );
}
