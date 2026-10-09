"""Bounded, read-only GameCube container/texture parsing; no game content included."""
import struct
import zlib
import math

LIMIT=32*1024*1024
DISC_LIMIT=1459978240


def span(data,offset,size):
    if type(offset)!=int or type(size)!=int or offset<0 or size<0 or offset>len(data) or size>len(data)-offset:
        raise ValueError('Truncated or invalid binary range')
    return data[offset:offset+size]


def u32(data,offset):
    return struct.unpack('>I',span(data,offset,4))[0]


def name_at(table,offset):
    if offset<0 or offset>=len(table): raise ValueError('Invalid string offset')
    end=table.find(b'\0',offset,min(len(table),offset+256))
    if end<0: raise ValueError('Unterminated name')
    name=table[offset:end].decode('ascii')
    if not name or '/' in name or '\\' in name or ':' in name or any(ord(c)<32 for c in name): raise ValueError('Unsafe archive name')
    return name


def iso_files(context):
    header=context.read_game(0,0x440)
    if header[:6] not in (b'GZLE01',b'GZLP01',b'GZLJ01') or u32(header,0x1c)!=0xc2339f3d:
        raise ValueError('Select an uncompressed USA/EU/Japanese Wind Waker GameCube ISO')
    offset,size=u32(header,0x424),u32(header,0x428)
    if offset<0x440 or not 12<=size<=LIMIT or offset+size>DISC_LIMIT: raise ValueError('Invalid disc FST range')
    fst=context.read_game(offset,size)
    count=u32(fst,8)
    if fst[0]!=1 or not 1<=count<=65536 or count*12>len(fst): raise ValueError('Invalid FST root/count')
    strings=fst[count*12:]
    files={};seen=set();stack=[('',count,0)]
    for index in range(1,count):
        while stack and index>=stack[-1][1]: stack.pop()
        if not stack: raise ValueError('Invalid FST directory interval')
        word,start,length=struct.unpack('>III',span(fst,index*12,12))
        kind=word>>24;name=name_at(strings,word&0xffffff)
        if name in ('.','..'): raise ValueError('Unsafe FST path')
        path=stack[-1][0]+name
        if len(path)>512 or path in seen: raise ValueError('Duplicate/oversized FST path')
        seen.add(path)
        if kind==1:
            if start!=stack[-1][2] or not index<length<=stack[-1][1] or len(stack)>32: raise ValueError('Invalid FST directory parent/end')
            stack.append((path+'/',length,index))
        elif kind==0:
            if length>LIMIT or start<0x440 or start+length>DISC_LIMIT: raise ValueError('Invalid disc file range')
            files[path]=(start,length)
        else: raise ValueError('Unknown FST entry type')
    return files


def yaz0(data):
    if not data.startswith(b'Yaz0'): return data
    size=u32(data,4)
    if not 1<=size<=LIMIT: raise ValueError('Yaz0 output exceeds limit')
    span(data,0,16);pos=16;out=bytearray()
    while len(out)<size:
        code=span(data,pos,1)[0];pos+=1
        for bit in range(7,-1,-1):
            if len(out)==size: break
            if code&(1<<bit): out.extend(span(data,pos,1));pos+=1
            else:
                a,b=span(data,pos,2);pos+=2
                distance=((a&15)<<8|b)+1
                length=a>>4
                if length: length+=2
                else: length=span(data,pos,1)[0]+18;pos+=1
                if distance>len(out) or length>size-len(out): raise ValueError('Invalid Yaz0 back-reference')
                for _ in range(length): out.append(out[-distance])
    return bytes(out)


def rarc_files(raw):
    data=yaz0(raw)
    if span(data,0,4)!=b'RARC' or u32(data,4)!=len(data) or len(data)>LIMIT: raise ValueError('Invalid RARC header/size')
    nodes=u32(data,0x20);node_offset=0x20+u32(data,0x24)
    entries=u32(data,0x28);entry_offset=0x20+u32(data,0x2c)
    string_size=u32(data,0x30);string_offset=0x20+u32(data,0x34)
    payload_offset=0x20+u32(data,0xc)
    if not 1<=nodes<=4096 or entries>65536 or payload_offset>len(data): raise ValueError('Invalid RARC inventory')
    span(data,node_offset,nodes*16);span(data,entry_offset,entries*20)
    strings=span(data,string_offset,string_size);result={};visited=set();paths=set()
    pending=[(0,'')]
    while pending:
        index,prefix=pending.pop()
        if index in visited or index>=nodes or len(prefix)>512: raise ValueError('RARC directory cycle/invalid node')
        visited.add(index)
        _,node_name,_,count,first=struct.unpack('>4sIHHI',span(data,node_offset+index*16,16))
        name_at(strings,node_name)
        if first+count>entries: raise ValueError('Invalid RARC node entries')
        for entry in range(first,first+count):
            _,_,flags,name_offset,offset,size,_=struct.unpack('>HHHHIII',span(data,entry_offset+entry*20,20))
            name=name_at(strings,name_offset)
            if name in ('.','..'): continue
            path=prefix+name
            if path in paths: raise ValueError('Duplicate RARC path')
            paths.add(path)
            if flags&0x0200:
                if flags&0x0100: raise ValueError('Ambiguous RARC entry type')
                pending.append((offset,path+'/'))
            elif flags&0x0100:
                if path in result or size>LIMIT: raise ValueError('Duplicate/oversized RARC file')
                result[path]=span(data,payload_offset+offset,size)
            else: raise ValueError('Unknown RARC entry type')
    return result


def bounds_from_dzr(data,floor=128):
    # Public GC dStage_roomDt_c::getMapInfo2 selects byte +0x35; d_map.cpp
    # pairs s<floor>.bti with map0 X0/Z0/X1/Z1 at +0/+4/+8/+C.
    count=u32(data,0)
    if count>1024: raise ValueError('Invalid DZR chunk count')
    span(data,4,count*12);found=None
    for i in range(count):
        tag,number,offset=struct.unpack('>4sII',span(data,4+i*12,12))
        if tag!=b'2DMA': continue
        if not 1<=number<=256 or offset<4+count*12: raise ValueError('Invalid map bounds chunk')
        span(data,offset,number*0x38)
        for index in range(number):
            record=span(data,offset+index*0x38,0x38)
            if record[0x35]!=floor: continue
            if found is not None: raise ValueError('Ambiguous floor map bounds')
            values=struct.unpack('>4f',record[:16])
            if not all(math.isfinite(v) and abs(v)<=10000000 for v in values) or values[0]>=values[2] or values[1]>=values[3]: raise ValueError('Unverified/degenerate map bounds')
            found=values
    if found is None: raise ValueError('No verified floor 2DMA bounds; sector hidden')
    return found


def decode_bti(data):
    span(data,0,32)
    kind=data[0];width,height=struct.unpack('>HH',data[2:6])
    palette_kind=data[9];count=struct.unpack('>H',data[10:12])[0]
    palette=u32(data,12);image=u32(data,28)
    if kind not in (8,9) or palette_kind!=2: raise ValueError('Expected CI4/CI8 RGB5A3 map')
    if not width or not height or width>2048 or height>2048: raise ValueError('Invalid BTI dimensions')
    if not 1<=count<=(16 if kind==8 else 256) or palette<32: raise ValueError('Invalid BTI palette')
    palette_data=span(data,palette,count*2);colors=[]
    for i in range(count):
        c=struct.unpack('>H',palette_data[i*2:i*2+2])[0]
        if c&0x8000: colors.append((((c>>10)&31)*255//31,((c>>5)&31)*255//31,(c&31)*255//31,255))
        else: colors.append((((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17,((c>>12)&7)*255//7))
    block_height=8 if kind==8 else 4;blocks_x=(width+7)//8;blocks_y=(height+block_height-1)//block_height
    if image<32: raise ValueError('Invalid BTI image offset')
    pixels=span(data,image,blocks_x*blocks_y*32);out=bytearray(width*height*4)
    for y in range(height):
        for x in range(width):
            block=(y//block_height*blocks_x+x//8)*32;pixel=(y%block_height)*8+x%8
            value=pixels[block+(pixel//2 if kind==8 else pixel)]
            index=(value>>4 if pixel%2==0 else value&15) if kind==8 else value
            if index>=len(colors): raise ValueError('BTI palette index out of range')
            out[(y*width+x)*4:(y*width+x+1)*4]=bytes(colors[index])
    return width,height,bytes(out)


def png_rgba(width,height,rgba):
    if not 1<=width<=2048 or not 1<=height<=2048 or len(rgba)!=width*height*4: raise ValueError('Invalid PNG dimensions/pixels')
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    scan=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    result=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b'')
    if len(result)>1024*1024: raise ValueError('PNG exceeds per-mod 1 MiB limit')
    return result
