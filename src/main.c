#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/debug.h>
#include <clocks/vm.h>

int main(int argc, const char* argv[])
{
    init_vm();

    Chunk chunk;

    init_chunk(&chunk);
    write_chunk(&chunk, OP_RETURN, 123);

    int constant = add_constant(&chunk, 21.03);
    write_chunk(&chunk, OP_CONSTANT, 123);
    write_chunk(&chunk, constant, 123);

    disassemble_chunk(&chunk, "test chunk");

    free_vm();
    free_chunk(&chunk);

    return 0;
}
