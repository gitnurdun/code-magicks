"""MAGIC: memoryview exposes bytes without copying them."""
packet = bytearray(b'HEADERpayload')
payload = memoryview(packet)[6:]
payload[0:7] = b'PAYLOAD'
print(packet)

# ================= EXPLANATION =================
# What it does
#   Mutates part of a bytearray through a memoryview slice, without
#   copying the bytes out and back.
#
# Step by step
#   1. `packet` is a 13-byte bytearray: b'HEADERpayload'. A bytearray is
#      mutable and owns a resizeable buffer.
#   2. `memoryview(packet)` creates a view object that exposes the same
#      buffer using the buffer protocol. No bytes are copied.
#   3. `[6:]` slices the view. Python slices views by byte offset, so the
#      result points at packet[6:13] == b'payload', still sharing memory.
#   4. `payload[0:7] = b'PAYLOAD'` writes into the underlying buffer. The
#      replacement must have the same length as the slice (7), because a
#      view cannot resize its source.
#   5. `print(packet)` reads the original bytearray, which now contains
#      b'HEADERPAYLOAD'.
#
# Output
#   b'HEADERPAYLOAD'
#
# Why it matters
#   Zero-copy access is what makes memoryview useful for sockets, mmap,
#   struct.unpack_from, numpy, and image buffers: parse or patch a field
#   in place instead of allocating a new bytes object for every slice.
#   It also enforces the buffer protocol's shape and type information.
#
# Caveats
#   - A view of `bytes` is read-only; only writable buffers (bytearray,
#     array, numpy) allow assignment. Attempting a write raises TypeError.
#   - A slice assignment cannot change the buffer's length; use the
#     bytearray itself for insert/delete/resize.
#   - You cannot resize the underlying bytearray while any view is alive;
#     Python raises BufferError to keep pointers valid.
#   - Multi-dimensional, non-contiguous, or strided views have their own
#     indexing rules; memoryview is not always a simple byte array.
#   - Keep a reference to the view as long as you use it; it keeps the
#     buffer exported (and thus pinned) until released.
# ==============================================
