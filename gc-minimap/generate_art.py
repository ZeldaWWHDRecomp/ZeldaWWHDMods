"""Original panel artwork. Pure stdlib; no game data or filesystem access."""
import struct
import zlib


def png(size, pixels):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    rows = b''.join(b'\0' + bytes(pixels[y * size * 4:(y + 1) * size * 4]) for y in range(size))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


def inside(x, y, vertices):
    sign = 0
    for index, (ax, ay) in enumerate(vertices):
        bx, by = vertices[(index + 1) % len(vertices)]
        cross = (bx - ax) * (y - ay) - (by - ay) * (x - ax)
        if cross:
            direction = 1 if cross > 0 else -1
            if sign and sign != direction:
                return False
            sign = direction
    return True


def raster(size, shade):
    pixels = bytearray()
    # Four authored subpixel samples give deterministic straight-alpha edges.
    for y in range(size):
        for x in range(size):
            samples = [shade((x + dx) / size, (y + dy) / size) for dx, dy in ((.25, .25), (.75, .25), (.25, .75), (.75, .75))]
            alpha = sum(sample[3] for sample in samples)
            rgb = [round(sum(sample[channel] * sample[3] for sample in samples) / alpha) if alpha else 0 for channel in range(3)]
            pixels.extend(rgb + [round(alpha / 4)])
    return png(size, pixels)


def frame(x, y):
    edge = min(x, y, 1 - x, 1 - y)
    if edge < .014:
        return (163, 184, 212, 235)
    if edge < .027:
        return (36, 67, 97, 220)
    return (0, 0, 0, 0)


def marker(x, y):
    if inside(x, y, ((.5, .04), (.94, .94), (.5, .72), (.06, .94))):
        return (255, 218, 61, 255) if y < .64 else (212, 56, 44, 255)
    return (0, 0, 0, 0)


def arrow(x, y):
    if inside(x, y, ((.07, .15), (.93, .15), (.5, .91))):
        if inside(x, y, ((.19, .22), (.81, .22), (.5, .78))):
            return (84, 130, 160, 238)
        return (163, 184, 212, 235)
    return (0, 0, 0, 0)


def generate():
    return {'frame.png': raster(256, frame), 'marker.png': raster(64, marker), 'arrow.png': raster(64, arrow)}
