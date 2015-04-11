#include <stdio.h>

int main(void)
{
	int byte;
	int i;
	char rgba[4];
	
	while( 1 )
	{
		for(i=0; i<4; i++)
		{
			byte = getc(stdin);
			if(byte == EOF)return 0;
			rgba[i] = byte;
		}
		
		putc(rgba[2], stdout);
		putc(rgba[1], stdout);
		putc(rgba[0], stdout);
		putc(rgba[3], stdout);

		putc(0, stdout);
		putc(0, stdout);
		putc(0, stdout);
		putc(0, stdout);
	}
	return 0;
}
