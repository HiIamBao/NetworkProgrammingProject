import os
import struct

# Target path
path = '/home/hung1fps/NetworkProgrammingProject/resources/mapData2.bin'
os.makedirs(os.path.dirname(path), exist_ok=True)

W, H = 48,27
EMPTY = 0  # non-collidable
WALL  = 1  # collidable

# Build tile data row-major
data = [EMPTY] * (W * H)
for y in range(H):
    for x in range(W):
        if x == 0 or y == 0 or x == W-1 or y == H-1:
            data[y*W + x] = WALL

# Write binary: width, height, then tiles
with open(path, 'wb') as f:
    f.write(struct.pack('B', W))
    f.write(struct.pack('B', H))
    f.write(bytes(data))

# Verify
size = os.path.getsize(path)
print('Wrote', path, 'size =', size)
print('First bytes:', list(open(path,'rb').read(4)))