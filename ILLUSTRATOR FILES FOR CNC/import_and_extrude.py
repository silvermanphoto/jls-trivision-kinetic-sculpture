import bpy
import os
import math

def run_import_and_extrude():
    # --- CONFIGURATION ---
    # Path to your SVGs
    svg_folder = "/Users/joelsilverman/Desktop/2025 Files/25-033 Trivision Kinetic Sculpture/ILLUSTRATOR FILES FOR CNC/McTell SVGs v1"
    
    # Material Thickness (15mm)
    # Blender's default unit is Meters. 15mm = 0.015m
    thickness_m = 0.015 
    
    # Spacing between parts (e.g., 20mm)
    spacing = 0.02
    
    # Scale factor (Illustrator SVGs often import very small in Blender)
    # Usually they come in at 72dpi or 96dpi relative to meters.
    # Often scaling up by 10 or 100 is needed, or just keep as is if the generated SVGs were unit-aware.
    # We will apply scale=1 for now, but enabling "Clamp" on solidify prevents issues.
    global_scale = 1.0

    # --- SETUP ---
    # Runs in a new, empty scene (run_in_new_scene below): nothing is deleted.

    # Ensure Unit System
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.length_unit = 'METERS'
    
    # Check folder
    if not os.path.exists(svg_folder):
        print(f"Error: Folder not found: {svg_folder}")
        return

    # Get SVG files
    files = [f for f in os.listdir(svg_folder) if f.lower().endswith(".svg")]
    files.sort() # Ensure consistent order
    
    if not files:
        print("No SVGs found!")
        return
        
    print(f"Found {len(files)} SVGs. Processing...")

    current_x = 0.0
    
    # Add a collection to keep things organized
    main_coll = bpy.data.collections.new("CNC_Parts")
    bpy.context.scene.collection.children.link(main_coll)

    for filename in files:
        full_path = os.path.join(svg_folder, filename)
        
        # 1. Import SVG
        # This usually creates a Collection for the SVG containing curves
        bpy.ops.import_curve.svg(filepath=full_path)
        
        # The import creates a new collection named after the file (usually)
        # We need to find the objects that were just imported.
        # Strategy: SVG import selects the new objects.
        imported_objects = bpy.context.selected_objects
        
        if not imported_objects:
            print(f"Warning: Nothing imported for {filename}")
            continue
            
        # 2. Join Curves
        # SVGs often come in as many separate curves. We want one object per file.
        bpy.context.view_layer.objects.active = imported_objects[0]
        if len(imported_objects) > 1:
            bpy.ops.object.join()
            
        part_obj = bpy.context.active_object
        part_obj.name = filename.replace(".svg", "")
        
        # Unlink from the messy import collection and link to our main one
        for coll in part_obj.users_collection:
            coll.objects.unlink(part_obj)
        main_coll.objects.link(part_obj)
        
        # 3. Geometry Fixes (Set Origin)
        # Set origin to geometry center so we can position it easily
        bpy.ops.object.origin_set(type='GEOMETRY_ORIGIN', center='BOUNDS')
        
        # 4. Extrude (Solidify Modifier)
        # Note: SVG Curves are 2D. We can either use "Extrude" in data, or Modifier.
        # Modifier is non-destructive and often cleaner.
        mod = part_obj.modifiers.new(name="Solidify", type='SOLIDIFY')
        mod.thickness = thickness_m
        mod.offset = 0 # Center extrusion
        
        # 5. Positioning
        # Get dimensions
        # Note: Dimensions might be 0 z if it's a curve.
        width_x = part_obj.dimensions.x
        
        # Place at current X
        # Since origin is center, location should be current_x + half_width
        part_obj.location.x = current_x + (width_x / 2)
        part_obj.location.y = 0
        part_obj.location.z = 0
        
        # 6. Apply Material (Basic Wood)
        mat = bpy.data.materials.get("Plywood")
        if not mat:
            mat = bpy.data.materials.new(name="Plywood")
            mat.diffuse_color = (0.6, 0.4, 0.2, 1.0) # Brownish
        
        if not part_obj.data.materials:
            part_obj.data.materials.append(mat)
        else:
            part_obj.data.materials[0] = mat
            
        print(f"Processed: {filename} | Width: {width_x:.4f}m")
        
        # Increment X for next part
        current_x += width_x + spacing

    print("Done! All parts imported and arranged.")

def run_in_new_scene(build, name):
    """Run build() in a new, empty scene, so nothing in the open file is deleted.
    The window switches to the new scene; the scene that was open is untouched."""
    scene = bpy.data.scenes.new(name)
    scene.world = bpy.context.scene.world      # same background as the open scene
    window = bpy.context.window or next(iter(bpy.context.window_manager.windows), None)
    if window is not None:
        window.scene = scene
    with bpy.context.temp_override(window=window, scene=scene,
                                   view_layer=scene.view_layers[0]):
        build()

if __name__ == "__main__":
    run_in_new_scene(run_import_and_extrude, "CNC parts")
