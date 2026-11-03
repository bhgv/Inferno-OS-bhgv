#include "lib9.h"
#include "draw.h"
#include "kernel.h"


/* Reduce through a wide positive period so Int32 endpoints cannot overflow before modulo. */
int
drawreplxy(int min, int max, int x)
{
	vlong period, offset;

	period = (vlong)max-min;
	if(period <= 0){
		kwerrstr("drawreplxy: nonpositive interval");
		return min;
	}
	offset = ((vlong)x-min)%period;
	if(offset < 0)
		offset += period;
	return (int)(offset+min);
}


/* Normalize each coordinate through its own independently bounded interval. */
Point
drawrepl(Rectangle rectangle, Point point)
{
	point.x = drawreplxy(rectangle.min.x, rectangle.max.x, point.x);
	point.y = drawreplxy(rectangle.min.y, rectangle.max.y, point.y);
	return point;
}
