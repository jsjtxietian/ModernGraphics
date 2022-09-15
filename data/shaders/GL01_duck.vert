﻿//
#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
};

struct Vertex
{
	float p[3];
	float n[3];
	float tc[2];
};

layout(std430, binding = 1) restrict readonly buffer Vertices
{
	Vertex in_Vertices[];
};

layout(std430, binding = 2) restrict readonly buffer Matrices
{
	mat4 in_ModelMatrices[];
};


vec3 getPosition(int i)
{
	return vec3(in_Vertices[i].p[0], in_Vertices[i].p[1], in_Vertices[i].p[2]);
}

vec3 getNormal(int i)
{
	return vec3(in_Vertices[i].n[0], in_Vertices[i].n[1], in_Vertices[i].n[2]);
}

vec2 getTexCoord(int i)
{
	return vec2(in_Vertices[i].tc[0], in_Vertices[i].tc[1]);
}

struct PerVertex
{
	vec2 uv;
	vec3 normal;
	vec3 worldPos;
};

layout (location=0) out PerVertex vtx;

void main()
{
	mat4 model = in_ModelMatrices[gl_BaseInstance];
	mat4 MVP = proj * view * model;

	vec3 pos = getPosition(gl_VertexID);
	gl_Position = MVP * vec4(pos, 1.0);

	mat3 normalMatrix = mat3(transpose(inverse(model)));

	vtx.uv = getTexCoord(gl_VertexID);
	vtx.normal = getNormal(gl_VertexID) * normalMatrix;
	vtx.worldPos = (model * vec4(pos, 1.0)).xyz;
}
