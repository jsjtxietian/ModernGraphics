#include "UtilsMath.h"
#include "UtilsCubemap.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

// If we naively convert the equirectangular projection into cube map faces by iterating
// over its pixels, calculating the Cartesian coordinates for each pixel, and saving the pixel
// into a cube map face using these Cartesian coordinates, we will end up with a texture
// that's been heavily damaged by a Moiré pattern. Here, it's best to do things the other way
// around; that is, iterate over each pixel of the resulting cube map faces, calculate the source
// floating-point equirectangular coordinates corresponding to each pixel, and sample the
// equirectangular texture using bilinear interpolation. This way, the final cube map will be
// free of artifacts. 

using glm::ivec2;
using glm::vec3;
using glm::vec4;

// maps integer coordinates inside a specified cube map face as floating-point normalized coordinates.
vec3 faceCoordsToXYZ(int i, int j, int faceID, int faceSize)
{
	const float A = 2.0f * float(i) / faceSize;
	const float B = 2.0f * float(j) / faceSize;

	if (faceID == 0)
		return vec3(-1.0f, A - 1.0f, B - 1.0f);
	if (faceID == 1)
		return vec3(A - 1.0f, -1.0f, 1.0f - B);
	if (faceID == 2)
		return vec3(1.0f, A - 1.0f, 1.0f - B);
	if (faceID == 3)
		return vec3(1.0f - A, 1.0f, 1.0f - B);
	if (faceID == 4)
		return vec3(B - 1.0f, A - 1.0f, 1.0f);
	if (faceID == 5)
		return vec3(1.0f - B, A - 1.0f, -1.0f);

	return vec3();
}

// calculates the required faceSize, width, and height of the resulting bitmap
Bitmap convertEquirectangularMapToVerticalCross(const Bitmap &b)
{
	if (b.type_ != eBitmapType_2D)
		return Bitmap();

	const int faceSize = b.w_ / 4;

	const int w = faceSize * 3;
	const int h = faceSize * 4;

	Bitmap result(w, h, b.comp_, b.fmt_);

	//  define the locations of individual faces inside the cross
	const ivec2 kFaceOffsets[] =
		{
			ivec2(faceSize, faceSize * 3),
			ivec2(0, faceSize),
			ivec2(faceSize, faceSize),
			ivec2(faceSize * 2, faceSize),
			ivec2(faceSize, 0),
			ivec2(faceSize, faceSize * 2)};

	// Two constants will be necessary to clamp the texture lookup
	const int clampW = b.w_ - 1;
	const int clampH = b.h_ - 1;

	// start iterating over the six cube map faces and each pixel inside each face
	for (int face = 0; face != 6; face++)
	{
		for (int i = 0; i != faceSize; i++)
		{
			for (int j = 0; j != faceSize; j++)
			{
				// Use trigonometry functions to calculate the latitude and longitude coordinates of
				// the Cartesian cube map coordinates
				const vec3 P = faceCoordsToXYZ(i, j, face, faceSize);
				const float R = hypot(P.x, P.y);
				const float theta = atan2(P.y, P.x);
				const float phi = atan2(P.z, R);
				// map the latitude and longitude of the floating-point coordinates inside
				// the equirectangular image
				//	float point source coordinates
				const float Uf = float(2.0f * faceSize * (theta + M_PI) / M_PI);
				const float Vf = float(2.0f * faceSize * (M_PI / 2.0f - phi) / M_PI);
				// get two pairs of integer UV coordinates
				// 4-samples for bilinear interpolation
				const int U1 = clamp(int(floor(Uf)), 0, clampW);
				const int V1 = clamp(int(floor(Vf)), 0, clampH);
				const int U2 = clamp(U1 + 1, 0, clampW);
				const int V2 = clamp(V1 + 1, 0, clampH);
				// fractional part
				const float s = Uf - U1;
				const float t = Vf - V1;
				// fetch 4-samples
				const vec4 A = b.getPixel(U1, V1);
				const vec4 B = b.getPixel(U2, V1);
				const vec4 C = b.getPixel(U1, V2);
				const vec4 D = b.getPixel(U2, V2);
				// bilinear interpolation
				const vec4 color = A * (1 - s) * (1 - t) + B * (s) * (1 - t) + C * (1 - s) * t + D * (s) * (t);
				result.setPixel(i + kFaceOffsets[face].x, j + kFaceOffsets[face].y, color);
			}
		};
	}

	return result;
}

// The layout is 3x4 faces, which makes it possible to calculate the dimensions of the
// resulting cube map as follows
Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap &b)
{
	const int faceWidth = b.w_ / 3;
	const int faceHeight = b.h_ / 4;

	Bitmap cubemap(faceWidth, faceHeight, 6, b.comp_, b.fmt_);
	cubemap.type_ = eBitmapType_Cube;

	const uint8_t *src = b.data_.data();
	uint8_t *dst = cubemap.data_.data();

	/*
			------
			| +Y |
	 ----------------
	 | -X | -Z | +X |
	 ----------------
			| -Y |
			------
			| +Z |
			------
	*/

	// This function is pixel-format agnostic, so it needs to know the size of each pixel in bytes
	const int pixelSize = cubemap.comp_ * Bitmap::getBytesPerComponent(cubemap.fmt_);

	// Iterate over the faces and over every pixel of each face. The order of the cube map
	// faces here corresponds to the order of the OpenGL cube map faces, as defined by
	// the GL_TEXTURE_CUBE_MAP_* constants
	for (int face = 0; face != 6; ++face)
	{
		for (int j = 0; j != faceHeight; ++j)
		{
			for (int i = 0; i != faceWidth; ++i)
			{
				int x = 0;
				int y = 0;

				// Calculate the source pixel position in the vertical cross layout based on the
				// destination cube map face index
				switch (face)
				{
					// GL_TEXTURE_CUBE_MAP_POSITIVE_X
				case 0:
					x = i;
					y = faceHeight + j;
					break;

					// GL_TEXTURE_CUBE_MAP_NEGATIVE_X
				case 1:
					x = 2 * faceWidth + i;
					y = 1 * faceHeight + j;
					break;

					// GL_TEXTURE_CUBE_MAP_POSITIVE_Y
				case 2:
					x = 2 * faceWidth - (i + 1);
					y = 1 * faceHeight - (j + 1);
					break;

					// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
				case 3:
					x = 2 * faceWidth - (i + 1);
					y = 3 * faceHeight - (j + 1);
					break;

					// GL_TEXTURE_CUBE_MAP_POSITIVE_Z
				case 4:
					x = 2 * faceWidth - (i + 1);
					y = b.h_ - (j + 1);
					break;

					// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				case 5:
					x = faceWidth + i;
					y = faceHeight + j;
					break;
				}

				// Copy the pixel and advance to the next one
				memcpy(dst, src + (y * b.w_ + x) * pixelSize, pixelSize);

				dst += pixelSize;
			}
		}
	}

	return cubemap;
}
