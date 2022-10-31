/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main()
{
  // uses a dot product to convert red-green-blue-alpha (RGBA) values to luminance. 
  // Output the values only if the result is brighter than 1.0:
  vec4 Color = vec4( texture(texSampler, texCoord) );

  if ( dot( Color, vec4( 0.33, 0.34, 0.33, 0.0) ) < 1.0 ) Color = vec4( 0.0, 0.0, 0.0, 1.0 );

  outColor = vec4(Color.xyz, 1.0);
//  outColor = vec4(0, 1, 0, 1.0);
}
