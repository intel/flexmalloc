#!/bin/bash

FLEXMALLOC_HOME=@__sub_FLEXMALLOC_HOME__@

library=libcounter

# # If MPI rank is 0 or non MPI-execution, set minimum verbosity
if [[ "${PMI_RANK}" == "0" || "${PMI_RANK}" == "" ]] ; then
	LD_PRELOAD=${FLEXMALLOC_HOME}/lib/${library}.so ${@}
else 
	LD_PRELOAD=${FLEXMALLOC_HOME}/lib/${library}.so ${@} > /dev/null 2> /dev/null
fi

