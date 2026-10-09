"""Original panel artwork. Pure stdlib; no game data or filesystem access."""
import struct
import zlib


def png(size, pixels):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    rows = b''.join(b'\0' + bytes(pixels[y * size * 4:(y + 1) * size * 4]) for y in range(size))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


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


def display_colour(rgb, alpha=1):
    # The authored prototype shader emits premultiplied linear RGB. Convert to
    # straight display-encoded PNG samples for the SDK's source-alpha renderer.
    channels = []
    for value in rgb:
        value = min(1, max(0, value / alpha))
        encoded = 12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055
        channels.append(round(encoded * 255))
    return tuple(channels) + (round(alpha * 255),)


def frame(x, y):
    return display_colour((.64, .72, .83), .92) if min(x, y, 1 - x, 1 - y) < .015 else (0, 0, 0, 0)


def marker(x, y):
    # Square canvas keeps the rotation pivot at Link, including the asymmetric
    # .039-tip/.023-base triangle used by the original authored panel.
    px, py = (x - .5) * .078, (.5 - y) * .078
    if -.023 < py < .039 and abs(px) < (.039 - py) * .47:
        return display_colour((1, .88, .05) if py > 0 else (.95, .18, .05))
    return (0, 0, 0, 0)


def arrow(x, y):
    # Downward canonical art; the HUD rotates it for the three panel edges.
    px, py = (x - .5) * .1, (.5 - y) * .05
    if -.025 < py < .025 and abs(px) < .025 + py:
        edge = min(.025 - py, (.025 + py - abs(px)) * .7071)
        return display_colour((.78, .84, .91), .9) if edge < .005 else display_colour((.03, .06, .20), .6)
    return (0, 0, 0, 0)


def generate():
    return {'frame.png': raster(256, frame), 'marker.png': raster(64, marker), 'arrow.png': raster(64, arrow)}
