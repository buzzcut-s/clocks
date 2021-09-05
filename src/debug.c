#include "clocks/debug.h"

#include <stdio.h>

#include <clocks/chunk.h>
#include <clocks/common.h>

void disassemble_chunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int constant_instruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];

    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int simple_instruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int disassemble_instruction(Chunk* chunk, int offset)
{
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
        printf("   | ");
    else
        printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
        case OpConstant:
            return constant_instruction("OP_CONSTANT", chunk, offset);

        case OpReturn:
            return simple_instruction("OP_RETURN", offset);

        case OpAdd:
            return simple_instruction("OP_ADD", offset);
        case OpSubtract:
            return simple_instruction("OP_SUBTRACT", offset);
        case OpMultiply:
            return simple_instruction("OP_MULTIPLY", offset);
        case OpDivide:
            return simple_instruction("OP_DIVIDE", offset);

        case OpNegate:
            return simple_instruction("OP_NEGATE", offset);

        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
