#pragma once

#include <deque>
#include <limits>

#include "EasyProfilerWrapper.h"
#include "Vulkan/VulkanCanvas.h"

class LinearGraph
{
public:
	explicit LinearGraph(size_t maxGraphPoints = 256)
		: maxPoints_(maxGraphPoints)
	{
	}

	// maintain the maximum number of points
	void addPoint(float value)
	{
		graph_.push_back(value);
		if (graph_.size() > maxPoints_)
			graph_.pop_front();
	}

	void renderGraph(VulkanCanvas &c, const glm::vec4 &color = vec4(1.0)) const
	{
		// EASY_FUNCTION();

		// first to find minimum and maximum values to normalize the graph into the 0...1 range
		float minfps = std::numeric_limits<float>::max();
		float maxfps = std::numeric_limits<float>::min();

		for (float f : graph_)
		{
			if (f < minfps)
				minfps = f;
			if (f > maxfps)
				maxfps = f;
		}

		const float range = maxfps - minfps;

		// iterate all the points once again and draw them from left to right near the bottom part of the screen
		float x = 0.0;
		vec3 p1 = vec3(0, 0, 0);

		for (float f : graph_)
		{
			const float val = (f - minfps) / range;
			const vec3 p2 = vec3(x, val * 0.15f, 0);
			x += 1.0f / maxPoints_;
			c.line(p1, p2, color);
			p1 = p2;
		}
	}

private:
	std::deque<float> graph_;
	const size_t maxPoints_;
};
