#!/bin/bash

FLEXMALLOC_HOME=@__sub_FLEXMALLOC_HOME__@

library=libflexmalloc
if [[ ${FLEXMALLOC_DEBUG} == "1" ]] || [[ ${FLEXMALLOC_DEBUG} == "enabled" ]] || [[ ${FLEXMALLOC_DEBUG} == "yes" ]] ; then
		library=${library}_dbg
fi

if [[ $# -lt 3 ]] ; then
	echo Error! Check for parameters!
	echo 1st parameter: memory definitions
	echo 2nd parameter: data-object allocation locations
	echo 3rd and successive parameters: application process
	exit
fi

if [[ ! -f ${1} ]] ; then
	echo Warning! Cannot locate given definitions file ${1}
fi

if [[ ! -f ${2} ]] ; then
	echo Warning! Cannot locate given locations file ${2}
fi

export FLEXMALLOC_DEFINITIONS=${1}
export FLEXMALLOC_LOCATIONS=${2}

set_LD_PRELOAD="LD_PRELOAD=${FLEXMALLOC_HOME}/lib/${library}.so"
runner="env ${set_LD_PRELOAD}"

mpi_rank="${PMIX_RANK}"
if [[ -z "${mpi_rank}" ]]; then
	mpi_rank="${PMI_RANK}"
fi

# # If MPI rank is 0 or non MPI-execution, set minimum verbosity
if [[ "${mpi_rank}" == "0" || "${mpi_rank}" == "" ]] ; then
	# Set verbose level 1 if no verbosity requested in rank 0
	if [[ "${FLEXMALLOC_VERBOSE}" == "" ]] ; then
		export FLEXMALLOC_VERBOSE=1
	fi
	# Ease the starting of a program using flexmalloc through GDB debugger
	if test -n "${FLEXMALLOC_GDB}"
	then
		tmp_gdb=`mktemp /tmp/flexmalloc_gdb_cmd_file.XXXXXX`
		cat >$tmp_gdb <<EOF
set environment FLEXMALLOC_DEFINITIONS ${1}
set environment FLEXMALLOC_LOCATIONS ${2}
set exec-wrapper env '${set_LD_PRELOAD}'
break main
EOF
		case "${FLEXMALLOC_GDB}" in
			1|enabled|yes)
				runner="gdb -x ${tmp_gdb} --args"
				;;
		esac
	fi
	# Start the application
	${runner} ${@:3}
else
	export FLEXMALLOC_DEBUG=no
	export FLEXMALLOC_VERBOSE=0
	${runner} ${@:3} > /dev/null 2> /dev/null
fi

