* How to use auto-hbwmalloc package

  Simply preload the libauto_hbwmalloc.so using
    LD_PRELOAD=<dir>/lib/libauto_hbwmalloc.so
  and then run the binary. If you run an MPI application, you'd probably want
  not to intercept the mpi launcher. You can achieve that by setting the
  LD_PRELOAD in a intercept.sh file (for instance) and run the application like
    mpirun -np <N> ./intercept.sh <mpi-binary> <args>


* Supported environment variables

  AUTO_HBWMALLOC_VERBOSE=n
  where n can be an integral from 0 to 4 that specifies the level of verbosity
  of the package.

  AUTO_HBWMALLOC_LOCATIONS=f
  where f is a file containing a list of call-stacks to be intercepted and
  allocated through the specified allocator in AUTO_HBWMALLOC_ALLOCATOR

  AUTO_HBWMALLOC_MAX_SIZE=s
  use allocator if the malloc/realloc size is smaller or equal than s bytes
  This option supersedes size ranges provided in the locations file

  AUTO_HBWMALLOC_MIN_SIZE=s
  use allocator if the malloc/realloc size is larger or equal than s bytes
  This option supersedes size ranges provided in the locations file

  AUTO_HBWMALLOC_COMPARE_WHOLE_PATH=b
  where b is either {0/no} or {1/yes} and instructs the package to check for
  the whole path in the call-stacks
