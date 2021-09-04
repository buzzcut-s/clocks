#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/debug.h>

int main(int argc, const char* argv[])
{
    Chunk chunk;

    init_chunk(&chunk);
    write_chunk(&chunk, OP_RETURN);

    disassemble_chunk(&chunk, "test chunk");
    free_chunk(&chunk);

    return 0;
}
