// grid lines are rendered based on how fast the uv coordinates
// change in the image space to avoid the Moiré pattern
// need screen space derivatives

float log10(float x)
{
	return log(x) / log(10.0);
}

float satf(float x)
{
	return clamp(x, 0.0, 1.0);
}

vec2 satv(vec2 x)
{
	return clamp(x, vec2(0.0), vec2(1.0));
}

float max2(vec2 v)
{
	return max(v.x, v.y);
}

vec4 gridColor(vec2 uv)
{
	vec2 dudv = vec2(
		length(vec2(dFdx(uv.x), dFdy(uv.x))),
		length(vec2(dFdx(uv.y), dFdy(uv.y))));

	// The gridMinPixelsBetweenCells value controls how fast we
	// want our LOD level to increase. In this case, it is the minimum number of pixels
	// between two adjacent cell lines of the grid:
	float lodLevel = max(0.0, log10((length(dudv) * gridMinPixelsBetweenCells) / gridCellSize) + 1.0);

	// a fading factor to render smooth
	// transitions between the adjacent levels. This can be obtained by taking a fractional
	// part of the floating-point LOD level
	float lodFade = fract(lodLevel);

	// calculate the cell size for each LOD.
	// cell sizes for lod0, lod1 and lod2
	// instead of calculating pow() three times, can calculate it for lod0 only,
	// and multiply each subsequent LOD cell size by 10.0
	float lod0 = gridCellSize * pow(10.0, floor(lodLevel));
	float lod1 = lod0 * 10.0;
	float lod2 = lod1 * 10.0;

	// increase the screen coverage of our lines
	// each anti-aliased line covers up to 4 pixels
	dudv *= 4.0;

	// get a coverage alpha value that corresponds to each calculated
	// LOD level of the grid. To do that, we calculate the absolute distances to the cell line
	// centers for each LOD and pick the maximum coordinate
	// blend between falloff colors to handle LOD transition
	// calculate absolute distances to cell line centers for each lod and pick max X/Y to get coverage alpha value
	float lod0a = max2(vec2(1.0) - abs(satv(mod(uv, lod0) / dudv) * 2.0 - vec2(1.0)));
	float lod1a = max2(vec2(1.0) - abs(satv(mod(uv, lod1) / dudv) * 2.0 - vec2(1.0)));
	float lod2a = max2(vec2(1.0) - abs(satv(mod(uv, lod2) / dudv) * 2.0 - vec2(1.0)));

	// Nonzero alpha values represent non-empty transition areas of the grid. Let's blend
	// between them using two colors to handle the LOD transitions
	vec4 c = lod2a > 0.0 ? gridColorThick : lod1a > 0.0 ? mix(gridColorThick, gridColorThin, lodFade)
														: gridColorThin;

	// make the grid disappear when it is far away from the camera.
	// calculate opacity falloff based on distance to grid extents
	float opacityFalloff = (1.0 - satf(length(uv) / gridSize));
	
	// blend between LOD level alphas and scale with opacity falloff
	c.a *= (lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a
											  : (lod0a * (1.0 - lodFade))) *
		   opacityFalloff;

	return c;
}
