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

# Build
clocks is built using CMake and requires at least C11. clang-tidy and clang-format config files are also provided.

clocks requires no external dependencies.

```
git clone https://github.com/buzzcut-s/clocks.git
cd clocks/
mkdir build
cd build/
cmake ..
cmake --build .
```

# Usage - REPL
clocks provides a REPL environment to play around in. 

To run:
``` 
cd src/
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
You can pass in a a different const char* argument to the ```linenoise("clocks > ")``` call at ```main.cpp:16:30```[ (here) ](https://github.com/buzzcut-s/clocks/blob/main/src/main.cpp#L16) to change the shell prompt from ```clocks >``` to anything else that your heart desires :D

Debugging can be enabled by defining the ```CLOCKS_DEBUG``` macro. Debugging can also be toggled at a granular level for different components individually (Compiler, Virtual Machine, GC). See ```common.h``` for more details.

Various optimizations can also be toggled using the ```CLOCKS_OPTIMIZATION``` macro. Individual toggle macros are also provided. See ```common.h``` for more details.

# License
clocks is licensed under the MIT License, © 2021 Piyush Kumar. See [LICENSE](https://github.com/buzzcut-s/clocks/blob/main/LICENSE) for more details.

clocks uses the linenoise library. The files ```linenoise.h``` and ```linenoise.c``` are taken from the [linenoise](https://github.com/antirez/linenoise) library, © 2010-2014 Salvatore Sanfilippo and © 2010-2013 Pieter Noordhuis, licensed under the BSD license. See [LICENSE](https://github.com/antirez/linenoise/blob/master/LICENSE) for more details.

Code for original Lox spec is licensed under the MIT License, © 2015 Robert Nystrom. See [LICENSE](https://github.com/munificent/craftinginterpreters/blob/master/LICENSE) for more details.

