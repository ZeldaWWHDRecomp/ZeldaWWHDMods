"""Bounded GameCube RVZ random reader, implemented from public format facts.

Format reference: https://github.com/dolphin-emu/dolphin/blob/master/docs/WiaAndRvz.md
No Dolphin implementation source is copied. Wii partitions/keys are never read.
Zstandard RVZ requires Python 3.14's standard-library compression.zstd module.
"""
import struct
import hashlib
import bz2
import lzma
import formats

LIMIT=32*1024*1024


def number(data,offset,size):
    return int.from_bytes(formats.span(data,offset,size),'big')


def random_padding(seed,length,offset):
    words=list(struct.unpack('>17I',formats.span(seed,0,68)))
    for i in range(17,521):
        words.append(((words[i-17]<<23)^(words[i-16]>>9)^words[i-1])&0xffffffff)
    def advance():
        for i in range(521): words[i]^=words[i+489] if i<32 else words[i-32]
    for _ in range(4): advance()
    skip=offset%32768;needed=skip+length;out=bytearray()
    while len(out)<needed:
        for word in words:
            out.extend((word>>24,(word>>18)&255,(word>>8)&255,word&255))
        if len(out)<needed: advance()
    return bytes(out[skip:needed])


def unpack(data,expected,offset):
    result=bytearray();pos=0
    while pos<len(data):
        size=number(data,pos,4);pos+=4;random=bool(size&0x80000000);size&=0x7fffffff
        if size>expected-len(result): raise ValueError('RVZ packed run exceeds chunk')
        if random:
            seed=formats.span(data,pos,68);pos+=68
            result.extend(random_padding(seed,size,offset+len(result)))
        else:
            result.extend(formats.span(data,pos,size));pos+=size
    if len(result)!=expected: raise ValueError('RVZ unpacked chunk size mismatch')
    return bytes(result)


class Reader:
    def __init__(self,context):
        self.context=context;self.cache={};self.cache_order=[]
        head=context.read_game(0,0x48)
        if head[:4]!=b'RVZ\x01' or number(head,8,4)>0x01000000: raise ValueError('Unsupported RVZ version')
        if hashlib.sha1(head[:0x34]).digest()!=head[0x34:0x48]: raise ValueError('RVZ header checksum mismatch')
        size=number(head,12,4)
        self.virtual_size=number(head,0x24,8);self.physical_size=number(head,0x2c,8)
        if not 0xd5<=size<=4096 or not 0x440<=self.virtual_size<=formats.DISC_LIMIT or not 0x48+size<=self.physical_size<=formats.DISC_LIMIT+LIMIT: raise ValueError('Invalid RVZ sizes')
        disc=self.physical(0x48,size)
        if hashlib.sha1(disc).digest()!=head[0x10:0x24]: raise ValueError('RVZ disc header checksum mismatch')
        if number(disc,0,4)!=1 or number(disc,0x90,4)!=0: raise ValueError('Only GameCube RVZ is supported; no Wii partitions are read')
        self.codec=number(disc,4,4);self.chunk_size=number(disc,12,4)
        if self.codec not in (0,2,3,4,5): raise ValueError('Unsupported RVZ compression method')
        if not 32768<=self.chunk_size<=LIMIT or (self.chunk_size<2*1024*1024 and self.chunk_size&(self.chunk_size-1)) or (self.chunk_size>=2*1024*1024 and self.chunk_size%(2*1024*1024)): raise ValueError('Invalid RVZ chunk size')
        self.disc_header=formats.span(disc,16,0x80)
        properties=disc[0xd5:0xd5+disc[0xd4]]
        if disc[0xd4]>7 or len(properties)!=disc[0xd4]: raise ValueError('Invalid RVZ codec properties')
        self.filters=None;self.zstd=None
        if self.codec==5:
            try:
                from compression import zstd
            except ImportError as error:
                raise ValueError('This Zstandard RVZ needs Python 3.14 or newer (standard-library compression.zstd). Select a compatible setup interpreter; no external programs are run.') from error
            self.zstd=zstd
        elif self.codec==3:
            if len(properties)!=5 or properties[0]>=225: raise ValueError('Invalid LZMA properties')
            prop=properties[0];dictionary=int.from_bytes(properties[1:],'little')
            if not 4096<=dictionary<=LIMIT: raise ValueError('RVZ LZMA dictionary exceeds limit')
            self.filters=[{'id':lzma.FILTER_LZMA1,'dict_size':dictionary,'lc':prop%9,'lp':(prop//9)%5,'pb':prop//45}]
        elif self.codec==4:
            if len(properties)!=1 or properties[0]>40: raise ValueError('Invalid LZMA2 properties')
            prop=properties[0];dictionary=(2|(prop&1))<<(prop//2+11) if prop<40 else 0xffffffff
            if dictionary>LIMIT: raise ValueError('RVZ LZMA2 dictionary exceeds limit')
            self.filters=[{'id':lzma.FILTER_LZMA2,'dict_size':dictionary}]
        raw_count=number(disc,0xb4,4);group_count=number(disc,0xc4,4)
        if not 1<=raw_count<=65536 or not 1<=group_count<=65536: raise ValueError('RVZ table inventory exceeds limit')
        raw=self.decode(self.physical(number(disc,0xb8,8),number(disc,0xc0,4)),raw_count*24,self.codec)
        groups=self.decode(self.physical(number(disc,0xc8,8),number(disc,0xd0,4)),group_count*12,self.codec)
        self.groups=[struct.unpack('>III',groups[i*12:i*12+12]) for i in range(group_count)]
        self.regions=[]
        for i in range(raw_count):
            start,length,first,count=struct.unpack('>QQII',raw[i*24:i*24+24])
            aligned=start&~32767;length+=start-aligned
            if not length or aligned+length>self.virtual_size or count!=(length+self.chunk_size-1)//self.chunk_size or first+count>group_count: raise ValueError('Invalid RVZ raw segment mapping')
            self.regions.append((aligned,aligned+length,first,count))
        self.regions.sort()
        if any(a[1]>b[0] for a,b in zip(self.regions,self.regions[1:])): raise ValueError('Overlapping RVZ raw segments')

    def physical(self,offset,size):
        if not 0<=size<=LIMIT or offset<0 or offset>self.physical_size or size>self.physical_size-offset: raise ValueError('Invalid RVZ physical range')
        return self.context.read_game(offset,size)

    def decode(self,data,expected,codec):
        if not 0<=expected<=LIMIT: raise ValueError('RVZ decompression exceeds limit')
        if codec==0: result=data
        else:
            if codec==2: decoder=bz2.BZ2Decompressor()
            elif codec in (3,4): decoder=lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=self.filters)
            elif codec==5: decoder=self.zstd.ZstdDecompressor(options={self.zstd.DecompressionParameter.window_log_max:25})
            else: raise ValueError('Unsupported RVZ codec')
            try: result=decoder.decompress(data,max_length=expected+1)
            except Exception as error: raise ValueError('Invalid compressed RVZ data') from error
            if not decoder.eof or decoder.unused_data: raise ValueError('Truncated/trailing compressed RVZ data')
        if len(result)!=expected: raise ValueError('RVZ decompressed size mismatch')
        return result

    def group(self,index,expected,offset):
        key=(index,expected,offset)
        if key in self.cache:
            del self.cache_order[self.cache_order.index(key)];self.cache_order.append(key);return self.cache[key]
        position,size,packed=self.groups[index];compressed=bool(size&0x80000000);size&=0x7fffffff
        if size==0:
            if packed: raise ValueError('Zero RVZ group cannot contain packing')
            result=bytes(expected)
        else:
            if packed>LIMIT: raise ValueError('RVZ packed data exceeds limit')
            raw=self.decode(self.physical(position*4,size),packed or expected,self.codec if compressed else 0)
            result=unpack(raw,expected,offset) if packed else raw
        self.cache[key]=result;self.cache_order.append(key)
        while len(self.cache_order)>4:
            del self.cache[self.cache_order.pop(0)]
        return result

    def read_game(self,offset,size):
        if type(offset)!=int or type(size)!=int or offset<0 or not 0<=size<=LIMIT or offset>self.virtual_size or size>self.virtual_size-offset: raise ValueError('Invalid RVZ virtual read')
        out=bytearray();end=offset+size
        while offset<end:
            if offset<0x80:
                length=min(end,0x80)-offset;out.extend(self.disc_header[offset:offset+length]);offset+=length;continue
            region=next((r for r in self.regions if r[0]<=offset<r[1]),None)
            if region is None: raise ValueError('RVZ virtual range is not mapped')
            start,stop,first,_=region;relative=offset-start;index=relative//self.chunk_size;within=relative%self.chunk_size
            expected=min(self.chunk_size,stop-start-index*self.chunk_size);length=min(end-offset,expected-within)
            data=self.group(first+index,expected,start+index*self.chunk_size)
            out.extend(data[within:within+length]);offset+=length
        return bytes(out)


def disc_reader(context):
    return Reader(context) if context.read_game(0,4)==b'RVZ\x01' else context
