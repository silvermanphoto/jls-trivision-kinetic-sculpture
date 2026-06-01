import bpy
import os
import math

def setup_scene():
    # Clear existing objects
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

def setup_camera_and_lights(target_obj):
    # Add Camera
    # Position camera to look at the object.
    # Object is flat on XY plane (or XZ depending on import).
    
    # Get object center
    center = target_obj.location
    dims = target_obj.dimensions
    
    # Position camera above and slightly angled
    # For a "Product shot" look
    cam_x = center.x + dims.x / 2
    cam_y = center.y - (dims.y * 2) - 1.0 # Back a bit
    cam_z = center.z + 1.0 # Up a bit
    
    bpy.ops.object.camera_add(location=(cam_x, cam_y, cam_z))
    camera = bpy.context.active_object
    camera.rotation_euler = (math.radians(60), 0, 0) # Look down 60 degrees?
    
    # Make active camera
    bpy.context.scene.camera = camera
    
    # Auto-adjust camera to fit?
    bpy.ops.view3d.camera_to_view_selected() 
    # This requires a context window, might fail in background mode.
    # Manual positioning is safer.
    
    # Let's try a top-down view for clarity first, or an iso view.
    camera.location = (center.x + dims.x/2, center.y - 1.0, 1.5)
    # Look at processed object center
    
    # Simply point camera at object
    # direction = obj_center - camera_pos
    
    # Add Light
    bpy.ops.object.light_add(type='SUN', location=(0, 0, 10))
    light = bpy.context.active_object
    light.data.energy = 5.0

def run():
    svg_path = "/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ILLUSTRATOR FILES FOR CNC/McTell SVGs v1/BOTTOM_RAIL_v1.svg"
    output_path = "/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ILLUSTRATOR FILES FOR CNC/BOTTOM_RAIL_render.png"
    extrude_height = 0.015

    setup_scene()

    print(f"Checking file: {svg_path}")
    if not os.path.exists(svg_path):
        print(f"File NOT found: {svg_path}")
        return
    else:
        print("File exists.")

    # Enable addon just in case
    try:
        bpy.ops.preferences.addon_enable(module="io_curve_svg")
        print("SVG Addon enabled.")
    except Exception as e:
        print(f"Could not enable addon (might be already enabled or missing): {e}")

    # Import
    print("Attempting import...")
    bpy.ops.import_curve.svg(filepath=svg_path)
    
    # Check all objects to see what happened
    print(f"Total objects in scene: {len(bpy.data.objects)}")
    for o in bpy.data.objects:
        print(f" - {o.name} ({o.type})")

    selected = bpy.context.selected_objects
    print(f"Selected objects: {len(selected)}")
    
    if not selected:
        # Fallback: try to find objects that look like the import (Curve)
        # created just now (since we cleared scene)
        candidates = [o for o in bpy.context.scene.objects if o.type == 'CURVE']
        if candidates:
            print(f"Found {len(candidates)} curve objects. Selecting them.")
            for o in candidates:
                o.select_set(True)
            selected = candidates
        else:
            print("Nothing imported (no curves found).")
            return

    bpy.context.view_layer.objects.active = selected[0]
    if len(selected) > 1:
        bpy.ops.object.join()
    
    obj = bpy.context.active_object
    
    # Add Solidify
    mod = obj.modifiers.new(name="Solidify", type='SOLIDIFY')
    mod.thickness = extrude_height
    mod.offset = 0
    
    # Material? Give it a wood color
    mat = bpy.data.materials.new(name="Plywood")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs['Base Color'].default_value = (0.8, 0.6, 0.4, 1) # Wood-ish
    
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    # Setup Render
    setup_camera_and_lights(obj)
    
    # Render settings
    scene = bpy.context.scene
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.filepath = output_path
    
    print(f"Rendering to {output_path}...")
    bpy.ops.render.render(write_still=True)
    print("Render complete.")

if __name__ == "__main__":
    run()
