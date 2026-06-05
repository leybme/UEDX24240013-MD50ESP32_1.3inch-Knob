#!/usr/bin/env python3
"""
Convert LVGL 8 TRUE_COLOR_ALPHA image data to LVGL 9 RGB565A8 format.

LVGL 8 (16-bit + alpha): Each pixel = [RGB565_low, RGB565_high, Alpha] (interleaved)
LVGL 9 RGB565A8: First all RGB565 pixels, then all alpha bytes (separated)
"""
import re
import sys
import os

def parse_hex_data(content):
    """Extract hex data array from C file."""
    match = re.search(r'uint8_t\s+\w+\[\]\s*=\s*\{([^}]+)\}', content, re.DOTALL)
    if not match:
        return None
    hex_str = match.group(1)
    values = []
    for token in re.findall(r'0x[0-9A-Fa-f]+', hex_str):
        values.append(int(token, 16))
    return values

def convert_alpha_data(data):
    """Convert interleaved RGB565+Alpha to separated RGB565 then Alpha."""
    num_pixels = len(data) // 3
    rgb_data = []
    alpha_data = []
    
    for i in range(num_pixels):
        idx = i * 3
        rgb_data.append(data[idx])      # RGB565 low byte
        rgb_data.append(data[idx + 1])  # RGB565 high byte
        alpha_data.append(data[idx + 2])  # Alpha byte
    
    return rgb_data + alpha_data

def format_c_array(data, per_line=16):
    """Format data as C hex array."""
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i+per_line]
        line = ','.join(f'0x{v:02X}' for v in chunk)
        if i + per_line < len(data):
            line += ','
        lines.append(line)
    return '\n'.join(lines)

def convert_file(filepath):
    """Convert a single image C file from LVGL 8 to LVGL 9 format."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Check if it's an alpha image
    if 'LV_IMG_CF_TRUE_COLOR_ALPHA' not in content:
        print(f"  Skipping (not alpha): {filepath}")
        return False
    
    # Parse dimensions
    w_match = re.search(r'\.header\.w\s*=\s*(\d+)', content)
    h_match = re.search(r'\.header\.h\s*=\s*(\d+)', content)
    if not w_match or not h_match:
        print(f"  ERROR: Cannot parse dimensions: {filepath}")
        return False
    
    w = int(w_match.group(1))
    h = int(h_match.group(1))
    
    # Parse data
    data = parse_hex_data(content)
    if data is None:
        print(f"  ERROR: Cannot parse data array: {filepath}")
        return False
    
    expected_size = w * h * 3
    if len(data) != expected_size:
        print(f"  WARNING: Data size mismatch: expected {expected_size}, got {len(data)}: {filepath}")
    
    # Convert data
    new_data = convert_alpha_data(data)
    new_data_str = format_c_array(new_data)
    
    # Find and replace the data array
    content = re.sub(
        r'(uint8_t\s+\w+\[\]\s*=\s*\{)[^}]+(\})',
        lambda m: m.group(1) + '\n' + new_data_str + '\n' + m.group(2),
        content,
        flags=re.DOTALL
    )
    
    # Update struct
    content = content.replace('.header.always_zero = 0,', '')
    
    # Replace cf and add magic + stride
    cf_new = (
        f'.header.magic = LV_IMAGE_HEADER_MAGIC,\n'
        f'    .header.cf = LV_COLOR_FORMAT_RGB565A8,\n'
        f'    .header.stride = {w * 2},'
    )
    content = content.replace('.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,', cf_new)
    
    # Update type
    content = content.replace('lv_img_dsc_t', 'lv_image_dsc_t')
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"  Converted alpha: {filepath} ({w}x{h}, {len(data)} -> {len(new_data)} bytes)")
    return True

def convert_non_alpha_file(filepath):
    """Update non-alpha image C file struct for LVGL 9."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    if 'LV_IMG_CF_TRUE_COLOR_ALPHA' in content:
        return False  # Skip alpha files, handled separately
    
    if 'LV_IMG_CF_TRUE_COLOR' not in content:
        print(f"  Skipping (unknown format): {filepath}")
        return False
    
    # Parse dimensions
    w_match = re.search(r'\.header\.w\s*=\s*(\d+)', content)
    h_match = re.search(r'\.header\.h\s*=\s*(\d+)', content)
    if not w_match or not h_match:
        print(f"  ERROR: Cannot parse dimensions: {filepath}")
        return False
    
    w = int(w_match.group(1))
    h = int(h_match.group(1))
    
    # Update struct
    content = content.replace('.header.always_zero = 0,', '')
    
    cf_new = (
        f'.header.magic = LV_IMAGE_HEADER_MAGIC,\n'
        f'    .header.cf = LV_COLOR_FORMAT_RGB565,\n'
        f'    .header.stride = {w * 2},'
    )
    content = content.replace('.header.cf = LV_IMG_CF_TRUE_COLOR,', cf_new)
    
    # Update type
    content = content.replace('lv_img_dsc_t', 'lv_image_dsc_t')
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"  Updated non-alpha: {filepath} ({w}x{h})")
    return True

if __name__ == '__main__':
    images_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lib', 'ui', 'src', 'images')
    
    if not os.path.exists(images_dir):
        print(f"ERROR: Images directory not found: {images_dir}")
        sys.exit(1)
    
    print(f"Converting images in: {images_dir}")
    
    for filename in sorted(os.listdir(images_dir)):
        if filename.endswith('.c'):
            filepath = os.path.join(images_dir, filename)
            print(f"Processing: {filename}")
            if not convert_file(filepath):
                convert_non_alpha_file(filepath)
    
    print("Done!")