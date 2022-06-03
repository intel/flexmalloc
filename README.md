<a name="top"></a>

# Flexible Memory Allocation Tool
Flexible Memory Allocation Tool for multi-tiered memory systems

## Table of contents

+ [What is the Flexible Memory Allocation Tool?](#what-is-the-flexible-memory-allocation-tool)
+ [Requirements](#requirements)
+ [Status](#status)
+ [Building the tool](#building-the-tool)
+ [Using the tool](#using-the-tool)
+ [Environment variables](#environment-variables)
+ [Copyrights](#copyrights)

## What is the Flexible Memory Allocation Tool?
The Flexible Memory Allocation Tool is an interposition library that allocates dynamic-memory objects into designated memory systems on a multi-tiered memory system based on a user-given configuration without having to alter the application binary.
This tool takes two inputs:
1. Definition of the memory tiers and their respective allocators (e.g. various flavors of memkind)
2. Code-locations of dynamic allocations that are forwarded to specific allocators
 
## Requirements

* Linux OS
* C++ compiler (g++ 4.8 will do)
* Optionally:
  * [binutils](https://www.gnu.org/software/binutils) - to enable human-readable call-stack matching
  * [Memkind](https://github.com/memkind/memkind) - to enable promoting memory allocations to Memkind handled memory (e.g. Intel Optane Persistent Memory or HBM)
  * [PMDK](https://github.com/pmem/pmdk) - takes advantage of fast pmem_copy routines
  * [PAPI](http://icl.utk.edu/papi/software) - collect some performance metrics

## Status

## Building the tool

Create a build directory
```
mkdir build
```      
Go inside that build directory
```
cd build
```

Issue the configure
```
../configure --prefix=INSTALL_DIR --with-memkind=MEMKIND_DIR --with-pmdk=PMDK_DIR
```
and adapt the respective DIRectories to fit your installation. Ensure that you have permissions to install in `INSTALL_DIR`.

Compile and install
```
make && make install
```

## Using the tool

To use the tool, you need to prepare two configuration files.
1. Memory definitions: This file refers to a list of the available memory tiers/subsystems in the machine. For instance, a base configuration for a DRAM-only system (handled by regular posix calls) and with a limit of 4 GB per process would look like:
```
$ cat base-memory-configuration
# Memory configuration for allocator posix
Size 4096 MBytes
```
The following configuration file is for a heterogeneous memory system composed by Intel Optane Persistent Memory (handled by memkind) and DRAM (limited to 1GB per process). In this scenario, the `memkind/pmem` allocator specifies one mount point per socket.
```
# Memory configuration for allocator posix
Size 1024 Mbytes
# Memory configuration for allocator memkind/pmem
@ /mnt/pmem0 @ /mnt/pmem1
```

2. Memory locations: This file refers to a list of pairs composed by call-stacks and the memory tier where the data object shall be allocated. The call-stacks are defined by a sequence of code locations identified by pairs of `file:line` number. For instance, the following allocations file would forward allocations found in lines 252, 253, and 254 in stream-manymallocs.c and invoked from line 342 on libc-start to the posix memory allocator.
```
$ cat stream-manymallocs-locations
# Memory configuration with size 0 bytes on allocator posix
# This is an example. The format is, one line per call-stack, on each line the complete call-stack
# e.g. file1.c:line1 > file2.c:line2 > file3.c:line3 ... > fileN.c:lineN
stream-manymallocs.c:252 > libc-start.c:342 @ posix
stream-manymallocs.c:253 > libc-start.c:342 @ posix
stream-manymallocs.c:254 > libc-start.c:342 @ posix
```

Once you have the configuration files, issue:
```
$INSTALL_DIR/bin/flexmalloc.sh memory-definitions.cfg memory-locations.cfg <binary & params>
```
where `binary and params` refers to the binary to be executed and the parameters being passed to that execution.

## Environment variables

## Copyrights

&copy; 2022 Harald Servat, Intel Corporation

Intel is a trademark of Intel Corporation or its subsidiaries.

\* Other names and brands may be claimed as the property of others.

We welcome anyone interested in using, developing or contributing in Flexible Memory Tool!

Go to [top](#top)
