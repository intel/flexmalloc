#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define N 1024*1024

int main (int argc, char *argv[])
{
	size_t s = 1;
	void * ptr = NULL;
	uintptr_t c = 0;

	for ( ; s < N; ++s)
	{
		ptr = realloc (ptr, s);
		c = ((uintptr_t) ptr) ^ c;
	}
	printf ("c = %p\n", (void*)c);
	free (ptr);
	return EXIT_SUCCESS;
}
