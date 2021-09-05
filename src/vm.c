#include "clocks/vm.h"

#include <stdio.h>

#include <clocks/common.h>

VM vm;

void init_vm()
{
}

void free_vm()
{
}

static InterpretResult run()
{
#define READ_BYTE()     (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    while (true)
    {
        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
            case OP_CONSTANT:
            {
                Value constant = READ_CONSTANT();
                print_value(constant);
                printf("\n");
                break;
            }

            case OP_RETURN:
            {
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk)
{
    vm.chunk = chunk;
    vm.ip    = vm.chunk->code;
    return run();
}
