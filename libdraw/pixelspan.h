/* Locate a signed pixel coordinate's containing byte without signed shifts. */
static vlong
drawpixelbyte(int x, int depth)
{
	vlong value, offset;

	value = (vlong)x*depth;
	offset = value/8;
	if(value%8 < 0)
		offset--;
	return offset;
}


/* Validate byte extents before narrowing or multiplying a caller's rectangle. */
static int
drawpixelspan(Rectangle r, int depth, int capacity, int *stride, int *total)
{
	vlong right, first, last, rows, bytes;

	if(depth <= 0 || depth > 32 || capacity < 0 ||
	   r.max.x < r.min.x || r.max.y < r.min.y)
		return 0;
	*stride = *total = 0;
	if(r.max.x == r.min.x || r.max.y == r.min.y)
		return 1;
	right = (vlong)r.max.x*depth;
	first = drawpixelbyte(r.min.x, depth);
	last = right/8;
	if(right%8 > 0)
		last++;
	bytes = last-first;
	rows = (vlong)r.max.y-r.min.y;
	if(bytes <= 0 || bytes > capacity || rows > capacity/bytes)
		return 0;
	*stride = bytes;
	*total = rows*bytes;
	return 1;
}
