#include "clocks/debug.h"

#include <stdio.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/object.h>
#include <clocks/value.h>

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

static int byte_instruction(const char* name, const Chunk* chunk, int offset)
{
    const uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char* name, const int sign,
                            const Chunk* chunk, const int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int closure_instruction(const Chunk* chunk, int offset)
{
    offset++;
    const uint8_t constant = chunk->code[offset++];
    printf("%-16s %4d ", "OpClosure", constant);
    print_value(chunk->constants.values[constant]);
    printf("\n");

    const ObjFunction* func = AS_FUNCTION(chunk->constants.values[constant]);
    for (int i = 0; i < func->upvalue_count; i++)
    {
        const int is_local = chunk->code[offset++];
        const int index    = chunk->code[offset++];
        printf("%04d      |                     %s %d\n",
               offset - 2, is_local ? "local" : "upvalue", index);
    }

    return offset;
}

static int invoke_instruction(const Chunk* chunk, int offset)
{
    const uint8_t constant  = chunk->code[offset + 1];
    const uint8_t arg_count = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", "OpInvoke", arg_count, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
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

        case OpPop:
            return simple_instruction("OpPop", offset);

        case OpReadLocal:
            return byte_instruction("OpReadLocal", chunk, offset);
        case OpAssignLocal:
            return byte_instruction("OpAssignLocal", chunk, offset);

        case OpReadGlobal:
            return constant_instruction("OpReadGlobal", chunk, offset);
        case OpDefineGlobal:
            return constant_instruction("OpDefineGlobal", chunk, offset);
        case OpAssignGlobal:
            return constant_instruction("OpAssignGlobal", chunk, offset);

        case OpReadUpvalue:
            return byte_instruction("OpReadUpvalue", chunk, offset);
        case OpAssignUpvalue:
            return byte_instruction("OpAssignUpvalue", chunk, offset);

        case OpSetField:
            return constant_instruction("OpSetField", chunk, offset);
        case OpGetProperty:
            return constant_instruction("OpGetProperty", chunk, offset);

        case OpReadSuper:
            return constant_instruction("OpReadSuper", chunk, offset);

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

        case OpJump:
            return jump_instruction("OpJump", 1, chunk, offset);
        case OpJumpIfFalse:
            return jump_instruction("OpJumpIfFalse", 1, chunk, offset);
        case OpLoop:
            return jump_instruction("OpLoop", -1, chunk, offset);

        case OpCall:
            return byte_instruction("OpCall", chunk, offset);
        case OpInvoke:
            return invoke_instruction(chunk, offset);

        case OpClosure:
            return closure_instruction(chunk, offset);

        case OpCloseUpvalue:
            return simple_instruction("OpCloseUpvalue", offset);

        case OpReturn:
            return simple_instruction("OpReturn", offset);

        case OpInherit:
            return simple_instruction("OpInherit", offset);
        case OpClass:
            return constant_instruction("OpClass", chunk, offset);
        case OpMethod:
            return constant_instruction("OpMethod", chunk, offset);

        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
