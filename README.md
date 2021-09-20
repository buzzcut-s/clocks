# Overview
> Lox is a dynamically typed programming language with a simple C-like syntax, designed by Robert Nystrom.

clocks is a cross-platform compiler and bytecode virtual machine for the Lox language, written in C.

Here's how you would implement a program to print the fibonacci sequence in Lox:

```
fun fib(n) 
{
    if (n < 2) 
        return n;
    return fib(n - 2) + fib(n - 1);
}

print fib(35);
```

# Language Features
- Dynamically typed
- Object Oriented
- Automatic memory management (Garbage Collected)
- Data Types : Booleans, Numbers, Strings, Nil
- Lexical scoping
- Control flow statements
- First-class functions and closures
- Classes
- Inheritance (Superclasses)
- The Standard Library

# Release Build
clocks is built using CMake and requires at least C11. clang-tidy and clang-format config files are also provided.

clocks requires no external dependencies.

```
git clone https://github.com/buzzcut-s/clocks.git
cd clocks/
mkdir build
cd build/
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd src/
```

The binary is built in the ```clocks/build/src/``` directory

# Usage - REPL
clocks provides a REPL environment to play around in. 

To run:
``` 
./clocks
```
After executing, use the ```clocks >``` shell to provide input.

# Usage - File
Additionally you can also pass a .lc file as an argument to the executable.

To compile and run a file called hashmap_bench.lc:
```
./clocks hashmap_bench.lc
```

# (extra)
You can pass in a a different const char* argument to the ```linenoise("clocks > ")``` call at ```main.cpp:16:30```[ (here) ](https://github.com/buzzcut-s/clocks/blob/main/src/main.c#L16) to change the shell prompt from ```clocks >``` to anything else that your heart desires :D

Debugging can be enabled by defining the ```CLOCKS_DEBUG``` macro. Debugging can also be toggled at a granular level for different components individually (Compiler, Virtual Machine, GC). See ```common.h``` for more details.

Various optimizations can also be toggled using the ```CLOCKS_OPTIMIZATION``` macro. Individual toggle macros are also provided. See ```common.h``` for more details.

# Examples
See the examples directory.

Also checkout the extensive testing suite written by Bob [here](https://github.com/munificent/craftinginterpreters/tree/master/test).

# Implementation Details
All parts of the compiler, virtual machine and GC are written from scratch in pure C. 

- We use single pass compiler (ie, parsing and compiling are not separate) which compiles a Lox source program down to bytecode, using 36 bytecode instructions in total. The instructions enum can be found [here](https://github.com/buzzcut-s/clocks/blob/main/include/clocks/chunk.h#L7). The VM then interprets this bytecode.
- The VM is stack based and supports upto 64 call frames.
- The VM uses a hash table as one of it's primary data structure. The API can be found in ```table.h```, and the implementation in ```table.c```. The hash table uses open addressing with a linear probing sequence. [FNV-1a](https://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash) is used as the hash function, the details for which can be found [here](http://www.isthe.com/chongo/tech/comp/fnv/). The table's growth factor is defined by ```TABLE_MAX_LOAD```, 0.75 by default, and can be changed [here](https://github.com/buzzcut-s/clocks/blob/main/src/table.c#L9). The linear probing sequence is optimized for performance by using bitmasks when calculating the index (Upto 43% improvement compared to using the % operator, in one benchmark. For more details see [commit](https://github.com/buzzcut-s/clocks/commit/f703e8e088759293c7a55368cda02710377c60ea))
- Lox is a managed language, and uses a tracing Mark-Sweep Garbage Collector to reclaim back unreachable memory. Collection frequency is dynamically adjusted based on the live size of the heap. The result is that as the amount of live memory increases, we collect less frequently in order to avoid sacrificing throughput by re-traversing the growing pile of live objects. As the amount of live memory goes down, we collect more frequently so that we don’t lose too much latency by waiting too long. The starting threshold is defined [here](https://github.com/buzzcut-s/clocks/blob/main/src/vm.c#L76) and the threshold growth factor is defined [here](https://github.com/buzzcut-s/clocks/blob/main/src/memory.c#L19).
- The VM interns all strings and stores all these unique strings in a hash table. This results in faster name lookups during runtime. Interning all strings also makes string equality trivial during runtime, as we only have to compare the pointers - not the actual value.
- Lexical scoping is implementing using a technique called Upvalues, [described](https://www.lua.org/pil/27.3.3.html) by the Lua JIT compiler team, to capture surrounding local variables that a closure needs. An upvalue refers to a local variable in an enclosing function. Every closure maintains an array of upvalues, one for each surrounding local variable that the closure uses. Upvalues are resolved outwards (see [```resolve_upvalue()```](https://github.com/buzzcut-s/clocks/blob/main/src/compiler.c#L662)) This way, for most variables which do have stack semantics, we allocate them entirely on the stack which is simple and fast. Then, for the few local variables where that doesn't work, we have a second slower path we can opt in to as needed.
- The VM by default uses NaN Tagging to represent values internally by default, using 8 bytes / Value. This can optionally be disabled if your CPU exhibits some weird behaviour with this optimization turned on by toggling the ```NAN_BOXING``` macro [here](https://github.com/buzzcut-s/clocks/blob/main/include/clocks/common.h#L20). In this other representation, the VM uses a tagged union to represent all values, with 16 bytes / Value. See ```value.h``` for more details on how the VM represents values internally.
- Error messages, with line numbers, are produced at both stages - lexing and parsing/compiling. Stack traces are also produced when there is an error during execution.
- clocks provides a complete bytecode disassembler and which can be turned on by defining the debugging macros ```DEBUG_PRINT_CODE``` and ```DEBUG_TRACE_EXECUTION```. These come with a performance penalty and are so disabled by default. See ```common.h``` for more details.

# License
clocks is licensed under the MIT License, © 2021 Piyush Kumar. See [LICENSE](https://github.com/buzzcut-s/clocks/blob/main/LICENSE) for more details.

clocks uses the linenoise library. The files ```linenoise.h``` and ```linenoise.c``` are taken from the [linenoise](https://github.com/antirez/linenoise) library, © 2010-2014 Salvatore Sanfilippo and © 2010-2013 Pieter Noordhuis, licensed under the BSD license. See [LICENSE](https://github.com/antirez/linenoise/blob/master/LICENSE) for more details.

Code for original Lox spec is licensed under the MIT License, © 2015 Robert Nystrom. See [LICENSE](https://github.com/munificent/craftinginterpreters/blob/master/LICENSE) for more details.

