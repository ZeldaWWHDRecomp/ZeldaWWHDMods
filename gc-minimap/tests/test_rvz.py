"""Synthetic RVZ containers made from authored bytes; never game fixtures."""
from pathlib import Path
import sys
import struct
import hashlib
import unittest
import bz2
import lzma

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import formats
import rvz
from test_formats import iso,MemoryDisc


def synthetic(codec=0,packed=False,raw_group=False):
    virtual=bytes(iso()).ljust(65536,b'\0');chunk=32768
    if codec==2:compress=bz2.compress;properties=b''
    elif codec==3:
        properties=bytes([93])+struct.pack('<I',1024*1024)
        compress=lambda data:lzma.compress(data,format=lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_LZMA1,'dict_size':1024*1024,'lc':3,'lp':0,'pb':2}])
    elif codec==4:
        properties=bytes([16]);compress=lambda data:lzma.compress(data,format=lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_LZMA2,'dict_size':1024*1024}])
    elif codec==5:
        from compression import zstd
        properties=b'';compress=zstd.compress
    else:properties=b'';compress=lambda data:data
    file=bytearray(0x124);groups=[]
    for index in range(2):
        payload=virtual[index*chunk:(index+1)*chunk]
        packing=struct.pack('>I',len(payload))+payload if packed else payload
        compressed=packing if raw_group and index==0 else compress(packing)
        while len(file)%4:file.append(0)
        groups.append((len(file)//4,len(compressed)|(0x80000000 if codec and not (raw_group and index==0) else 0),len(packing) if packed else 0))
        file.extend(compressed)
    raw=compress(struct.pack('>QQII',0x80,len(virtual)-0x80,0,2));raw_offset=len(file);file.extend(raw)
    group_data=compress(b''.join(struct.pack('>III',*entry) for entry in groups));group_offset=len(file);file.extend(group_data)
    disc=bytearray(0xdc);struct.pack_into('>IIII',disc,0,1,codec,0,chunk);disc[16:16+128]=virtual[:128]
    struct.pack_into('>IQIIQI',disc,0xb4,1,raw_offset,len(raw),2,group_offset,len(group_data))
    disc[0xd4]=len(properties);disc[0xd5:0xd5+len(properties)]=properties
    head=bytearray(0x48);head[:4]=b'RVZ\x01';struct.pack_into('>III',head,4,0x01000000,0x00030000,len(disc))
    head[0x10:0x24]=hashlib.sha1(disc).digest();struct.pack_into('>QQ',head,0x24,len(virtual),len(file));head[0x34:]=hashlib.sha1(head[:0x34]).digest()
    file[:0x48]=head;file[0x48:0x124]=disc
    return bytes(file),virtual


def rehash(data):
    data=bytearray(data);size=struct.unpack_from('>I',data,12)[0]
    data[0x10:0x24]=hashlib.sha1(data[0x48:0x48+size]).digest();data[0x34:0x48]=hashlib.sha1(data[:0x34]).digest()
    return bytes(data)


class RVZTests(unittest.TestCase):
    def test_codecs_and_cross_chunk(self):
        for codec in (0,2,3,4,5):
            data,virtual=synthetic(codec,packed=True);reader=rvz.Reader(MemoryDisc(data))
            self.assertEqual(reader.read_game(0,128),virtual[:128])
            self.assertEqual(reader.read_game(32760,64),virtual[32760:32824])
            self.assertEqual(reader.read_game(0,len(virtual)),virtual)
            self.assertEqual(formats.iso_files(reader),{'res/map.arc':(0x1000,4)})
            self.assertLessEqual(len(reader.cache),4)
    def test_uncompressed_and_zero_groups(self):
        data,virtual=synthetic(5,raw_group=True);reader=rvz.Reader(MemoryDisc(data))
        self.assertEqual(reader.read_game(128,1024),virtual[128:1152])
        reader.groups[1]=(0,0,0)
        self.assertEqual(reader.read_game(32768,64),bytes(64))
    def test_older_python_dependency(self):
        from unittest import mock
        import builtins
        real_import=builtins.__import__
        def without_zstd(name,*args,**kwargs):
            if name=='compression':raise ImportError('authored missing-codec fixture')
            return real_import(name,*args,**kwargs)
        data,_=synthetic(5)
        with mock.patch('builtins.__import__',side_effect=without_zstd):
            with self.assertRaisesRegex(ValueError,'Python 3.14'):
                rvz.Reader(MemoryDisc(data))
    def test_hashes(self):
        original,_=synthetic()
        for offset in (0x10,0x34,0x48+16):
            data=bytearray(original);data[offset]^=1
            with self.assertRaises(ValueError):rvz.Reader(MemoryDisc(data))
    def test_table_limits(self):
        original,_=synthetic()
        for relative,value in ((0,2),(0x90,1),(0xb4,65537),(0xc4,65537),(12,1),(0xc0,rvz.LIMIT+1)):
            data=bytearray(original);struct.pack_into('>I',data,0x48+relative,value)
            with self.assertRaises(ValueError):rvz.Reader(MemoryDisc(rehash(data)))
        data=bytearray(original);struct.pack_into('>Q',data,0x48+0xb8,len(data)+1)
        with self.assertRaises(ValueError):rvz.Reader(MemoryDisc(rehash(data)))
    def test_bounds_and_truncation(self):
        data,_=synthetic();reader=rvz.Reader(MemoryDisc(data))
        for offset,size in ((-1,1),(65536,1),(0,rvz.LIMIT+1),(65530,10)):
            with self.assertRaises(ValueError):reader.read_game(offset,size)
        with self.assertRaises(ValueError):rvz.Reader(MemoryDisc(data[:-1]))
        entry=reader.groups[0];reader.groups[0]=(len(data)//4+1,entry[1],entry[2])
        with self.assertRaises(ValueError):reader.read_game(128,4)
    def test_decompression_cap(self):
        data,_=synthetic(2);reader=rvz.Reader(MemoryDisc(data))
        with self.assertRaises(ValueError):reader.decode(bz2.compress(b'x'*10000),4,2)
        with self.assertRaises(ValueError):reader.decode(bz2.compress(b'x')+b'trailing',1,2)
        with self.assertRaises(ValueError):reader.decode(b'',rvz.LIMIT+1,2)
        with self.assertRaises(ValueError):reader.decode(bz2.compress(b'x')[:-2],1,2)
    def test_packing(self):
        self.assertEqual(rvz.unpack(struct.pack('>I',3)+b'abc',3,0),b'abc')
        self.assertEqual(rvz.unpack(struct.pack('>I',0x80000005)+bytes(68),5,0),bytes(5))
        for data,size in ((struct.pack('>I',4)+b'abcd',3),(struct.pack('>I',3)+b'ab',3),(struct.pack('>I',0x80000005)+bytes(67),5),(b'',1)):
            with self.assertRaises(ValueError):rvz.unpack(data,size,0)
        seed=struct.pack('>17I',*range(17))
        self.assertEqual(rvz.random_padding(seed,16,3),rvz.random_padding(seed,19,0)[3:])
        self.assertEqual(rvz.random_padding(seed,16,32771),rvz.random_padding(seed,16,3))


if __name__=='__main__':unittest.main()
