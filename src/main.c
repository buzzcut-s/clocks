#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/debug.h>
#include <clocks/vm.h>

int main(int argc, const char* argv[])
{
    init_vm();

    Chunk chunk;

    init_chunk(&chunk);

    int constant = add_constant(&chunk, 21.03);
    write_chunk(&chunk, OpConstant, 123);
    write_chunk(&chunk, constant, 123);

    constant = add_constant(&chunk, 3.4);
    write_chunk(&chunk, OpConstant, 123);
    write_chunk(&chunk, constant, 123);

    write_chunk(&chunk, OpAdd, 123);

    constant = add_constant(&chunk, 5.6);
    write_chunk(&chunk, OpConstant, 123);
    write_chunk(&chunk, constant, 123);

    write_chunk(&chunk, OpDivide, 123);

    write_chunk(&chunk, OpNegate, 123);

    write_chunk(&chunk, OpReturn, 123);

    disassemble_chunk(&chunk, "test chunk");
    interpret(&chunk);

    free_vm();
    free_chunk(&chunk);

    return 0;
}
