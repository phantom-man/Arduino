"""
3MF Exporter for Multi-Color Parts with Embedded Orca Slicer Settings
Creates 3MF files with multiple mesh bodies, each assigned to a filament/color,
plus embedded print/filament settings that Orca Slicer loads automatically.

3MF is a ZIP archive containing XML model data with embedded meshes.
Orca Slicer reads Metadata/plate_*.config and Metadata/project_settings.config.
"""

import zipfile
import io
import os
import json
import xml.etree.ElementTree as ET
import cadquery as cq
from config import STL_DIR


# PETG Translucent color palette
COLORS = {
    "red":    "#CC2222CC",   # Translucent red
    "yellow": "#CCAA22CC",   # Translucent yellow/gold
    "black":  "#222222CC",   # Translucent black/smoke
    "clear":  "#EEEEEEBB",   # Translucent clear
    "asagf":  "#333333FF",   # ASA-GF Black (opaque)
}

# Orca Slicer filament slot mapping
FILAMENT_SLOTS = {
    "red": 1,
    "yellow": 2,
    "black": 3,
    "clear": 4,
    "asagf": 0,
}

# ============================================================
# Orca Slicer embedded settings profiles
# ============================================================

ORCA_SETTINGS_ASAGF = {
    # Print settings
    "layer_height": "0.2",
    "initial_layer_print_height": "0.25",
    "wall_loops": "6",
    "top_shell_layers": "5",
    "bottom_shell_layers": "5",
    "sparse_infill_density": "40%",
    "sparse_infill_pattern": "gyroid",
    "outer_wall_speed": "80",
    "inner_wall_speed": "120",
    "sparse_infill_speed": "120",
    "initial_layer_speed": "40",
    "top_surface_speed": "80",
    "travel_speed": "300",
    "enable_support": "0",
    "brim_type": "outer_only",
    "brim_width": "10",
    "seam_position": "aligned",
    "z_hop": "0.4",
    # Filament settings
    "nozzle_temperature": "270",
    "nozzle_temperature_initial_layer": "270",
    "bed_temperature": "105",
    "bed_temperature_initial_layer": "105",
    "chamber_temperature": "60",
    "fan_min_speed": "0",
    "fan_max_speed": "10",
    "filament_retraction_length": "1.0",
    "filament_retraction_speed": "40",
    "filament_flow_ratio": "1.03",
    "filament_type": "ASA",
    # Quality
    "line_width": "0.45",
    "ironing_type": "no ironing",
    "reduce_crossing_wall": "1",
}

ORCA_SETTINGS_PETG = {
    # === Print Settings ===
    "layer_height": "0.16",
    "initial_layer_print_height": "0.24",
    "wall_loops": "3",
    "top_shell_layers": "5",
    "bottom_shell_layers": "4",
    "sparse_infill_density": "20%",
    "sparse_infill_pattern": "gyroid",
    # Speeds — conservative outer wall for translucent surface quality
    "outer_wall_speed": "50",
    "inner_wall_speed": "80",
    "sparse_infill_speed": "100",
    "initial_layer_speed": "25",
    "top_surface_speed": "40",
    "travel_speed": "200",
    "internal_solid_infill_speed": "80",
    "bridge_speed": "30",
    # Adhesion & support
    "enable_support": "0",
    "brim_type": "auto_brim",
    "brim_width": "5",
    "seam_position": "aligned",
    "z_hop": "0.4",
    # === Filament Settings (PETG-Translucent on Qidi Q2) ===
    # Hardened steel nozzle needs +5-10°C vs brass
    "nozzle_temperature": "240",
    "nozzle_temperature_initial_layer": "245",
    "bed_temperature": "80",
    "bed_temperature_initial_layer": "85",
    # Low chamber temp prevents PETG warping without overheating
    "chamber_temperature": "35",
    # PETG needs moderate cooling — too much = layer adhesion loss,
    # too little = stringing. 40-70% is the sweet spot.
    "fan_min_speed": "40",
    "fan_max_speed": "70",
    "close_fan_the_first_x_layers": "3",
    # Retraction — direct drive Qidi Q2; short, controlled
    "filament_retraction_length": "0.8",
    "filament_retraction_speed": "35",
    "filament_deretraction_speed": "25",
    "filament_retract_before_wipe": "70%",
    "filament_wipe_distance": "1.5",
    "filament_retraction_minimum_travel": "1.5",
    "filament_retract_when_changing_layer": "1",
    "filament_flow_ratio": "0.98",
    "filament_type": "PETG",
    # === Quality Settings ===
    "line_width": "0.45",
    "ironing_type": "no ironing",
    "reduce_crossing_wall": "1",
    # Pressure advance / flow smoothing
    "slow_down_layer_time": "8",
    "slow_down_min_speed": "15",
    # Overhang handling — PETG sags easily
    "detect_overhang_wall": "1",
    "overhang_speed_classic": "1",
    # Avoid stringing on travel
    "reduce_infill_retraction": "1",
    "only_retract_when_crossing_perimeter": "1",
    # === Multi-color AMS ===
    "flush_into_infill": "1",
    "flush_into_objects": "0",
    "prime_tower_brim_width": "3",
    "prime_tower_width": "40",
    "flush_volumes_vector": "120|120|120|120",
    "wipe_distance": "2",
}


def shape_to_triangles(shape, tolerance=0.01, angular_tolerance=0.1):
    """
    Tessellate a CadQuery shape into vertices and triangle indices.
    Returns (vertices_list, triangles_list).
    """
    import tempfile
    
    # CadQuery STL export requires a file path, not BytesIO
    with tempfile.NamedTemporaryFile(suffix='.stl', delete=False) as tmp:
        tmp_path = tmp.name
    
    try:
        cq.exporters.export(
            shape, tmp_path, exportType="STL",
            tolerance=tolerance, angularTolerance=angular_tolerance,
        )
        with open(tmp_path, 'rb') as f:
            stl_data = f.read()
    finally:
        os.remove(tmp_path)
    
    # Parse binary STL
    vertices, triangles = _parse_binary_stl(stl_data)
    return vertices, triangles


def _parse_binary_stl(data):
    """Parse binary STL data into deduplicated vertices and triangle indices."""
    import struct
    
    # Binary STL: 80-byte header + 4-byte triangle count + triangles
    n_triangles = struct.unpack_from('<I', data, 80)[0]
    
    vertices = []
    triangles = []
    vertex_map = {}
    
    offset = 84
    for _ in range(n_triangles):
        # Each triangle: normal (3 floats) + 3 vertices (9 floats) + attributes (2 bytes) = 50 bytes
        vals = struct.unpack_from('<12fH', data, offset)
        offset += 50
        
        # Skip normal (vals[0:3]), read 3 vertices
        tri_indices = []
        for vi in range(3):
            vx = round(vals[3 + vi * 3], 6)
            vy = round(vals[4 + vi * 3], 6)
            vz = round(vals[5 + vi * 3], 6)
            key = (vx, vy, vz)
            
            if key not in vertex_map:
                vertex_map[key] = len(vertices)
                vertices.append(key)
            tri_indices.append(vertex_map[key])
        
        triangles.append(tuple(tri_indices))
    
    return vertices, triangles


def create_3mf(color_bodies, filename, title="Dragon Jewelry Box Part",
               profile="petg"):
    """
    Create a 3MF file with multiple colored mesh bodies and embedded
    Orca Slicer print settings.
    
    Args:
        color_bodies: list of (color_name, cadquery_shape) tuples
                      color_name must be a key in COLORS dict
        filename: output filename (saved to STL_DIR)
        title: model title
        profile: "petg" or "asagf" — selects embedded slicer settings
    """
    filepath = os.path.join(STL_DIR, filename)
    
    # Select the settings profile
    if profile == "asagf":
        orca_settings = ORCA_SETTINGS_ASAGF
    else:
        orca_settings = ORCA_SETTINGS_PETG
    
    # Build the 3D model XML
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    ns_m = "http://schemas.microsoft.com/3dmanufacturing/material/2015/02"
    
    ET.register_namespace('', ns)
    ET.register_namespace('m', ns_m)
    
    model = ET.Element(f'{{{ns}}}model', attrib={
        'unit': 'millimeter',
        f'xmlns:m': ns_m,
    })
    
    # Metadata
    meta_title = ET.SubElement(model, f'{{{ns}}}metadata', name='Title')
    meta_title.text = title
    meta_app = ET.SubElement(model, f'{{{ns}}}metadata', name='Application')
    meta_app.text = 'DragonJewelryBox-CadQuery'
    
    # Resources
    resources = ET.SubElement(model, f'{{{ns}}}resources')
    
    # Base material group for colors
    basemats = ET.SubElement(resources, f'{{{ns_m}}}basematerials', id="1")
    color_to_matidx = {}
    for idx, (cname, cval) in enumerate(COLORS.items()):
        base = ET.SubElement(basemats, f'{{{ns_m}}}base',
                             name=f"PETG-{cname.capitalize()}", displaycolor=cval)
        color_to_matidx[cname] = idx
    
    # Create mesh objects for each color body
    object_ids = []
    for obj_idx, (color_name, shape) in enumerate(color_bodies):
        obj_id = str(obj_idx + 2)  # IDs start at 2 (1 is materials)
        object_ids.append((obj_id, color_name))
        
        vertices, triangles = shape_to_triangles(shape)
        
        mat_idx = color_to_matidx.get(color_name, 0)
        
        obj_elem = ET.SubElement(resources, f'{{{ns}}}object',
                                  id=obj_id, type="model", pid="1", pindex=str(mat_idx))
        
        mesh = ET.SubElement(obj_elem, f'{{{ns}}}mesh')
        
        # Vertices
        verts_elem = ET.SubElement(mesh, f'{{{ns}}}vertices')
        for vx, vy, vz in vertices:
            ET.SubElement(verts_elem, f'{{{ns}}}vertex',
                         x=f"{vx:.6f}", y=f"{vy:.6f}", z=f"{vz:.6f}")
        
        # Triangles
        tris_elem = ET.SubElement(mesh, f'{{{ns}}}triangles')
        for v1, v2, v3 in triangles:
            ET.SubElement(tris_elem, f'{{{ns}}}triangle',
                         v1=str(v1), v2=str(v2), v3=str(v3))
    
    # Build section: reference all objects as components of a single build item
    # For Orca Slicer AMS: each object = one filament assignment
    build = ET.SubElement(model, f'{{{ns}}}build')
    for obj_id, color_name in object_ids:
        ET.SubElement(build, f'{{{ns}}}item', objectid=obj_id)
    
    # Write the 3MF ZIP archive
    model_xml = io.BytesIO()
    tree = ET.ElementTree(model)
    ET.indent(tree, space='  ')
    tree.write(model_xml, xml_declaration=True, encoding='UTF-8')
    model_xml.seek(0)
    
    # Content types XML
    content_types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
        '  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n'
        '  <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
        '</Types>\n'
    )
    
    # Relationships XML
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
        '  <Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
        '</Relationships>\n'
    )
    
    with zipfile.ZipFile(filepath, 'w', zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('[Content_Types].xml', content_types)
        zf.writestr('_rels/.rels', rels)
        zf.writestr('3D/3dmodel.model', model_xml.getvalue())
        
        # Embed Orca Slicer project settings
        # Orca reads Metadata/project_settings.config as key=value pairs
        settings_lines = []
        for key, val in orca_settings.items():
            settings_lines.append(f"{key} = {val}")
        zf.writestr('Metadata/project_settings.config',
                     '\n'.join(settings_lines))
        
        # Plate config (per-object settings for multi-material)
        plate_config = _build_plate_config(object_ids, orca_settings)
        zf.writestr('Metadata/plate_1.config', plate_config)
    
    file_size_kb = os.path.getsize(filepath) / 1024
    n_bodies = len(color_bodies)
    colors_used = list(set(c for c, _ in color_bodies))
    print(f"  Exported 3MF: {filepath}")
    print(f"    {n_bodies} bodies, colors: {', '.join(colors_used)}, "
          f"profile: {profile.upper()}, size: {file_size_kb:.1f} KB")
    return filepath


def _build_plate_config(object_ids, settings):
    """Build Orca Slicer plate config XML for per-object material assignment."""
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<config>']
    
    for obj_id, color_name in object_ids:
        slot = FILAMENT_SLOTS.get(color_name, 0)
        lines.append(f'  <object id="{obj_id}">')
        lines.append(f'    <metadata key="extruder" value="{slot + 1}"/>')
        lines.append(f'    <metadata key="name" value="{color_name}"/>')
        lines.append(f'  </object>')
    
    # Embed print settings at plate level
    lines.append('  <plate>')
    lines.append('    <metadata key="plater_id" value="1"/>')
    lines.append('    <metadata key="plater_name" value="Plate 1"/>')
    for obj_id, _ in object_ids:
        lines.append(f'    <metadata key="object_id" value="{obj_id}"/>')
    lines.append('  </plate>')
    
    lines.append('</config>')
    return '\n'.join(lines)
