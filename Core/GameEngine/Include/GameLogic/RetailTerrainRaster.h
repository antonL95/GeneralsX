#pragma once

namespace RetailTerrain
{
template<typename Row>
void visitCircleRows(int centerY, int radius, Row row)
{
	int halfWidth = 0;
	int halfHeight = radius;
	int error = 2 - 2 * radius;
	while (true)
	{
		if (error + halfHeight > 0)
		{
			if (halfHeight == 0 && radius == 1)
				++halfWidth;
			row(centerY + halfHeight, halfWidth);
			if (halfHeight == 0)
				break;
			row(centerY - halfHeight, halfWidth);
			--halfHeight;
			error += 1 - 2 * halfHeight;
		}
		if (error < halfWidth)
		{
			++halfWidth;
			error += 1 + 2 * halfWidth;
		}
	}
}
}
