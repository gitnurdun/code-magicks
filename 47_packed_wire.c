/* MAGIC: `#pragma pack(1)` removes ABI padding so a struct matches a wire
   layout. WHY: network bytes are packed; C structs usually are not. */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#pragma pack(push, 1)
struct message {
    uint8_t  version;
    uint16_t length;
    uint32_t id;
};
#pragma pack(pop)
int main(void)
{
    struct message m = {1, 42, 0x12345678u};
    unsigned char wire[sizeof m];
    memcpy(wire, &m, sizeof m);
    printf("sizeof=%zu offsets=%zu,%zu,%zu wire=",
           sizeof m, offsetof(struct message, version),
           offsetof(struct message, length), offsetof(struct message, id));
    for (size_t i = 0; i < sizeof m; i++) printf("%02x", wire[i]);
    putchar('\n');
}

/* ================= EXPLANATION =================
   What it does
     Lays out a three-field message with no padding: one byte for the
     version, two for the length, four for the id. The struct becomes
     seven bytes, exactly the bytes that would go on the wire, and the
     program dumps them in memory order.

   Step by step
     1. Without packing, the compiler would align length to offset 2 (one
        byte of padding after version) and id to offset 4 (no padding
        after length), for sizeof == 8. The trailing byte is padding, not
        data.
     2. `#pragma pack(push, 1)` temporarily sets the maximum member
        alignment to 1, so each field follows the previous one. push/pop
        confines the setting to this declaration.
     3. The initializer sets version 1, length 42 (0x002A), and id
        0x12345678. offsets reports 0, 1, 3; sizeof reports 7.
     4. memcpy copies the object representation into a byte array. On a
        little-endian machine the bytes are 01 2a 00 78 56 34 12.
     5. The hex dump makes the endianness visible: 42 is stored low byte
        first, and so is the id.

   Output
     "sizeof=7 offsets=0,1,3 wire=012a0078563412"

   Why it matters
     Packed structs are a common way to describe on-disk records, network
     headers, and memory-mapped hardware registers. They show that C's
     object layout is not the same as a byte stream: padding, alignment,
     and endianness are all implementation details. Kernel headers and
     game asset formats often use explicit packing or serialization for
     exactly this reason.

   Caveats
     - Packed fields can be unaligned. Taking `&m.length` may produce a
       misaligned pointer; dereferencing it is undefined on strict-
       alignment CPUs and slow on x86. Use memcpy to read/write fields.
     - The bytes are host-endian. A packed struct is not a portable wire
       format across architectures; encode each field explicitly (or use
       htons/htonl) for network protocols.
     - `#pragma pack` is compiler-specific (GCC/Clang/MSVC support it,
       but syntax and interaction with ABI attributes vary).
     - Casting a `char*` buffer directly to `struct message*` can violate
       alignment and strict aliasing. memcpy into a struct is safe.
     - Packing can hurt performance by forcing unaligned loads and stores;
       pack only at the boundary, not in hot in-memory data.
   ============================================== */
