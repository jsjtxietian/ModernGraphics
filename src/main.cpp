#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>

#include "Etc.h"
#include "EtcImage.h"
#include "EtcFilter.h"
#include "EtcFile.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int main()
{
	// STB can load 8-bit-per-channel images as floating-point images directly via the stbi_loadf()
	// API. However, it will do an automatic gamma correction
	int w, h, comp;
	const uint8_t *img = stbi_load("data/stb_sample.jpg", &w, &h, &comp, 4);

	std::vector<float> rgbaf;

	// Etc2Comp takes floating-point RGBA bitmaps as input, so we have to convert our
	// data, as follows:
	for (int i = 0; i != w * h * 4; i += 4)
	{
		rgbaf.push_back(img[i + 0] / 255.0f);
		rgbaf.push_back(img[i + 1] / 255.0f);
		rgbaf.push_back(img[i + 2] / 255.0f);
		rgbaf.push_back(img[i + 3] / 255.0f);
	}

	// encode the floating-point image into ETC2 format using Etc2Comp
	// don't use alpha transparency, our target format should be RGB8
	const auto etcFormat = Etc::Image::Format::RGB8;
	const auto errorMetric = Etc::ErrorMetric::BT709;

	Etc::Image image(rgbaf.data(), w, h, errorMetric);

	image.Encode(etcFormat, errorMetric, ETCCOMP_DEFAULT_EFFORT_LEVEL, std::thread::hardware_concurrency(), 1024);
	// store compressed texture data that is directly consumable by OpenGL
	Etc::File etcFile(
		"image.ktx",
		Etc::File::Format::KTX,
		etcFormat,
		image.GetEncodingBits(),
		image.GetEncodingBitsBytes(),
		image.GetSourceWidth(),
		image.GetSourceHeight(),
		image.GetExtendedWidth(),
		image.GetExtendedHeight());
	etcFile.Write();

	return 0;
}
