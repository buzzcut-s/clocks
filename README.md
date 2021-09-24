# Overview
> Lox is a dynamically typed programming language with a simple C-like syntax, designed by Robert Nystrom.

clocks is a cross-platform compiler and bytecode virtual machine for the Lox language, written in C99.

Here's how you would implement a program to print the Fibonacci sequence in Lox:

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
- Object oriented
- Automatic memory management (Garbage Collected)
- Data Types : Booleans, Numbers, Strings, Nil
- Lexical scoping
- Control flow statements
- First-class functions and closures
- Classes
- Inheritance (Superclasses)
- A standard library

# Release Build
clocks is built using CMake and requires at least C99. clang-tidy and clang-format config files are also provided.

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
Additionally, you can also pass a .lc file as an argument to the executable.

To compile and run a file called hashmap_bench.lc:
```
./clocks hashmap_bench.lc
```

# (extra)
You can pass in a different const char* argument to the ```linenoise("clocks > ")``` call at ```main.cpp:16:30```[ (here) ](https://github.com/buzzcut-s/clocks/blob/main/src/main.c#L16) to change the shell prompt from ```clocks >``` to anything else that your heart desires :D

Debugging can be enabled by defining the ```CLOCKS_DEBUG``` flag. Debugging can also be toggled at a granular level for different components individually (Compiler, Virtual Machine, GC). See ```common.h``` for more details.

All optimizations are enabled by default, and can be toggled by using the ```CLOCKS_OPTIMIZATION``` flag. Flags for individually toggling optimizations are also provided. See ```common.h``` for more details.

# Examples
See the examples directory.

You can also check out the extensive testing suite written by Bob [here](https://github.com/munificent/craftinginterpreters/tree/master/test).

# Implementation Details
All parts of the scanner, compiler, virtual machine and GC are written from scratch in pure C. 

- clocks has three phases - scanner, compiler, and virtual machine. A data structure joins each pair of phases. Tokens flow from scanner to compiler, and chunks of bytecode from compiler to VM. The VM runs the program.
- A recursive Pratt parser is implemented for parsing. Pratt parsing was described by Vaughan R. Pratt in his paper ["Top Down Operator Precedence"](https://dl.acm.org/doi/10.1145/512927.512931), in 1973. This is used to handle operator precedence and infix expressions during the parsing/compiling phase. (see [```parse_precedence()```](https://github.com/buzzcut-s/clocks/blob/main/src/compiler.c#L581))
- clocks implements a single pass compiler (i.e., parsing and compiling are not separate) which compiles a Lox source program down to bytecode, using 36 bytecode instructions in total. The instructions enum can be found [here](https://github.com/buzzcut-s/clocks/blob/main/include/clocks/chunk.h#L7). The VM then interprets this bytecode.
- The VM is stack based and supports up to 64 call frames.
- The VM uses a hash table as one of its primary data structures. The API can be found in ```table.h```, and the implementation in ```table.c```. The hash table uses open addressing with a linear probing sequence. [FNV-1a](https://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash) is used as the hash function, the details for which can be found [here](http://www.isthe.com/chongo/tech/comp/fnv/). The table's growth factor is defined by ```TABLE_MAX_LOAD```, 0.75 by default, and can be changed [here](https://github.com/buzzcut-s/clocks/blob/main/src/table.c#L9). The linear probing sequence is optimized for performance by using bitmasks when calculating the index (Up to a 43% improvement compared to using the % operator, in one benchmark. For more details see [commit](https://github.com/buzzcut-s/clocks/commit/f703e8e088759293c7a55368cda02710377c60ea))
- Lox is a managed language, i.e., memory is managed automatically. We implement a tracing Mark-Sweep Garbage Collector to reclaim unreachable memory. Collection frequency is dynamically adjusted based on the live heap size. The result is that as the amount of live memory increases, we collect less frequently in order to avoid sacrificing throughput by re-traversing the growing pile of live objects. As the amount of live memory goes down, we collect more frequently so that we don’t lose too much latency by waiting too long. The starting threshold is defined [here](https://github.com/buzzcut-s/clocks/blob/main/src/vm.c#L76) and, the threshold growth factor is defined [here](https://github.com/buzzcut-s/clocks/blob/main/src/memory.c#L19). Optionally, the GC can be stress tested (forcing collection to occur before every allocation) by defining the ```DEBUG_STRESS_GC``` flag. See ```common.h``` for more details.
- The VM interns all strings and stores these unique strings in a hash table. This results in faster method calls and instance fields lookups by name (i.e., all lookups) during runtime, and lower memory usage during compilation, at the cost of requiring more time when the string is created or interned. In addition to that, interning strings also makes string equality, during runtime, extremely fast and trivial, as we only have to compare the pointers - not the actual value.
- Lexical scoping is implementing using a technique called Upvalues, [described](https://www.lua.org/pil/27.3.3.html) by the Lua JIT compiler team, to capture surrounding local variables that a closure needs. An upvalue refers to a local variable in an enclosing function. Every closure maintains an array of upvalues, one for each surrounding local variable that the closure uses. Upvalues are resolved outwards (see [```resolve_upvalue()```](https://github.com/buzzcut-s/clocks/blob/main/src/compiler.c#L662)) This way, for most local variables which do have stack semantics, we allocate them entirely on the stack which is simple and fast. Then, for the few local variables where that doesn't work, we have a second slower path we can opt in to as needed.
- The VM by default uses NaN Tagging to internally represent all values by default, using 8 bytes / Value. This can optionally be disabled if your CPU exhibits some weird behaviour with this optimization turned on by undefining the ```NAN_BOXING``` flag [here](https://github.com/buzzcut-s/clocks/blob/main/include/clocks/common.h#L20). In this other representation, the VM uses a tagged union to internally represent values, using 16 bytes / Value. See ```value.h``` for more details.
- Two "superinstructions" (A single instruction that fuses some series of bytecode instructions, observed frequently, into a single instruction with the same behavior as the entire sequence) ```OpInvoke``` and ```OpSuperInvoke``` have been implemented for optimizing class method and super calls. ```OpInvoke``` fuses ```OpGetProperty``` and ```OpCall```, while ```OpSuperInvoke``` fuses ```OpGetSuper``` and ```OpCall```. These optimizations improved performance by up to 7.6x, in one benchmark. See [commit](https://github.com/buzzcut-s/clocks/commit/9e881db88881d77e8016189aeaf428840bea85cb) for more details.  
- Error messages, with line numbers from the source program, are produced during all three phases. Stack traces are produced to report errors enountered by the VM when interpreting the compiled bytecode.
- clocks provides a complete bytecode disassembler and execution tracer which can be turned on by defining the debugging flags ```DEBUG_PRINT_CODE``` and ```DEBUG_TRACE_EXECUTION```. These come with a performance penalty and are so disabled by default. See ```common.h``` for more details.

# License
clocks is licensed under the MIT License, © 2021 Piyush Kumar. See [LICENSE](https://github.com/buzzcut-s/clocks/blob/main/LICENSE) for more details.

clocks uses the linenoise library. The files ```linenoise.h``` and ```linenoise.c``` are taken from the [linenoise](https://github.com/antirez/linenoise) library, © 2010-2014 Salvatore Sanfilippo and © 2010-2013 Pieter Noordhuis, licensed under the BSD license. See [LICENSE](https://github.com/antirez/linenoise/blob/master/LICENSE) for more details.

Code for original Lox spec is licensed under the MIT License, © 2015 Robert Nystrom. See [LICENSE](https://github.com/munificent/craftinginterpreters/blob/master/LICENSE) for more details.

