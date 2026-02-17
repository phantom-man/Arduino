"""
Convert STEP files to 3MF with embedded Orca Slicer ASA-GF settings.
Single-body, single-color — no AMS/multi-material features.

Usage:
    python convert_step_to_3mf.py
    
Reads STEP files from ~/Downloads, outputs 3MF to STL/ directory.
"""

import os
import sys
import zipfile
import io
import struct
import tempfile
import xml.etree.ElementTree as ET

# Add project to path
sys.path.insert(0, os.path.dirname(__file__))

import cadquery as cq
from config import STL_DIR

# Source directory
DOWNLOADS = os.path.join(os.path.expanduser("~"), "Downloads")

# STEP files to convert
STEP_FILES = [
    "X-axis_model.step",
    "Y-axis_model.step",
]

# ============================================================
# ASA-GF "Strength & Speed" Orca Slicer settings
# (Single-body, no AMS keys)
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


def parse_binary_stl(data):
    """Parse binary STL data into deduplicated vertices and triangle indices."""
    n_triangles = struct.unpack_from('<I', data, 80)[0]
    vertices = []
    triangles = []
    vertex_map = {}
    offset = 84
    for _ in range(n_triangles):
        vals = struct.unpack_from('<12fH', data, offset)
        offset += 50
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


def shape_to_triangles(shape, tolerance=0.01, angular_tolerance=0.1):
    """Tessellate a CadQuery shape into vertices and triangle indices."""
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
    return parse_binary_stl(stl_data)


def steps_to_combined_3mf(step_entries, output_filename, title="Combined Project",
                          spacing=10.0):
    """
    Load multiple STEP files and pack them into a single 3MF project
    with embedded ASA-GF settings. Objects are placed side-by-side on the plate.
    
    Args:
        step_entries: list of (step_path, part_name) tuples
        output_filename: output 3MF name (saved to STL_DIR)
        title: project title
        spacing: mm gap between parts on the plate
    """
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    ET.register_namespace('', ns)

    model = ET.Element(f'{{{ns}}}model', attrib={'unit': 'millimeter'})

    # Metadata
    meta_title = ET.SubElement(model, f'{{{ns}}}metadata', name='Title')
    meta_title.text = title
    meta_app = ET.SubElement(model, f'{{{ns}}}metadata', name='Application')
    meta_app.text = 'STEP-to-3MF Converter (ASA-GF)'

    resources = ET.SubElement(model, f'{{{ns}}}resources')
    build = ET.SubElement(model, f'{{{ns}}}build')

    object_entries = []  # (obj_id, part_name, dims)
    cursor_x = 0.0  # running X offset for side-by-side placement

    for idx, (step_path, part_name) in enumerate(step_entries):
        obj_id = str(idx + 1)

        print(f"\n  [{idx+1}] Loading: {os.path.basename(step_path)}")
        shape = cq.importers.importStep(step_path)

        bb = shape.val().BoundingBox()
        dims = (bb.xlen, bb.ylen, bb.zlen)
        print(f"      Dimensions: {dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f} mm")

        # Tessellate
        print(f"      Tessellating...")
        vertices, triangles = shape_to_triangles(shape)
        print(f"      {len(vertices)} vertices, {len(triangles)} triangles")

        # Create mesh object
        obj_elem = ET.SubElement(resources, f'{{{ns}}}object',
                                  id=obj_id, type="model")
        mesh = ET.SubElement(obj_elem, f'{{{ns}}}mesh')

        verts_elem = ET.SubElement(mesh, f'{{{ns}}}vertices')
        for vx, vy, vz in vertices:
            ET.SubElement(verts_elem, f'{{{ns}}}vertex',
                         x=f"{vx:.6f}", y=f"{vy:.6f}", z=f"{vz:.6f}")

        tris_elem = ET.SubElement(mesh, f'{{{ns}}}triangles')
        for v1, v2, v3 in triangles:
            ET.SubElement(tris_elem, f'{{{ns}}}triangle',
                         v1=str(v1), v2=str(v2), v3=str(v3))

        # Place objects side-by-side using transform
        # Offset each part so the bounding box min-x starts at cursor_x
        offset_x = cursor_x - bb.xmin
        offset_y = -bb.ymin  # align to Y=0
        offset_z = -bb.zmin  # sit on the build plate

        # 3MF uses a 3x4 affine transform matrix (row-major, no last row)
        # | 1 0 0 tx |
        # | 0 1 0 ty |
        # | 0 0 1 tz |
        transform = f"1 0 0 0 1 0 0 0 1 {offset_x:.4f} {offset_y:.4f} {offset_z:.4f}"
        ET.SubElement(build, f'{{{ns}}}item', objectid=obj_id, transform=transform)

        object_entries.append((obj_id, part_name, dims))
        cursor_x += bb.xlen + spacing
        print(f"      Plate position: X offset {offset_x:.1f} mm")

    # Check combined width fits build plate (270mm)
    total_width = cursor_x - spacing
    print(f"\n  Combined plate width: {total_width:.1f} mm (max 270)")
    if total_width > 270:
        print(f"  WARNING: Combined width exceeds 270mm build plate!")

    # Serialize XML
    model_xml = io.BytesIO()
    tree = ET.ElementTree(model)
    ET.indent(tree, space='  ')
    tree.write(model_xml, xml_declaration=True, encoding='UTF-8')
    model_xml.seek(0)

    # 3MF boilerplate
    content_types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
        '  <Default Extension="rels" ContentType='
        '"application/vnd.openxmlformats-package.relationships+xml"/>\n'
        '  <Default Extension="model" ContentType='
        '"application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
        '</Types>\n'
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
        '  <Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
        '</Relationships>\n'
    )

    # Embed ASA-GF settings
    settings_lines = [f"{k} = {v}" for k, v in ORCA_SETTINGS_ASAGF.items()]

    # Plate config — all objects on plate 1
    plate_lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<config>',
    ]
    for obj_id, part_name, _ in object_entries:
        plate_lines.extend([
            f'  <object id="{obj_id}">',
            f'    <metadata key="extruder" value="1"/>',
            f'    <metadata key="name" value="{part_name}"/>',
            f'  </object>',
        ])
    plate_lines.extend([
        '  <plate>',
        '    <metadata key="plater_id" value="1"/>',
        '    <metadata key="plater_name" value="Plate 1"/>',
    ])
    for obj_id, _, _ in object_entries:
        plate_lines.append(f'    <metadata key="object_id" value="{obj_id}"/>')
    plate_lines.extend(['  </plate>', '</config>'])

    # Write the 3MF ZIP
    filepath = os.path.join(STL_DIR, output_filename)
    with zipfile.ZipFile(filepath, 'w', zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('[Content_Types].xml', content_types)
        zf.writestr('_rels/.rels', rels)
        zf.writestr('3D/3dmodel.model', model_xml.getvalue())
        zf.writestr('Metadata/project_settings.config',
                     '\n'.join(settings_lines))
        zf.writestr('Metadata/plate_1.config',
                     '\n'.join(plate_lines))

    file_size_kb = os.path.getsize(filepath) / 1024
    print(f"\n  Output: {filepath}")
    print(f"  Size: {file_size_kb:.1f} KB")
    print(f"  Objects: {len(object_entries)}")
    for oid, name, dims in object_entries:
        print(f"    [{oid}] {name}: {dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f} mm")
    print(f"  Profile: ASA-GF Strength & Speed (no AMS)")
    return filepath


def main():
    print("=" * 60)
    print("STEP → 3MF Converter (ASA-GF embedded settings)")
    print("=" * 60)

    os.makedirs(STL_DIR, exist_ok=True)

    # Gather valid STEP files
    step_entries = []
    for step_file in STEP_FILES:
        step_path = os.path.join(DOWNLOADS, step_file)
        if not os.path.exists(step_path):
            print(f"\n  SKIP: {step_file} not found in {DOWNLOADS}")
            continue
        part_name = os.path.splitext(step_file)[0]
        step_entries.append((step_path, part_name))

    if not step_entries:
        print("\n  No STEP files found! Nothing to do.")
        return

    # Combined project — both parts on one plate
    result = steps_to_combined_3mf(
        step_entries,
        output_filename="XY_axis_models.3mf",
        title="X & Y Axis Models (ASA-GF)",
        spacing=15.0,
    )

    print(f"\n{'=' * 60}")
    print(f"Done! {len(step_entries)} parts combined into one 3MF project.")
    print(f"Settings: ASA-GF 270°C / 105°C bed / 60°C chamber")
    print(f"          6 walls, 40% gyroid, 10mm brim")
    print(f"          No AMS/multi-color features")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
