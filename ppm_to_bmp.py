import sys

def ppm_to_bmp(ppm_path, bmp_path):
    with open(ppm_path, 'rb') as f:
        header = f.readline().decode().strip()
        if header != 'P6':
            print("Not a P6 PPM file")
            return
        
        # Skip comments
        line = f.readline().decode().strip()
        while line.startswith('#'):
            line = f.readline().decode().strip()
            
        width, height = map(int, line.split())
        maxval = int(f.readline().decode().strip())
        
        data = f.read()

    # BMP Header
    # File Header (14 bytes)
    # DIB Header (40 bytes)
    # Pixel Data
    row_size = (width * 3 + 3) & ~3
    pixel_data_size = row_size * height
    file_size = 14 + 40 + pixel_data_size
    
    file_header = bytearray(14)
    file_header[0:2] = b'BM'
    file_header[2:6] = file_size.to_bytes(4, 'little')
    file_header[10:14] = (14 + 40).to_bytes(4, 'little')
    
    dib_header = bytearray(40)
    dib_header[0:4] = (40).to_bytes(4, 'little')
    dib_header[4:8] = width.to_bytes(4, 'little')
    dib_header[8:12] = height.to_bytes(4, 'little')
    dib_header[12:14] = (1).to_bytes(2, 'little')
    dib_header[14:16] = (24).to_bytes(2, 'little')
    dib_header[20:24] = pixel_data_size.to_bytes(4, 'little')
    
    # PPM is top-to-bottom, BMP is bottom-to-top by default, or top-to-bottom if height is negative.
    # Let's write bottom-to-top (standard).
    pixel_data = bytearray(pixel_data_size)
    for y in range(height):
        ppm_y = height - 1 - y
        for x in range(width):
            ppm_idx = (ppm_y * width + x) * 3
            bmp_idx = y * row_size + x * 3
            if ppm_idx + 2 < len(data):
                # RGB to BGR
                pixel_data[bmp_idx] = data[ppm_idx + 2]     # B
                pixel_data[bmp_idx + 1] = data[ppm_idx + 1] # G
                pixel_data[bmp_idx + 2] = data[ppm_idx]     # R
                
    with open(bmp_path, 'wb') as f:
        f.write(file_header)
        f.write(dib_header)
        f.write(pixel_data)
        
    print(f"Successfully converted {ppm_path} to {bmp_path}")

if __name__ == '__main__':
    ppm_to_bmp('screen.ppm', 'screen.bmp')
