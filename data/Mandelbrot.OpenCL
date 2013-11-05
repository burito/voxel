
__kernel void mandelbrot( __write_only image2d_t out, const float time )
{
	// figure out where on the screen this pixel is
	int2 pos = {get_global_id(0), get_global_id(1)};
	float2 size = {get_global_size(0), get_global_size(1)};
	float2 posf = {pos.x, pos.y};

	posf = (posf / size)*3 - 2;
	posf.y += 0.5;


	// iterate on the pixel
	int iterations = 1000;

	float2 z = {0,0};
	float2 c = posf;
	int n;
	for(n=0; n < iterations; n++)
	{
		if((z.x*z.x + z.y*z.y) >= 4.0)break;
		float nz = z.x*z.x - z.y*z.y + c.x;
		z.y = 2.0* z.x*z.y + c.y;
		z.x = nz;
	}


	// make a rainbow
	float o = (float)n / (float)iterations;

	o = o*300  + time;
	o = fmod(o, 6);

	float4 color = {0,0,0, 1};

	if(o < 1){
		color.x = 1;
		color.y = o;
	}
	else if(o < 2)
	{
		color.x = 2 - o;
		color.y = 1;
	}
	else if(o < 3)
	{
		color.y = 1;
		color.z = o - 2;
	}
	else if(o < 4)
	{
		color.y = 4 - o;
		color.z = 1;
	}
	else if(o < 5)
	{
		color.z = 1;
		color.x = o - 4;
	}
	else if(o < 6)
	{
		color.z = 6 - o;
		color.x = 1;
	}

	// give it a black heart
	if( n == iterations)color = (float4)(0,0,0,1);

	write_imagef(out, pos, color);
}

