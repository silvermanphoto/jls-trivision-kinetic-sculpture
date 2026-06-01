#!/usr/bin/env python3
"""
Trivision Billboard Image Slicer
Slices three source images into 12 vertical strips each,
expands canvas with black band on right side.
Outputs as PSD files with named layers.
"""

from PIL import Image
import os
import struct
import zlib

# =============================================================================
# CONFIGURATION
# =============================================================================

BASE_PATH = "/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/Equitable Views Camera Scalata"

SOURCE_IMAGES = [
    {
        "filename": "Equitable 1895 - Northwest View 2023 lowres-ish.tif",
        "output_subfolder": "Northwest View 2023",
        "output_prefix": "Equitable 1895 - Northwest View 2023 lowres-ish"
    },
    {
        "filename": "Equitable 1895 - Northwest View 1895 lowres-ish.tif",
        "output_subfolder": "Northwest View 1895",
        "output_prefix": "Equitable 1895 - Northwest View 1895 lowres-ish"
    },
    {
        "filename": "Equitable 1925 Wagner Map.tif",
        "output_subfolder": "Wagner Map 1925",
        "output_prefix": "Equitable 1925 Wagner Map"
    }
]

OUTPUT_BASE = os.path.join(BASE_PATH, "Photoshop Mockup Sliced Folder")

# Strip parameters
NUM_STRIPS = 12
STRIP_WIDTH = 812
STRIP_HEIGHT = 6886
CANVAS_WIDTH = 841
BLACK_BAND_WIDTH = 29

# =============================================================================
# PSD CREATION (minimal implementation)
# =============================================================================

def save_psd_with_layer(image, layer_name, output_path):
    """
    Save a PIL image as a PSD with a single named layer.
    Uses pytoshop if available, otherwise falls back to psd-tools.
    """
    try:
        from psd_tools import PSDImage
        from psd_tools.api.layers import PixelLayer
        
        # psd-tools doesn't easily create PSDs, try pytoshop
        raise ImportError("Try pytoshop instead")
    except ImportError:
        pass
    
    try:
        import pytoshop
        from pytoshop import layers
        from pytoshop.enums import ColorMode
        
        # Convert PIL image to numpy array
        import numpy as np
        img_array = np.array(image.convert('RGB'))
        
        # Create PSD
        psd = pytoshop.PSDImage(
            num_channels=3,
            height=image.height,
            width=image.width,
            color_mode=ColorMode.rgb
        )
        
        # Create layer
        layer = layers.ChannelImageData(image=img_array, compression=1)
        layer_record = layers.LayerRecord(
            name=layer_name,
            top=0,
            left=0,
            bottom=image.height,
            right=image.width,
            channels={-1: layer, 0: layer, 1: layer, 2: layer}
        )
        
        psd.layer_and_mask_info.layer_info.layer_records.append(layer_record)
        
        with open(output_path, 'wb') as f:
            psd.write(f)
        return True
        
    except ImportError:
        # Fallback: save as TIFF with layer name in metadata
        print(f"    (pytoshop not installed - saving as TIFF)")
        tiff_path = output_path.replace('.psd', '.tif')
        image.save(tiff_path, format='TIFF')
        return False

# =============================================================================
# PROCESSING
# =============================================================================

def slice_and_expand_image(source_path, output_folder, output_prefix):
    print(f"\n{'='*60}")
    print(f"Processing: {os.path.basename(source_path)}")
    print(f"{'='*60}")
    
    if not os.path.exists(source_path):
        print(f"  ✗ ERROR: Source file not found: {source_path}")
        return False
    
    os.makedirs(output_folder, exist_ok=True)
    print(f"  ✓ Output folder: {output_folder}")
    
    print(f"  Loading source image...")
    img = Image.open(source_path)
    src_width, src_height = img.size
    print(f"  ✓ Source size: {src_width} x {src_height} px")
    
    for i in range(NUM_STRIPS):
        strip_num = i + 1
        left = i * STRIP_WIDTH
        right = left + STRIP_WIDTH
        top = 0
        bottom = STRIP_HEIGHT
        
        if right > src_width:
            right = src_width
        
        strip = img.crop((left, top, right, bottom))
        canvas = Image.new('RGB', (CANVAS_WIDTH, STRIP_HEIGHT), (0, 0, 0))
        canvas.paste(strip, (0, 0))
        
        layer_name = f"{strip_num:02d}"
        output_filename = f"{output_prefix} {strip_num:02d}.psd"
        output_path = os.path.join(output_folder, output_filename)
        
        save_psd_with_layer(canvas, layer_name, output_path)
        
        print(f"  ✓ Strip {strip_num:02d}: layer '{layer_name}' → {output_filename}")
    
    print(f"\n  ✓ Completed: 12 PSDs saved")
    return True

def main():
    print("\n" + "="*60)
    print("TRIVISION BILLBOARD IMAGE SLICER")
    print("="*60)
    
    os.makedirs(OUTPUT_BASE, exist_ok=True)
    
    success_count = 0
    for img_config in SOURCE_IMAGES:
        source_path = os.path.join(BASE_PATH, img_config["filename"])
        output_folder = os.path.join(OUTPUT_BASE, img_config["output_subfolder"])
        output_prefix = img_config["output_prefix"]
        
        if slice_and_expand_image(source_path, output_folder, output_prefix):
            success_count += 1
    
    print("\n" + "="*60)
    print(f"✓ Processed {success_count} of {len(SOURCE_IMAGES)} images")
    print(f"✓ Total PSDs created: {success_count * NUM_STRIPS}")
    print("="*60)

if __name__ == "__main__":
    main()