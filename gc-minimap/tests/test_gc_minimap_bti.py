import unittest,struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gc_minimap_bti import decode_bti

def fixture(kind,width,height,image,palette):
    b=bytearray(32+len(image)+2*len(palette));b[0]=kind;b[9]=2
    struct.pack_into('>HH',b,2,width,height);struct.pack_into('>H',b,10,len(palette))
    struct.pack_into('>I',b,12,32+len(image));struct.pack_into('>I',b,28,32)
    b[32:32+len(image)]=image
    for i,color in enumerate(palette):struct.pack_into('>H',b,32+len(image)+i*2,color)
    return bytes(b)

class DecoderTests(unittest.TestCase):
    def test_ci4_nibbles_and_color(self):
        w,h,p=decode_bti(fixture(8,4,4,bytes([0x01])*32,[0x8000,0xffff]))
        self.assertEqual((w,h),(4,4));self.assertEqual(p[:8],bytes([0,0,0,255,255,255,255,255]))
    def test_tile_seams_and_padding(self):
        _,_,p=decode_bti(fixture(8,9,9,bytes(32)+bytes([0x11])*64+bytes(32),[0x8000,0xffff]))
        pixel=lambda x,y:p[(y*9+x)*4:(y*9+x+1)*4]
        self.assertEqual(pixel(8,0),bytes([255]*4));self.assertEqual(pixel(0,8),bytes([255]*4));self.assertEqual(pixel(8,8),bytes([0,0,0,255]))
    def test_ci8_and_transparency(self):
        _,_,p=decode_bti(fixture(9,8,4,bytes([0,1])*16,[0x0fff,0x7f00]))
        self.assertEqual(p[:8],bytes([255,255,255,0,255,0,0,255]))
    def test_truncated_and_invalid_index(self):
        good=fixture(8,4,4,bytes([0x01])*32,[0x8000,0xffff])
        with self.assertRaises(ValueError):decode_bti(good[:40])
        bad=bytearray(good);bad[32]=0x0f
        with self.assertRaises(ValueError):decode_bti(bytes(bad))

if __name__=='__main__':unittest.main()
