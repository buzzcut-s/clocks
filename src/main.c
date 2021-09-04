#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/debug.h>

int main(int argc, const char* argv[])
{
    Chunk chunk;

    init_chunk(&chunk);
    write_chunk(&chunk, OP_RETURN);

    int constant = add_constant(&chunk, 21.03);
    write_chunk(&chunk, OP_CONSTANT);
    write_chunk(&chunk, constant);

    disassemble_chunk(&chunk, "test chunk");
    free_chunk(&chunk);

    return 0;
}
