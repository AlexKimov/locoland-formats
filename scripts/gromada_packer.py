import struct
import os
import sys
import argparse
import shutil
from typing import Optional
from PIL import Image

SECTION_HEADER_SIZE = 11
NAME_FIELD_SIZE = 34
METADATA_FIELD_SIZE = 229
PIXEL_HEADER_SIZE = 13
PALETTE_SIZE = 768

VID_SECTION_TYPE = 0x21
FORMAT_0_RAW = 0
FORMAT_2_RLE = 2
FORMAT_6_RGB = 6
FORMAT_8_ALPHA = 8
DIRECT_FRAME_SHARE_INDEX = -1


def parse_res_sections(file_path: str) -> list[dict]:
    sections = []
    with open(file_path, 'rb') as file:
        resource_count = struct.unpack('<I', file.read(4))[0]
        for _ in range(resource_count):
            header_bytes = file.read(SECTION_HEADER_SIZE)
            if len(header_bytes) < SECTION_HEADER_SIZE:
                break
                
            skip_size = struct.unpack('<I', header_bytes[1:5])[0]
            payload_size = skip_size - 6
            
            if payload_size < 0:
                break
                
            payload_bytes = file.read(payload_size)
            if len(payload_bytes) < payload_size:
                break
                
            sections.append({
                'header': header_bytes,
                'payload': payload_bytes
            })
    return sections


def extract_vid_metadata(sections: list[dict], source_index: Optional[int]) -> tuple[bytes, bytes]:
    default_name = b'unnamed'.ljust(NAME_FIELD_SIZE, b'\x00')
    default_metadata = b'\x00' * METADATA_FIELD_SIZE

    if source_index is None or source_index < 0 or source_index >= len(sections):
        return default_name, default_metadata

    section = sections[source_index]
    if section['header'][0] != VID_SECTION_TYPE:
        return default_name, default_metadata

    payload = section['payload']
    if len(payload) >= NAME_FIELD_SIZE + METADATA_FIELD_SIZE:
        name = payload[:NAME_FIELD_SIZE]
        metadata = payload[NAME_FIELD_SIZE:NAME_FIELD_SIZE + METADATA_FIELD_SIZE]
        return name, metadata
        
    return default_name, default_metadata


def extract_vid_format_byte(sections: list[dict], source_index: Optional[int]) -> int:
    if source_index is None or source_index < 0 or source_index >= len(sections):
        return FORMAT_2_RLE

    section = sections[source_index]
    if section['header'][0] != VID_SECTION_TYPE:
        return FORMAT_2_RLE

    payload = section['payload']
    header_offset = NAME_FIELD_SIZE + METADATA_FIELD_SIZE + 4
    
    if len(payload) >= header_offset + 1:
        return payload[header_offset]
        
    return FORMAT_2_RLE


def process_image_for_packing(image_path: str) -> tuple[int, int, bytes, bytearray, bytearray, bytearray]:
    source_image = Image.open(image_path).convert('RGBA')
    width, height = source_image.size

    rgb_image = source_image.convert('RGB')
    quantized_image = rgb_image.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    
    raw_palette = quantized_image.getpalette()
    final_palette = bytes([0, 0, 0] + raw_palette[:765])
    
    indexed_data = bytearray(quantized_image.tobytes())
    for pixel_index in range(len(indexed_data)):
        indexed_data[pixel_index] += 1
        
    alpha_mask = bytearray(width * height)
    rgba_data = bytearray(width * height * 4)
    pixel_accessor = source_image.load()
    
    for y_coord in range(height):
        for x_coord in range(width):
            r, g, b, pixel_alpha = pixel_accessor[x_coord, y_coord]
            flat_index = y_coord * width + x_coord
            rgba_index = flat_index * 4
            
            rgba_data[rgba_index] = r
            rgba_data[rgba_index + 1] = g
            rgba_data[rgba_index + 2] = b
            rgba_data[rgba_index + 3] = pixel_alpha
            
            if pixel_alpha < 128:
                indexed_data[flat_index] = 0
                alpha_mask[flat_index] = 0
            else:
                alpha_mask[flat_index] = 255
                
    return width, height, final_palette, indexed_data, alpha_mask, rgba_data


def encode_mode0(indexed_data: bytearray, alpha_mask: bytearray, width: int, height: int) -> tuple[bytes, bytes]:
    return b'', bytes(indexed_data)


def encode_format2_rle(indexed_data: bytearray, alpha_mask: bytearray, width: int, height: int) -> tuple[bytes, bytes]:
    top_row = height
    bottom_row = -1
    
    for y_coord in range(height):
        for x_coord in range(width):
            if alpha_mask[y_coord * width + x_coord] > 0:
                if y_coord < top_row: top_row = y_coord
                if y_coord > bottom_row: bottom_row = y_coord
                break
                
    if bottom_row == -1:
        return struct.pack('<hh', 0, 0), b''
        
    visible_height = bottom_row - top_row + 1
    rle_stream = bytearray()
    
    for y_coord in range(top_row, bottom_row + 1):
        x_coord = 0
        while x_coord < width:
            current_pixel_index = y_coord * width + x_coord
            
            if alpha_mask[current_pixel_index] == 0:
                skip_length = 0
                while (x_coord + skip_length < width and 
                       alpha_mask[y_coord * width + x_coord + skip_length] == 0 and 
                       skip_length < 63):
                    skip_length += 1
                rle_stream.append(skip_length & 0x3F)
                x_coord += skip_length
            else:
                copy_length = 0
                while (x_coord + copy_length < width and 
                       alpha_mask[y_coord * width + x_coord + copy_length] > 0 and 
                       copy_length < 63):
                    copy_length += 1
                    
                rle_stream.append(0x80 | (copy_length & 0x3F))
                for offset in range(copy_length):
                    rle_stream.append(indexed_data[y_coord * width + x_coord + offset])
                x_coord += copy_length
                
        rle_stream.append(0x00)
        
    bounding_box = struct.pack('<hh', top_row, visible_height)
    return bounding_box, bytes(rle_stream)


def encode_mode6(rgba_data: bytearray, width: int, height: int) -> tuple[bytes, bytes]:
    rle_stream = bytearray()
    for y_coord in range(height):
        x_coord = 0
        while x_coord < width:
            idx = (y_coord * width + x_coord) * 4
            alpha = rgba_data[idx + 3]
            
            if alpha == 0:
                skip_length = 0
                while (x_coord + skip_length < width and 
                       rgba_data[(y_coord * width + x_coord + skip_length) * 4 + 3] == 0 and 
                       skip_length < 127):
                    skip_length += 1
                rle_stream.append(skip_length & 0x7F)
                x_coord += skip_length
            else:
                r, g, b = rgba_data[idx], rgba_data[idx + 1], rgba_data[idx + 2]
                color16 = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
                
                fill_length = 0
                is_fill = True
                while x_coord + fill_length < width and fill_length < 127:
                    n_idx = (y_coord * width + x_coord + fill_length) * 4
                    if rgba_data[n_idx + 3] == 0: break
                    cr, cg, cb = rgba_data[n_idx], rgba_data[n_idx + 1], rgba_data[n_idx + 2]
                    c16 = ((cr >> 3) << 10) | ((cg >> 3) << 5) | (cb >> 3)
                    if c16 != color16:
                        is_fill = False
                        break
                    fill_length += 1
                
                if is_fill and fill_length > 2:
                    rle_stream.append(0x80 | (fill_length & 0x7F))
                    rle_stream.extend(struct.pack('<H', color16))
                    x_coord += fill_length
                else:
                    copy_length = 0
                    while x_coord + copy_length < width and copy_length < 127:
                        n_idx = (y_coord * width + x_coord + copy_length) * 4
                        if rgba_data[n_idx + 3] == 0: break
                        copy_length += 1
                    rle_stream.append(copy_length & 0x7F)
                    for i in range(copy_length):
                        n_idx = (y_coord * width + x_coord + i) * 4
                        cr, cg, cb = rgba_data[n_idx], rgba_data[n_idx + 1], rgba_data[n_idx + 2]
                        c16 = ((cr >> 3) << 10) | ((cg >> 3) << 5) | (cb >> 3)
                        rle_stream.extend(struct.pack('<H', c16))
                    x_coord += copy_length
                    
    return b'', bytes(rle_stream)


def encode_mode8(indexed_data: bytearray, alpha_mask: bytearray, width: int, height: int) -> tuple[bytes, bytes]:
    top_row = height
    bottom_row = -1
    
    for y_coord in range(height):
        for x_coord in range(width):
            if alpha_mask[y_coord * width + x_coord] > 0:
                if y_coord < top_row: top_row = y_coord
                if y_coord > bottom_row: bottom_row = y_coord
                break
                
    if bottom_row == -1:
        return struct.pack('<hh', 0, 0), b''
        
    visible_height = bottom_row - top_row + 1
    rle_stream = bytearray()
    
    for y_coord in range(top_row, bottom_row + 1):
        x_coord = 0
        while x_coord < width:
            current_pixel_index = y_coord * width + x_coord
            
            if alpha_mask[current_pixel_index] == 0:
                skip_length = 0
                while (x_coord + skip_length < width and 
                       alpha_mask[y_coord * width + x_coord + skip_length] == 0 and 
                       skip_length < 31):
                    skip_length += 1
                rle_stream.append(skip_length & 0x1F)
                x_coord += skip_length
            else:
                copy_length = 0
                while (x_coord + copy_length < width and 
                       alpha_mask[y_coord * width + x_coord + copy_length] > 0 and 
                       copy_length < 31):
                    copy_length += 1
                    
                rle_stream.append(0xE0 | (copy_length & 0x1F))
                for offset in range(copy_length):
                    rle_stream.append(indexed_data[y_coord * width + x_coord + offset])
                x_coord += copy_length
                
        rle_stream.append(0x00)
        
    bounding_box = struct.pack('<hh', top_row, visible_height)
    return bounding_box, bytes(rle_stream)


def build_frame_block(bounding_box: bytes, rle_stream: bytes) -> bytes:
    share_index_bytes = struct.pack('<h', DIRECT_FRAME_SHARE_INDEX)
    frame_data_bytes = bounding_box + rle_stream
    frame_block_size = 2 + len(frame_data_bytes)
    
    block = bytearray()
    block.extend(struct.pack('<I', frame_block_size))
    block.extend(share_index_bytes)
    block.extend(frame_data_bytes)
    return bytes(block)


def build_vid_payload(name: bytes, metadata: bytes, palette: bytes, frame_block: bytes, width: int, height: int, format_byte: int) -> bytes:
    pixel_header = bytearray(PIXEL_HEADER_SIZE)
    pixel_header[0] = format_byte
    struct.pack_into('<H', pixel_header, 3, 1)
    struct.pack_into('<I', pixel_header, 5, PALETTE_SIZE + len(frame_block))
    struct.pack_into('<H', pixel_header, 9, width)
    struct.pack_into('<H', pixel_header, 11, height)

    payload_size = PIXEL_HEADER_SIZE + PALETTE_SIZE + len(frame_block)
    
    payload = bytearray()
    payload.extend(name)
    payload.extend(metadata)
    payload.extend(struct.pack('<i', payload_size))
    payload.extend(pixel_header)
    payload.extend(palette)
    payload.extend(frame_block)
    return bytes(payload)


def build_section_header(payload_size: int) -> bytes:
    header = bytearray(SECTION_HEADER_SIZE)
    header[0] = VID_SECTION_TYPE
    struct.pack_into('<I', header, 1, payload_size + 6)
    struct.pack_into('<I', header, 5, 1)
    struct.pack_into('<H', header, 9, 0)
    return bytes(header)


def main():
    parser = argparse.ArgumentParser(description="Gromada .res Packer")
    parser.add_argument('res_file', help="Path to the .res archive")
    parser.add_argument('image_file', help="Path to the input image")
    parser.add_argument('--target-index', type=int, help="Index of the resource to replace (required for 'replace' mode)")
    parser.add_argument('--meta-source-index', type=int, help="Index of the resource to copy name, metadata, and format from")
    parser.add_argument('--mode', choices=['replace', 'add'], default='replace', help="Operation mode")
    parser.add_argument('--backup', action='store_true', help="Create a .bak backup")
    args = parser.parse_args()

    if args.backup and os.path.exists(args.res_file):
        shutil.copy2(args.res_file, args.res_file + '.bak')

    if args.mode == 'replace' and args.target_index is None:
        print("[!] Error: --target-index is required for 'replace' mode.")
        sys.exit(1)

    sections = parse_res_sections(args.res_file)
    
    metadata_source_index = args.meta_source_index if args.meta_source_index is not None else args.target_index
    name, metadata = extract_vid_metadata(sections, metadata_source_index)
    format_byte = extract_vid_format_byte(sections, metadata_source_index)

    width, height, palette, indexed_data, alpha_mask, rgba_data = process_image_for_packing(args.image_file)
    
    if format_byte in [FORMAT_0_RAW, 64, 70]:
        bounding_box, rle_stream = encode_mode0(indexed_data, alpha_mask, width, height)
    elif format_byte == FORMAT_6_RGB:
        bounding_box, rle_stream = encode_mode6(rgba_data, width, height)
    elif format_byte in [7, FORMAT_8_ALPHA]:
        bounding_box, rle_stream = encode_mode8(indexed_data, alpha_mask, width, height)
    else:
        bounding_box, rle_stream = encode_format2_rle(indexed_data, alpha_mask, width, height)
        format_byte = FORMAT_2_RLE
        
    frame_block = build_frame_block(bounding_box, rle_stream)
    vid_payload = build_vid_payload(name, metadata, palette, frame_block, width, height, format_byte)
    section_header = build_section_header(len(vid_payload))

    if args.mode == 'replace':
        if args.target_index < 0 or args.target_index >= len(sections):
            print(f"[!] Error: Target index {args.target_index} is out of bounds. Cannot replace.")
            sys.exit(1)
            
        if sections[args.target_index]['header'][0] != VID_SECTION_TYPE:
            print(f"[!] Error: Resource at index {args.target_index} is not a Vid section. Cannot replace.")
            sys.exit(1)
            
        sections[args.target_index]['header'] = section_header
        sections[args.target_index]['payload'] = vid_payload
        print(f"[+] Replaced resource at index {args.target_index} using Format {format_byte}")
    else:
        sections.append({
            'header': section_header,
            'payload': vid_payload
        })
        print(f"[+] Added new resource at index {len(sections) - 1} using Format {format_byte}")

    with open(args.res_file, 'wb') as file:
        file.write(struct.pack('<I', len(sections)))
        for section in sections:
            file.write(section['header'])
            file.write(section['payload'])
            
    print(f"[+] Successfully packed {args.res_file}")


if __name__ == '__main__':
    main()