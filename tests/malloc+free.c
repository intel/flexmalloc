#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
	int *p;
	printf ("Hello world\n");
	p = (int*) malloc (16);
	free (p);
	printf ("Bye world\n");
	return 0;
}
