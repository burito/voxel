
__kernel void gradient( __write_only image2d_t out, const float time )
{
	// figure out where on the screen this pixel is
	int2 pos = {get_global_id(0), get_global_id(1)};
	float2 size = {get_global_size(0), get_global_size(1)};
	float2 posf = {pos.x, pos.y};

	posf = (posf / size);

	float4 colour = {posf.x, posf.y, 0, 1};

	write_imagef(out, pos, colour);
}

