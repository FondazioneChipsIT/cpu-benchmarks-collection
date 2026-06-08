/* Cheshire port: fix unaligned accesses in lz77_compress/lz77_decompress.
 * CVA6 does not support unaligned halfword stores/loads.
 * We replace the uint16_t pointer casts with byte-by-byte accesses. */

#include "libmin.h"
#include "lamia-txt.h"

/* Safe unaligned 16-bit read */
static inline uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0]) | ((uint16_t)(p[1]) << 8);
}

/* Safe unaligned 16-bit write */
static inline void write_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

/* Safe unaligned 32-bit read */
static inline uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0]) | ((uint32_t)(p[1]) << 8) |
           ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24);
}

/* Safe unaligned 32-bit write */
static inline void write_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

MFILE __infile = {
    "lamia.txt",
    __lamia_sz,
    __lamia,
    0
};
MFILE *infile = &__infile;

#define MALLOC_SIZE (256*1024)

uint32_t lz77_compress(uint8_t *uncompressed_text, uint32_t uncompressed_size,
                       uint8_t *compressed_text, uint8_t pointer_length_width)
{
    uint16_t pointer_pos, temp_pointer_pos, output_pointer, pointer_length, temp_pointer_length;
    uint32_t compressed_pointer, output_size, coding_pos, output_lookahead_ref, look_behind, look_ahead;
    uint16_t pointer_pos_max, pointer_length_max;

    pointer_pos_max = 1 << (16 - pointer_length_width);
    pointer_length_max = 1 << pointer_length_width;

    write_u32(compressed_text, uncompressed_size);
    *(compressed_text + 4) = pointer_length_width;
    compressed_pointer = output_size = 5;

    for (coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos) {
        pointer_pos = 0;
        pointer_length = 0;
        for (temp_pointer_pos = 1;
             (temp_pointer_pos < pointer_pos_max) && (temp_pointer_pos <= coding_pos);
             ++temp_pointer_pos) {
            look_behind = coding_pos - temp_pointer_pos;
            look_ahead = coding_pos;
            for (temp_pointer_length = 0;
                 uncompressed_text[look_ahead++] == uncompressed_text[look_behind++];
                 ++temp_pointer_length)
                if (temp_pointer_length == pointer_length_max)
                    break;
            if (temp_pointer_length > pointer_length) {
                pointer_pos = temp_pointer_pos;
                pointer_length = temp_pointer_length;
                if (pointer_length == pointer_length_max)
                    break;
            }
        }
        coding_pos += pointer_length;
        if ((coding_pos == uncompressed_size) && pointer_length) {
            output_pointer = (pointer_length == 1)
                ? 0
                : ((pointer_pos << pointer_length_width) | (pointer_length - 2));
            output_lookahead_ref = coding_pos - 1;
        } else {
            output_pointer = (pointer_pos << pointer_length_width) |
                             (pointer_length ? (pointer_length - 1) : 0);
            output_lookahead_ref = coding_pos;
        }
        /* Use safe write instead of *(uint16_t*) cast */
        write_u16(compressed_text + compressed_pointer, output_pointer);
        compressed_pointer += 2;
        *(compressed_text + compressed_pointer++) = *(uncompressed_text + output_lookahead_ref);
        output_size += 3;
    }
    return output_size;
}

uint32_t lz77_decompress(uint8_t *compressed_text, uint8_t *uncompressed_text)
{
    uint8_t  pointer_length_width;
    uint16_t input_pointer, pointer_length, pointer_pos, pointer_length_mask;
    uint32_t compressed_pointer, coding_pos, pointer_offset, uncompressed_size;

    uncompressed_size     = read_u32(compressed_text);
    pointer_length_width  = *(compressed_text + 4);
    compressed_pointer    = 5;
    pointer_length_mask   = (1u << pointer_length_width) - 1u;

    /* NOTA: niente ++coding_pos nel for-header */
    for (coding_pos = 0; coding_pos < uncompressed_size; ) {

        input_pointer = read_u16(compressed_text + compressed_pointer);
        compressed_pointer += 2;

        pointer_pos    = input_pointer >> pointer_length_width;
        pointer_length = pointer_pos ? ((input_pointer & pointer_length_mask) + 1u) : 0u;

        if (pointer_pos) {
            pointer_offset = coding_pos - pointer_pos;
            while (pointer_length-- > 0 && coding_pos < uncompressed_size) {
                uncompressed_text[coding_pos++] = uncompressed_text[pointer_offset++];
            }
        }

        /* literal */
        if (coding_pos < uncompressed_size) {
            uncompressed_text[coding_pos++] = compressed_text[compressed_pointer++];
        }
    }

    return coding_pos;
}


int main(int argc, char const *argv[])
{
    uint32_t compressed_size, uncompressed_size, orig_size;
    uint8_t *compressed_text, *uncompressed_text;

    libmin_mopen(infile, "r");
    uncompressed_size = libmin_msize(infile);
    libmin_printf("Original size: %ld\n", uncompressed_size);
    uncompressed_text = libmin_malloc(uncompressed_size);
    if ((uncompressed_size != libmin_mread(uncompressed_text, uncompressed_size, infile)))
        return 0;
    libmin_mclose(infile);

    compressed_text = libmin_malloc(MALLOC_SIZE);

    for (unsigned i = 1; i < 16; ++i) {
        compressed_size = lz77_compress(uncompressed_text, uncompressed_size, compressed_text, i);
        libmin_assert(compressed_size < MALLOC_SIZE);
        orig_size = lz77_decompress(compressed_text, uncompressed_text);
        libmin_assert(orig_size == uncompressed_size);
        libmin_printf("Compressed (%i): %u, decompressed: (%u)\n", i, compressed_size, uncompressed_size);
    }

    for (unsigned cnt = 0; cnt < uncompressed_size; cnt++)
        libmin_putc(uncompressed_text[cnt]);

    libmin_success();
}
