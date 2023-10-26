#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define N 1024*1024

#define TAG1 ((void*) 0xdeadbeef)
#define TAG2 ((void*) 0xbeefdead)

int main (int argc, char *argv[])
{
	size_t s;
	void * ptr;
	uintptr_t c = 0;
	assert ((ptr = malloc (2 * sizeof(void*))) != NULL);
	void **p = (void**) ptr;
	p[0] = TAG1;
	p[1] = TAG2;
	for (s = 2 * sizeof(void*); s < N; s *= 2)
	{
		ptr = realloc (ptr, s);
		if (ptr != NULL)
		{
			c = ((uintptr_t) ptr) ^ c;
			p = (void**) ptr;
			if (p[0] != TAG1)
			{
				fprintf (stderr, "Error! TAG1 not found in p[0] after realloc (, %lu)! -- value found %p\n", s, p[0]);
				free (ptr);
				return EXIT_FAILURE;
			}
			if (p[1] != TAG2)
			{
				fprintf (stderr, "Error! TAG2 not found in p[1] after realloc (, %lu)! -- value found %p\n", s, p[1]);
				free (ptr);
				return EXIT_FAILURE;
			}
		}
	}
	printf ("c = %p\n", (void*) c);
	free (ptr);
	return EXIT_SUCCESS;
}
