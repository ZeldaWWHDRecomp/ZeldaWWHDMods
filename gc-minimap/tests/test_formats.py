"""Entirely authored in-memory containers; no game files or extracted fixtures."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import formats
import vector


def dzr(bounds=(-10.,-20.,30.,40.),floor=128):
    record=bytearray(0x38);struct.pack_into('>4f',record,0,*bounds);record[0x35]=floor
    return struct.pack('>I4sII',1,b'2DMA',1,16)+record


def bti(kind=8):
    header=bytearray(32);header[0]=kind;struct.pack_into('>HH',header,2,8,8 if kind==8 else 4)
    header[9]=2;struct.pack_into('>HII',header,10,2,32,0);struct.pack_into('>I',header,28,36)
    pixels=bytes([0x11 if kind==8 else 1])*32
    return bytes(header)+struct.pack('>HH',0,0xffff)+pixels


def archive(files):
    # Root with flat authored names is sufficient to exercise real RARC offsets.
    strings=bytearray(b'root\0');names=[]
    for name in files:
        names.append(len(strings));strings.extend(name.encode()+b'\0')
    nodes=0x40;entries=nodes+16;string_offset=entries+20*len(files);payload_offset=(string_offset+len(strings)+31)&~31
    data=bytearray(payload_offset);data[:4]=b'RARC'
    struct.pack_into('>I',data,0xc,payload_offset-0x20)
    struct.pack_into('>IIIIII',data,0x20,1,nodes-0x20,len(files),entries-0x20,len(strings),string_offset-0x20)
    struct.pack_into('>4sIHHI',data,nodes,b'ROOT',0,0,len(files),0)
    data[string_offset:string_offset+len(strings)]=strings
    for index,((name,payload),name_offset) in enumerate(zip(files.items(),names)):
        struct.pack_into('>HHHHIII',data,entries+index*20,index,0,0x1100,name_offset,len(data)-payload_offset,len(payload),0)
        data.extend(payload)
    struct.pack_into('>I',data,4,len(data))
    return bytes(data)


def iso():
    names=b'\0res\0map.arc\0'
    fst=struct.pack('>III',0x01000000,0,3)+struct.pack('>III',0x01000001,0,3)+struct.pack('>III',5,0x1000,4)+names
    data=bytearray(0x1004);data[:6]=b'GZLE01';struct.pack_into('>I',data,0x1c,0xc2339f3d)
    struct.pack_into('>II',data,0x424,0x500,len(fst));data[0x500:0x500+len(fst)]=fst;data[0x1000:]=b'test'
    return data


class MemoryDisc:
    def __init__(self,data): self.data=data
    def read_game(self,offset,size): return formats.span(self.data,offset,size)


class FormatsTests(unittest.TestCase):
    def test_iso(self):
        data=iso();self.assertEqual(formats.iso_files(MemoryDisc(data)),{'res/map.arc':(0x1000,4)})
        for ident in (b'GZLP01',b'GZLJ01'):
            data[:6]=ident;self.assertEqual(len(formats.iso_files(MemoryDisc(data))),1)
    def test_large_unselected_disc_file_has_no_read_allocation(self):
        data=iso();struct.pack_into('>I',data,0x500+24+8,formats.LIMIT+1)
        self.assertEqual(formats.iso_files(MemoryDisc(data))['res/map.arc'][1],formats.LIMIT+1)
    def test_iso_invalid(self):
        for field,value in ((0x1c,0),(0x424,0),(0x428,0xffffffff)):
            data=iso();struct.pack_into('>I',data,field,value)
            with self.assertRaises(ValueError):formats.iso_files(MemoryDisc(data))
        data=iso();data[0x500+36+5:0x500+36+12]=b'../bad\0'
        with self.assertRaises(ValueError):formats.iso_files(MemoryDisc(data))
    def test_yaz(self):
        raw=b'Yaz0'+struct.pack('>I',6)+bytes(8)+bytes([0xe0])+b'abc'+bytes([0x10,2])
        self.assertEqual(formats.yaz0(raw),b'abcabc')
        with self.assertRaises(ValueError):formats.yaz0(raw[:-1])
        with self.assertRaises(ValueError):formats.yaz0(b'Yaz0'+struct.pack('>I',formats.LIMIT+1)+bytes(8))
        with self.assertRaises(ValueError):formats.yaz0(b'Yaz0'+struct.pack('>I',3)+bytes(8)+bytes([0,0x10,1]))
    def test_rarc(self):
        data=archive({'map.bti':b'authored','room.dzr':dzr()})
        self.assertEqual(formats.rarc_files(data)['map.bti'],b'authored')
        with self.assertRaises(ValueError):formats.rarc_files(data[:-1])
        bad=bytearray(data);struct.pack_into('>I',bad,0x70,formats.LIMIT)
        with self.assertRaises(ValueError):formats.rarc_files(bad)
        with self.assertRaises(ValueError):formats.rarc_files(archive({'../bad':b'x'}))
    def test_bounds(self):
        self.assertEqual(formats.bounds_from_dzr(dzr()),(-10.,-20.,30.,40.))
        for data in (dzr(floor=127),dzr((0.,0.,0.,0.)),dzr((float('nan'),0.,1.,1.)),dzr()[:-1],struct.pack('>I',0)):
            with self.assertRaises(ValueError):formats.bounds_from_dzr(data)
    def test_bti(self):
        for kind in (8,9):
            width,height,rgba=formats.decode_bti(bti(kind));self.assertEqual(width,8);self.assertEqual(rgba,bytes([255])*width*height*4)
        bad=bytearray(bti());bad[-1]=0xff
        with self.assertRaises(ValueError):formats.decode_bti(bad)
        with self.assertRaises(ValueError):formats.decode_bti(bti()[:-1])
    def test_png_deterministic_and_crc(self):
        rgba=bytes([255,0,0,255])*16;png=formats.png_rgba(4,4,rgba)
        self.assertEqual(png,formats.png_rgba(4,4,rgba));offset=8;compressed=b''
        while offset<len(png):
            size=struct.unpack_from('>I',png,offset)[0];tag=png[offset+4:offset+8];data=png[offset+8:offset+8+size]
            self.assertEqual(zlib.crc32(tag+data)&0xffffffff,struct.unpack_from('>I',png,offset+8+size)[0])
            if tag==b'IDAT':compressed+=data
            offset+=12+size
        self.assertEqual(zlib.decompress(compressed),b''.join(b'\0'+rgba[y*16:(y+1)*16] for y in range(4)))
    def test_trace_geography(self):
        rgba=bytearray(16*16*4)
        for y in range(3,13):
            for x in range(3,13):rgba[(y*16+x)*4:(y*16+x+1)*4]=bytes((200,180,140,255))
        raster,svg,receipt=vector.trace(16,16,bytes(rgba),120)
        self.assertEqual(len(raster),120*120*4);self.assertIn(b'<path',svg);self.assertIn(b'"source_pixel_center_mismatches": 0',receipt)
        self.assertEqual(vector.trace(16,16,bytes(rgba),120),(raster,svg,receipt))
        with self.assertRaises(ValueError):vector.trace(8,16,bytes(rgba),120)
        with self.assertRaises(ValueError):vector.trace(16,16,bytes(16*16*4),120)


if __name__=='__main__':unittest.main()
