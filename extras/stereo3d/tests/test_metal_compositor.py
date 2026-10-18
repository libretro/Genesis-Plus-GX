import ctypes as C
import os
import sys
import time
from pathlib import Path
import numpy as np
root=Path(sys.argv[1]).resolve()
os.environ['LG_FIXTURE_ACCEPT_QUILT']='1'
lib=C.CDLL(str(root/'adapter-metal.dylib'))
fixture=C.CDLL(str(root/'metal_fixture.dylib'))
lib.looking_glass_metal_open.argtypes=[C.c_char_p]
lib.looking_glass_metal_open.restype=C.c_void_p
lib.looking_glass_metal_compose.argtypes=[C.c_void_p,C.c_void_p]
lib.looking_glass_metal_compose.restype=C.c_bool
lib.looking_glass_metal_close.argtypes=[C.c_void_p]
lib.looking_glass_metal_close.restype=C.c_bool
fixture.fixture_copy_output.argtypes=[C.c_void_p,C.c_ulong]
fixture.fixture_copy_output.restype=C.c_bool
fixture.fixture_count.argtypes=[C.c_uint]
fixture.fixture_count.restype=C.c_uint
session=lib.looking_glass_metal_open(str(root/'metal_fixture.dylib').encode())
assert session
atlas=np.zeros((704,2048),np.uint16)
colors=np.array([((i%32)<<11)|(((i*3)%64)<<5)|((i*7)%32) for i in range(256)],np.uint16)
atlas[:576,1408:1664]=colors
idx=np.arange(65536,dtype=np.uint32)
for table in range(4):
 values=(((idx&255)+((idx>>8)&255)*(table+1))%127+1).astype(np.uint16)
 atlas[576+table*32:608+table*32]=values.reshape(32,2048)
for y in range(576):
 for layer in range(3): atlas[y,layer*352:layer*352+352]=((np.arange(352)+y+layer*13)%63+1)
 atlas[y,1056:1408]=0
 atlas[y,1667:1670]=[24,12,28]
output=np.empty((448,960,4),np.uint8)
y,x=np.indices((448,960))
for test in range(12):
 w,h=(256,192) if test%2 else (320,224)
 atlas[:576,1664]=w;atlas[:576,1665]=h
 atlas[:576,1670]=test%2
 atlas[:576,1671]=(test//2)%2
 atlas[:576,1672]=(test//4)%2
 atlas[:576,1673]=(test//6)%2
 atlas[:576,1666]=14 if test%3==0 else 0
 atlas[:576,1080:1100]=128|5 if test%3==1 else 0
 assert lib.looking_glass_metal_compose(session,atlas.ctypes.data)
 assert fixture.fixture_copy_output(output.ctypes.data,output.nbytes)
 sy=(y%224)*h//224
 sx=(x%320)*w//320-atlas[sy,1666].astype(np.int32)
 view=(1-y//224)*3+x//320
 direction=2*view/5-1
 if test%2: direction=-direction
 def shift(d):
  v=direction*d
  return np.where(v<0,-np.floor(-v+.5),np.floor(v+.5)).astype(int)
 def lut(t,v): return atlas[576+t*32+(v>>11),v&2047]
 av=atlas[sy,sx-shift(8)+16].astype(np.uint32)
 bv=atlas[sy,352+sx-shift(-4)+16].astype(np.uint32)
 sv=atlas[sy,704+sx-shift(12)+16].astype(np.uint32)
 window=atlas[sy,1056+sx+16].astype(np.uint32)
 av=np.where(window&128,window&127,av)
 sh=(test//2)%2
 index=lut(3 if sh else 1,(lut(2 if sh else 0,(bv<<8)|av).astype(np.uint32)<<8)|sv)
 if (test//4)%2: index=atlas[sy,sx+16]
 outside=(sx<0)|(sx>=w-2*atlas[sy,1666])|(((test//6)%2)!=0)&(sx<8)
 index=np.where(outside,64,index)
 rgb=atlas[sy,1408+index].astype(np.uint32)
 reference=np.stack([np.rint(((rgb>>11)&31)*255/31),np.rint(((rgb>>5)&63)*255/63),np.rint((rgb&31)*255/31),np.full_like(rgb,255)],axis=2).astype(np.uint8)
 assert np.array_equal(reference,output),(test,int(np.max(np.abs(reference.astype(int)-output.astype(int)))))
assert fixture.fixture_count(2)==1,'SDK texture reallocated'
assert fixture.fixture_count(3)==12,'Expected one SDK submission per frame'
assert lib.looking_glass_metal_close(session)
assert fixture.fixture_count(4)==1 and fixture.fixture_count(5)==1
print('PASS: 12 GPU quilts match CPU reference byte-for-byte; geometry changes, eye swap, shadow/highlight, fallback, left blanking, Window; one allocation and clean teardown')

os.environ['GENESIS_LOOKING_GLASS_MAX_FPS']='30'
session=lib.looking_glass_metal_open(str(root/'metal_fixture.dylib').encode())
assert session
before=fixture.fixture_count(3)
assert lib.looking_glass_metal_compose(session,atlas.ctypes.data)
assert lib.looking_glass_metal_compose(session,atlas.ctypes.data)
assert fixture.fixture_count(3)==before+1
time.sleep(.04)
assert lib.looking_glass_metal_compose(session,atlas.ctypes.data)
assert fixture.fixture_count(3)==before+2
assert lib.looking_glass_metal_close(session)
print('PASS: capped submissions skip early frames and resume after interval')
