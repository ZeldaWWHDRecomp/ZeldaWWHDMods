"""Original HD minimap artwork. Pure stdlib; no game data or filesystem access."""
import struct
import zlib

ARROW_WIDTH, ARROW_HEIGHT = 128, 112
# The floor-change arrow: a white triangle pointing up, its tip at the top centre and its base near
# the bottom (the HUD turns it 180 degrees for the down markers). It stands in for the game's
# half-arrow art, which needs mirrored UVs.
ARROW_TIP_Y, ARROW_BASE_Y, ARROW_HALF_BASE = 4.0, 107.5, 58.5


def png(width, height, pixels):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    stride = width * 4
    rows = b''.join(b'\0' + bytes(pixels[y * stride:(y + 1) * stride]) for y in range(height))
    header = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


def inside_arrow(x, y):
    if y < ARROW_TIP_Y or y > ARROW_BASE_Y:
        return False
    half = (y - ARROW_TIP_Y) * ARROW_HALF_BASE / (ARROW_BASE_Y - ARROW_TIP_Y)
    return abs(x - ARROW_WIDTH / 2) <= half


def arrow():
    # 4 x 4 samples a pixel give smooth, deterministic edges; white with straight alpha.
    samples = [(i + .5) / 4 for i in range(4)]
    pixels = bytearray()
    for y in range(ARROW_HEIGHT):
        for x in range(ARROW_WIDTH):
            covered = sum(inside_arrow(x + dx, y + dy) for dy in samples for dx in samples)
            alpha = round(covered * 255 / 16)
            pixels.extend((255, 255, 255, alpha) if alpha else (0, 0, 0, 0))
    return png(ARROW_WIDTH, ARROW_HEIGHT, pixels)


def generate():
    return {'floor-arrow.png': arrow()}
