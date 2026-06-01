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
    
    # --- MODIFY GEOMETRY FOR SPLIT TAB DESIGN ---
    
    # 1. Update ViewBox and Width (15.75 -> 16.25)
    root.set('width', '16.25in')
    root.set('viewBox', '0 0 16.25 44.362')
    
    # 2. Shrink Pink Tab (Left)
    # Move x=0.5 lines to x=1.5
    # (Checking loose match for "0.5000" or similar)
    for elem in root.iter():
        if 'x1' in elem.attrib and elem.attrib['x1'] == "0.5000":
            elem.set('x1', "1.5000")
        if 'x2' in elem.attrib and elem.attrib['x2'] == "0.5000":
            elem.set('x2', "1.5000")
            
    # 3. Create Yellow Tab (Right)
    # We need to extend from x=15.25 to x=16.25
    # Finding the vertical line at 15.25
    
    green_edge_cut = None
    cuts_group = None
    folds_group = None
    
    for g in root.findall(".//svg:g", ns):
        if g.get('id') == 'cut_perimeter':
            cuts_group = g
        if g.get('id') == 'score_folds':
            folds_group = g
            
    # Process Cuts to find x=15.25 line and extend top/bottom
    if cuts_group is not None:
        lines_to_remove = []
        lines_to_add = []
        
        for child in cuts_group:
            if child.tag.endswith('line'):
                x1 = child.get('x1')
                x2 = child.get('x2')
                y1 = child.get('y1')
                y2 = child.get('y2')
                
                # Check for Green Edge (Vertical at 15.25)
                if x1 == "15.2500" and x2 == "15.2500":
                    green_edge_cut = child
                    lines_to_remove.append(child) # Move to Folds later
                    continue # SKIP extension logic for this line!
                    
                # Check for Top/Bottom Horizontal Lines ending at 15.25
                # e.g. x2=15.25. We need to Extend them to 16.25
                if x2 == "15.2500":
                    child.set('x2', "16.2500")
                elif x1 == "15.2500": # Unlikely if drawing left-to-right but possible
                    child.set('x1', "16.2500")

        # Move Green Edge to Folds (Convert Cut to Fold)
        if green_edge_cut is not None and folds_group is not None:
            # We remove it from Cuts (done via loop)
            for l in lines_to_remove:
                cuts_group.remove(l)
                
            # Add to Folds
            new_fold = copy.deepcopy(green_edge_cut)
            # Remove existing styles
            if 'stroke' in new_fold.attrib: del new_fold.attrib['stroke']
            if 'stroke-width' in new_fold.attrib: del new_fold.attrib['stroke-width']
            # Ensure it has fold style via group, but separate_layers logic handles that.
            # Just append to folds_group for now so it gets extracted later
            folds_group.append(new_fold)
            
            # Create NEW Vertical Cut at 16.25 (The Yellow Tab Edge)
            new_cut = ET.Element('line', attrib={
                'x1': "16.2500", 'y1': green_edge_cut.get('y1'),
                'x2': "16.2500", 'y2': green_edge_cut.get('y2')
            })
            cuts_group.append(new_cut)

    # --- END GEOMETRY MODIFICATION ---

    # 1. Prepare Root for Combined Output
    root_combined = create_base_svg(root)
    
    # 2. Extract Holes
    # Updated logic: Find circles anywhere
    group_holes = ET.SubElement(root_combined, 'g', 
                                attrib={'id': "Holes", 'stroke': COLOR_HOLES, 'stroke-width': STROKE_WIDTH, 'fill': "none"})
    
    # Radius = 0.09941
    NEW_RADIUS = "0.09941" 

    for circle in root.findall(".//svg:circle", ns):
        c = copy.deepcopy(circle)
        if 'stroke' in c.attrib: del c.attrib['stroke']
        if 'stroke-width' in c.attrib: del c.attrib['stroke-width']
        if 'fill' in c.attrib: del c.attrib['fill']
        c.set('r', NEW_RADIUS)
        group_holes.append(c)

    # 3. Extract Folds
    group_folds = ET.SubElement(root_combined, 'g', 
                                attrib={'id': "Folds", 'stroke': COLOR_FOLDS, 'stroke-width': STROKE_WIDTH, 'fill': "none"})
    
    if folds_group is not None:
        for child in folds_group:
            elem = copy.deepcopy(child)
            if 'stroke-dasharray' in elem.attrib: del elem.attrib['stroke-dasharray']
            if 'stroke' in elem.attrib: del elem.attrib['stroke']
            if 'stroke-width' in elem.attrib: del elem.attrib['stroke-width']
            group_folds.append(elem)

    # 4. Extract Cuts
    group_cuts = ET.SubElement(root_combined, 'g', 
                               attrib={'id': "Cuts", 'stroke': COLOR_CUTS, 'stroke-width': STROKE_WIDTH, 'fill': "none"})

    if cuts_group is not None:
        for child in cuts_group:
            tag = child.tag.split('}')[-1] if '}' in child.tag else child.tag
            if tag == 'circle': continue # Skip holes in cut group
            
            elem = copy.deepcopy(child)
            if 'stroke' in elem.attrib: del elem.attrib['stroke']
            if 'stroke-width' in elem.attrib: del elem.attrib['stroke-width']
            group_cuts.append(elem)

    # Save files
    save_svg(root_combined, "v2_SplitTab")

if __name__ == "__main__":
    process_svg()
