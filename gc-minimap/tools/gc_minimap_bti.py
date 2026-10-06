"""Decode GC CI4/CI8 minimap BTIs to RGBA for a local-only cache."""
import struct

def decode_bti(data):
    if len(data)<32:raise ValueError('Truncated BTI header')
    kind=data[0];width,height=struct.unpack_from('>HH',data,2)
    palette_kind=data[9];count=struct.unpack_from('>H',data,10)[0]
    palette=struct.unpack_from('>I',data,12)[0];image=struct.unpack_from('>I',data,28)[0]
    if kind not in (8,9) or palette_kind!=2:raise ValueError('Expected CI4/CI8 with RGB5A3 palette')
    if not width or not height or width>2048 or height>2048:raise ValueError('Invalid dimensions')
    if not count or count>(16 if kind==8 else 256) or palette<32 or palette+count*2>len(data):raise ValueError('Invalid palette')
    colors=[]
    for i in range(count):
        c=struct.unpack_from('>H',data,palette+i*2)[0]
        if c&0x8000:colors.append((((c>>10)&31)*255//31,((c>>5)&31)*255//31,(c&31)*255//31,255))
        else:colors.append((((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17,((c>>12)&7)*255//7))
    block_height=8 if kind==8 else 4;blocks_x=(width+7)//8;blocks_y=(height+block_height-1)//block_height
    if image<32 or image+blocks_x*blocks_y*32>len(data):raise ValueError('Truncated image')
    out=bytearray(width*height*4)
    for y in range(height):
        for x in range(width):
            block=(y//block_height*blocks_x+x//8)*32
            pixel=(y%block_height)*8+x%8
            value=data[image+block+(pixel//2 if kind==8 else pixel)]
            index=(value>>4 if pixel%2==0 else value&15) if kind==8 else value
            if index>=len(colors):raise ValueError('Palette index out of range')
            out[(y*width+x)*4:(y*width+x+1)*4]=bytes(colors[index])
    return width,height,bytes(out)
