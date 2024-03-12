#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

static int check_zero (int n, int *p)
{
	int i;
	int sum;
	for (sum = 0, i = 0; i < n; ++i)
		sum += p[i];
	return sum == 0;
}

int main (int argc, char *argv[])
{
	printf ("Testing malloc:\n");

	{
#define MAX_MALLOC 16
		int i;
		void * ptr[MAX_MALLOC];

		// Sequence of mallocs and then freeing them
		printf ("* malloc test 1\n");
		for (i = 0; i < MAX_MALLOC; ++i)
			ptr[i] = malloc (i*2+1);

		for (i = 0; i < MAX_MALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = 0; i < MAX_MALLOC; ++i)
			free (ptr[i]);

		// Sequence of mallocs and then freing them in reverse order
		printf ("* malloc test 2\n");
		for (i = 0; i < MAX_MALLOC; ++i)
			ptr[i] = malloc (i*4+1);

		for (i = 0; i < MAX_MALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = MAX_MALLOC-1; i >= 0; --i)
			free (ptr[i]);

		// Sequence of malloc-and-free
		printf ("* malloc test 3\n");
		for (i = 0; i < MAX_MALLOC; ++i)
		{
			void * p = malloc (i+1);
			assert (p != NULL);
			free (p);
		}
#undef MAX_MALLOC
	}

	printf ("Testing calloc:\n");
	{
#define MAX_CALLOC 16
		int i;
		int * ptr[MAX_CALLOC];

		// Sequence of callocs, check they're 0 and then freeing them
		printf ("* calloc test 1\n");
		for (i = 0; i < MAX_CALLOC; ++i)
			ptr[i] = (int*) calloc (i+1, sizeof(int));

		for (i = 0; i < MAX_CALLOC; ++i)
			assert (check_zero (i+1, ptr[i]));

		for (i = 0; i < MAX_CALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = 0; i < MAX_CALLOC; ++i)
			free (ptr[i]);

		// Sequence of mallocs, check they're 0 and then freeing them in reverse order
		printf ("* calloc test 2\n");
		for (i = 0; i < MAX_CALLOC; ++i)
			ptr[i] = (int*) calloc (i+2, sizeof(int));

		for (i = 0; i < MAX_CALLOC; ++i)
			assert (check_zero (i+2, ptr[i]));

		for (i = 0; i < MAX_CALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = MAX_CALLOC-1; i >= 0; --i)
			free (ptr[i]);

		// Sequence of calloc-and-free
		printf ("* calloc test 3\n");
		for (i = 0; i < MAX_CALLOC; ++i)
		{
			int *p = (int *) calloc (i+1, sizeof(int));
			assert (check_zero (i, p));
			assert (p != NULL);
			free (p);
		}
#undef MAX_MALLOC
	}

	printf ("Testing realloc:\n");
	{
#define MAX_REALLOC 16
		int i;
		void * ptr[MAX_REALLOC];

		printf ("* realloc test 1\n");
		for (i = 0; i < MAX_REALLOC; ++i)
			ptr[i] = malloc (1+i*2);

		for (i = 0; i < MAX_REALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = 0; i < MAX_REALLOC; ++i)
			ptr[i] = realloc (ptr[i], i*4);

		for (i = 0; i < MAX_REALLOC; ++i)
			free (ptr[i]);

		printf ("* realloc test 2\n");
		for (i = 0; i < MAX_REALLOC; ++i)
			ptr[i] = malloc (3+i*4);

		for (i = 0; i < MAX_REALLOC; ++i)
			assert (ptr[i] != NULL);

		for (i = 0; i < MAX_REALLOC; ++i)
			ptr[i] = realloc (ptr[i], i*2);

		for (i = MAX_REALLOC-1; i >= 0; i--)
			free (ptr[i]);

		printf ("* realloc test 3\n");
		for (i = 0; i < MAX_REALLOC; i++)
		{
			void * p = realloc (NULL, 2+i*4);
			assert (p != NULL);
			free (p);
		}
#undef MAX_REALLOC
	}

	printf ("Testing posix_memalign:\n");
	{
#define MAX_ALLOC 16
		int i;
		void * ptr[MAX_ALLOC];

		// Sequence of posix_memalign with different alignments starting at 8
		printf ("* posix_memalign test 1\n");
		for (i = 0; i < MAX_ALLOC; ++i)
		{
			assert (0 == posix_memalign (&ptr[i], 1 << (i+3), i+1));
		}

		for (i = 0; i < MAX_ALLOC; ++i)
		{
			assert (ptr[i] != NULL);
			assert ( (((long) ptr[i]) & (1 << (i+3) - 1)) == 0 );
		}

		for (i = 0; i < MAX_ALLOC; ++i)
			free (ptr[i]);

		// Sequence of posix_memalign with different alignments starting at 32, and freeing in reverse order
		printf ("* posix_memalign test 2\n");
		for (i = 0; i < MAX_ALLOC; ++i)
		{
			assert (0 == posix_memalign (&ptr[i], 1 << (i+5), i+1));
		}

		for (i = 0; i < MAX_ALLOC; ++i)
		{
			assert (ptr[i] != NULL);
			assert ( (((long) ptr[i]) & (1 << (i+5) - 1)) == 0 );
		}

		for (i = MAX_ALLOC-1; i >= 0; --i)
			free (ptr[i]);

#undef MAX_ALLOC
	}
		

	return EXIT_SUCCESS;
}
