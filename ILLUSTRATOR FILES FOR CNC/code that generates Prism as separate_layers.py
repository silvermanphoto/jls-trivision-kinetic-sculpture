import xml.etree.ElementTree as ET
import copy
import os

# Configuration
INPUT_FILE = "/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ILLUSTRATOR FILES FOR CNC/01.03.25 Dibond Origami Prism 36in v1.svg"
OUTPUT_DIR = os.path.dirname(INPUT_FILE)
BASENAME = os.path.splitext(os.path.basename(INPUT_FILE))[0]

# Colors and Stroke
COLOR_HOLES = "#FF00FF"  # Bright Purple
COLOR_FOLDS = "#00FFFF"  # Bright Cyan
COLOR_CUTS = "#FF0000"   # Bright Red
STROKE_WIDTH = "1pt"     # 1 pt stroke

def create_base_svg(root):
    new_root = ET.Element('svg')
    # Copy attributes from original root (width, height, viewBox, etc.)
    for key, value in root.attrib.items():
        new_root.set(key, value)
    return new_root

def save_svg(root, suffix):
    tree = ET.ElementTree(root)
    output_path = os.path.join(OUTPUT_DIR, f"{BASENAME}_{suffix}.svg")
    # Register namespaces to avoid ns0 prefixes if possible (basic SVG doesn't strictly need it if we don't mess it up, but good practice)
    ET.register_namespace('', "http://www.w3.org/2000/svg")
    tree.write(output_path, encoding="UTF-8", xml_declaration=True)
    print(f"Saved: {output_path}")

def process_svg():
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()

    # Namespace handling
    ns = {'svg': 'http://www.w3.org/2000/svg'}
    
    # 1. Prepare Root for Combined Output
    root_combined = create_base_svg(root)
    
    # 2. Extract Holes (Circles)
    # Holes are circles. They might be in any group. We'll search recursively or just look in known structure.
    # The prompt implies loose structure or specific IDs. 
    # Based on file view: Holes are 'circle' tags inside 'g id="cut_perimeter"'.
    # We will grab ALL circles for the Holes layer.
    
    # Python keyword args can't have hyphens, so use attrib dict for stroke-width
    group_holes = ET.SubElement(root_combined, 'g', 
                                attrib={'id': "Holes", 'stroke': COLOR_HOLES, 'stroke-width': STROKE_WIDTH, 'fill': "none"})
    
    # scan for all circles
    # Target Diameter: 5.05 mm
    # Radius = 2.525 mm
    # 1 inch = 25.4 mm
    # Radius in inches = 2.525 / 25.4 ~= 0.0994094
    NEW_RADIUS = "0.09941" 

    for circle in root.findall(".//svg:circle", ns):
        c = copy.deepcopy(circle)
        # Strip existing style/stroke attributes to let group dictate, or force them
        if 'stroke' in c.attrib: del c.attrib['stroke']
        if 'stroke-width' in c.attrib: del c.attrib['stroke-width']
        if 'fill' in c.attrib: del c.attrib['fill']
        
        # Set new radius
        c.set('r', NEW_RADIUS)
        
        group_holes.append(c)

    # 3. Extract Folds (Score Lines)
    # Based on file view: 'g id="score_folds"'.
    group_folds = ET.SubElement(root_combined, 'g', 
                                attrib={'id': "Folds", 'stroke': COLOR_FOLDS, 'stroke-width': STROKE_WIDTH, 'fill': "none"})
    
    # Find original folds group
    # Note: namespace usage in findall might need explicit namespace if xmlns is defined
    # The file has xmlns="http://www.w3.org/2000/svg", so use {url}tag
    
    orig_fold_group = None
    for g in root.findall(".//svg:g", ns):
        if g.get('id') == 'score_folds':
            orig_fold_group = g
            break
            
    if orig_fold_group is not None:
        for child in orig_fold_group:
            # We assume these are lines or paths
            elem = copy.deepcopy(child)
            # Remove dasharray if present to make it a solid cut path for CNC
            if 'stroke-dasharray' in elem.attrib: del elem.attrib['stroke-dasharray']
            # Remove styles to inherit from new group
            if 'stroke' in elem.attrib: del elem.attrib['stroke']
            if 'stroke-width' in elem.attrib: del elem.attrib['stroke-width']
            group_folds.append(elem)
    else:
        print("Warning: No group with id='score_folds' found.")

    # 4. Extract Cuts (Perimeter Lines)
    # Based on file view: 'g id="cut_perimeter"'.
    # IMPORTANT: This group also contains the circles (Holes). We must Exclude circles.
    group_cuts = ET.SubElement(root_combined, 'g', 
                               attrib={'id': "Cuts", 'stroke': COLOR_CUTS, 'stroke-width': STROKE_WIDTH, 'fill': "none"})


    orig_cut_group = None
    for g in root.findall(".//svg:g", ns):
        if g.get('id') == 'cut_perimeter':
            orig_cut_group = g
            break
            
    if orig_cut_group is not None:
        for child in orig_cut_group:
            # Skip circles (they are holes)
            # Tag might be '{http://www.w3.org/2000/svg}circle' or just 'circle' depending on parser
            tag = child.tag.split('}')[-1] if '}' in child.tag else child.tag
            
            if tag == 'circle':
                continue
            
            elem = copy.deepcopy(child)
            if 'stroke' in elem.attrib: del elem.attrib['stroke']
            if 'stroke-width' in elem.attrib: del elem.attrib['stroke-width']
            group_cuts.append(elem)
    else:
        print("Warning: No group with id='cut_perimeter' found.")

    # Save files
    save_svg(root_combined, "Combined")

if __name__ == "__main__":
    process_svg()
