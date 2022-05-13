#include <stdio.h>
#include <stdlib.h>
#include "libtester.h"

int main (int argc, char *argv[])
{
	int *p;
	printf ("Hello world\n");
	p = (int*) malloc_lib (16);
	free (p);
	printf ("Bye world\n");
	return 0;
}
