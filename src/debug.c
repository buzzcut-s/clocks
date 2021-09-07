#include "clocks/debug.h"

#include <stdio.h>

#include <clocks/chunk.h>
#include <clocks/common.h>

void disassemble_chunk(const Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int constant_instruction(const char* name, const Chunk* chunk, const int offset)
{
    const uint8_t constant = chunk->code[offset + 1];

    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int simple_instruction(const char* name, const int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int disassemble_instruction(const Chunk* chunk, const int offset)
{
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
        printf("   | ");
    else
        printf("%4d ", chunk->lines[offset]);

    const uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
        case OpConstant:
            return constant_instruction("OpConstant", chunk, offset);

        case OpNil:
            return simple_instruction("OpNil", offset);
        case OpTrue:
            return simple_instruction("OpTrue", offset);
        case OpFalse:
            return simple_instruction("OpFalse", offset);

        case OpEqual:
            return simple_instruction("OpEqual", offset);
        case OpGreater:
            return simple_instruction("OpGreater", offset);
        case OpLess:
            return simple_instruction("OpLess", offset);

        case OpAdd:
            return simple_instruction("OpAdd", offset);
        case OpSubtract:
            return simple_instruction("OpSubtract", offset);
        case OpMultiply:
            return simple_instruction("OpMultiply", offset);
        case OpDivide:
            return simple_instruction("OpDivide", offset);

        case OpNot:
            return simple_instruction("OpNot", offset);

        case OpNegate:
            return simple_instruction("OpNegate", offset);

        case OpPrint:
            return simple_instruction("OpPrint", offset);

        case OpReturn:
            return simple_instruction("OpReturn", offset);

        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
