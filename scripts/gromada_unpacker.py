import struct
import os
import sys
import re
from PIL import Image

SECTION_HEADER_SIZE = 11
NAME_SIZE = 34
METADATA_SIZE = 229
PIXEL_HEADER_SIZE = 13
PALETTE_SIZE = 768

BOUNDING_BOX_FORMATS = {2, 7, 8}
SKIPPED_FORMATS = {3, 4}


def sanitize_filename(name_bytes: bytes) -> str:
    raw_name = name_bytes.split(b'\x00')[0]
    if not raw_name:
        return "unnamed"
    try:
        name = raw_name.decode('ascii')
    except UnicodeDecodeError:
        try:
            name = raw_name.decode('cp866')
        except UnicodeDecodeError:
            return "unnamed"
    name = name.strip()
    return re.sub(r'[\\/*?:"<>|]', "_", name) if name else "unnamed"


def decode_mode0(raw_data: bytes, width: int, height: int):
    pixels = bytearray(width * height)
    alpha = bytearray(width * height)
    copy_len = min(len(raw_data), width * height)
    pixels[:copy_len] = raw_data[:copy_len]
    for idx in range(copy_len):
        alpha[idx] = 255
    return pixels, alpha, False


def decode_mode2(rle_data: bytes, width: int, height: int, start_y: int, draw_height: int):
    pixels = bytearray(width * height)
    alpha = bytearray(width * height)
    end_y = min(start_y + draw_height, height)
    data_idx = 0
    
    for y in range(max(0, start_y), end_y):
        x = 0
        while data_idx < len(rle_data):
            byte = rle_data[data_idx]
            data_idx += 1
            count = byte & 0x3F
            command = (byte >> 6) & 0x03
            
            if count == 0:
                break
            if command == 0:
                x += count
            elif command == 1:
                for _ in range(count):
                    if x < width:
                        pixels[y * width + x] = 0
                        alpha[y * width + x] = 255
                    x += 1
            elif command == 2:
                for _ in range(count):
                    if data_idx < len(rle_data) and x < width:
                        pixels[y * width + x] = rle_data[data_idx]
                        alpha[y * width + x] = 255
                    data_idx += 1
                    x += 1
            elif command == 3:
                if data_idx < len(rle_data):
                    color = rle_data[data_idx]
                    data_idx += 1
                    for _ in range(count):
                        if x < width:
                            pixels[y * width + x] = color
                            alpha[y * width + x] = 255
                        x += 1
    return pixels, alpha, False


def decode_mode6(rle_data: bytes, width: int, height: int):
    pixels = bytearray(width * height * 4)
    alpha = bytearray(width * height)
    data_idx = 0
    
    for y in range(height):
        x = 0
        while x < width and data_idx < len(rle_data):
            cmd = rle_data[data_idx]
            data_idx += 1
            count = cmd & 0x7F
            repeat = (cmd >> 7) & 0x01
            
            if count == 0:
                continue
            if repeat:
                if data_idx + 1 < len(rle_data):
                    color16 = struct.unpack('<H', rle_data[data_idx:data_idx+2])[0]
                    data_idx += 2
                    b = (color16 & 0x1F) * 255 // 31
                    g = ((color16 >> 5) & 0x1F) * 255 // 31
                    r = ((color16 >> 10) & 0x1F) * 255 // 31
                    for _ in range(count):
                        if x < width:
                            idx = (y * width + x) * 4
                            pixels[idx:idx+4] = (r, g, b, 255)
                            alpha[y * width + x] = 255
                        x += 1
            else:
                for _ in range(count):
                    if data_idx + 1 < len(rle_data) and x < width:
                        color16 = struct.unpack('<H', rle_data[data_idx:data_idx+2])[0]
                        data_idx += 2
                        b = (color16 & 0x1F) * 255 // 31
                        g = ((color16 >> 5) & 0x1F) * 255 // 31
                        r = ((color16 >> 10) & 0x1F) * 255 // 31
                        idx = (y * width + x) * 4
                        pixels[idx:idx+4] = (r, g, b, 255)
                        alpha[y * width + x] = 255
                    else:
                        data_idx += 2
                    x += 1
    return pixels, alpha, True


def decode_mode8(rle_data: bytes, width: int, height: int, start_y: int, draw_height: int):
    pixels = bytearray(width * height)
    alpha = bytearray(width * height)
    end_y = min(start_y + draw_height, height)
    data_idx = 0
    
    for y in range(max(0, start_y), end_y):
        x = 0
        while data_idx < len(rle_data):
            cmd = rle_data[data_idx]
            data_idx += 1
            count = cmd & 0x1F
            opacity = (cmd >> 5) & 0x07
            
            if count == 0:
                break
            if opacity == 0:
                x += count
            else:
                for _ in range(count):
                    if data_idx < len(rle_data) and x < width:
                        pixels[y * width + x] = rle_data[data_idx]
                        alpha[y * width + x] = 255
                    data_idx += 1
                    x += 1
    return pixels, alpha, False


class GromadaUnpacker:
    def __init__(self, file_path: str, out_dir: str):
        self.file_path = file_path
        self.out_dir = out_dir
        self.unpacked_vids_count = 0
        os.makedirs(out_dir, exist_ok=True)

    def run(self):
        with open(self.file_path, 'rb') as f:
            resource_count = struct.unpack('<I', f.read(4))[0]
            print(f"[*] Archive contains {resource_count} resources")

            section_idx = 0
            while True:
                hdr_bytes = f.read(SECTION_HEADER_SIZE)
                if len(hdr_bytes) < SECTION_HEADER_SIZE:
                    break

                sec_id = hdr_bytes[0]
                skip_size = struct.unpack('<I', hdr_bytes[1:5])[0]
                payload_size = skip_size - 6
                if payload_size < 0:
                    break

                payload = f.read(payload_size)
                if len(payload) < payload_size:
                    break

                if sec_id == 0x21:
                    self._parse_vid_section(payload, section_idx)

                section_idx += 1

        print(f"\n[✓] Unpacked {self.unpacked_vids_count} Vids successfully to {self.out_dir}")

    def _parse_vid_section(self, payload: bytes, section_idx: int):
        pos = 0
        while pos < len(payload):
            if pos + NAME_SIZE > len(payload):
                break
            raw_name = payload[pos:pos + NAME_SIZE]
            pos += NAME_SIZE
            name_str = sanitize_filename(raw_name)

            pos += METADATA_SIZE

            if pos + 4 > len(payload):
                break
            payload_size_or_clone_index = struct.unpack('<i', payload[pos:pos + 4])[0]
            pos += 4

            if payload_size_or_clone_index < 0:
                continue

            if pos + PIXEL_HEADER_SIZE > len(payload):
                break
            pix_hdr = payload[pos:pos + PIXEL_HEADER_SIZE]
            pos += PIXEL_HEADER_SIZE

            format_byte = pix_hdr[0]
            frame_count = struct.unpack('<H', pix_hdr[3:5])[0]
            width = struct.unpack('<H', pix_hdr[9:11])[0]
            height = struct.unpack('<H', pix_hdr[11:13])[0]

            if width <= 0 or height <= 0 or width > 4096 or height > 4096:
                width, height = 1, 1

            if pos + PALETTE_SIZE > len(payload):
                break
            pal_data = payload[pos:pos + PALETTE_SIZE]
            pos += PALETTE_SIZE

            pal_8bit = list(pal_data)
            if pal_8bit and max(pal_8bit) <= 63:
                pal_8bit = [v << 2 for v in pal_8bit]
            pal_8bit = [min(255, max(0, v)) for v in pal_8bit]
            if len(pal_8bit) < PALETTE_SIZE:
                pal_8bit += [0] * (PALETTE_SIZE - len(pal_8bit))

            self.unpacked_vids_count += 1
            print(f"  [{self.unpacked_vids_count:03d}] Unpacking: {name_str} ({frame_count} frames)")

            for frame_idx in range(frame_count):
                if pos + 4 > len(payload):
                    break
                frame_size = struct.unpack('<I', payload[pos:pos + 4])[0]
                pos += 4

                if pos + 2 > len(payload):
                    break
                share_index = struct.unpack('<h', payload[pos:pos + 2])[0]
                pos += 2

                remaining_in_block = frame_size - 6
                if remaining_in_block < 0:
                    remaining_in_block = 0

                if share_index != -1 or frame_size <= 6:
                    pos += remaining_in_block
                    continue

                y_offset = 0
                draw_height = height

                if format_byte in BOUNDING_BOX_FORMATS:
                    if pos + 4 > len(payload):
                        break
                    y_offset = struct.unpack('<h', payload[pos:pos + 2])[0]
                    draw_height = struct.unpack('<H', payload[pos + 2:pos + 4])[0]
                    pos += 4
                    rle_data_len = frame_size - 6
                else:
                    rle_data_len = frame_size - 2

                rle_data = payload[pos:pos + rle_data_len]
                pos += rle_data_len

                if format_byte in SKIPPED_FORMATS:
                    continue

                if format_byte in [0, 64, 70]:
                    pixels, alpha, is_rgba = decode_mode0(rle_data, width, height)
                elif format_byte == 2:
                    pixels, alpha, is_rgba = decode_mode2(rle_data, width, height, y_offset, draw_height)
                elif format_byte == 6:
                    pixels, alpha, is_rgba = decode_mode6(rle_data, width, height)
                elif format_byte in [7, 8]:
                    pixels, alpha, is_rgba = decode_mode8(rle_data, width, height, y_offset, draw_height)
                else:
                    pixels, alpha, is_rgba = bytearray(width * height), bytearray(width * height), False

                img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
                rgba_data = []
                for idx in range(width * height):
                    if alpha[idx] == 0:
                        rgba_data.append((0, 0, 0, 0))
                    else:
                        if is_rgba:
                            rgba_data.append((pixels[idx * 4], pixels[idx * 4 + 1], pixels[idx * 4 + 2], pixels[idx * 4 + 3]))
                        else:
                            palette_idx = pixels[idx]
                            rgba_data.append((pal_8bit[palette_idx * 3], pal_8bit[palette_idx * 3 + 1], pal_8bit[palette_idx * 3 + 2], alpha[idx]))
                img.putdata(rgba_data)

                if frame_count == 1:
                    img_name = f"vid_{section_idx:03d}_{name_str}.png"
                else:
                    img_name = f"vid_{section_idx:03d}_{name_str}_f{frame_idx:02d}.png"
                    
                img.save(os.path.join(self.out_dir, img_name))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python gromada_unpack.py <file.res> <out_dir>")
        sys.exit(1)
    GromadaUnpacker(sys.argv[1], sys.argv[2]).run()