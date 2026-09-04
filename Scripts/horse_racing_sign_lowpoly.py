"""
Low/mid-poly Horse Racing neon sign generator for Blender.

Usage:
  1. Open Blender.
  2. Scripting > Open this file > Run Script.
  3. Optional: set EXPORT_FBX = True below before running.

The model is intentionally stylized for game use: repeated bulbs, simple
extruded panels, bevelled text, neon curves, and simplified horse icons.
"""

import math

import bpy


# -----------------------------
# Artist/game-asset controls
# -----------------------------

CLEAR_SCENE = True
CONVERT_CURVES_AND_TEXT_TO_MESH = True
EXPORT_FBX = False
FBX_PATH = r"C:\Users\user1\Documents\Unreal Projects\Casino_Simulator\Content\LJH\Map\horse_racing_sign_lowpoly.fbx"

SIGN_WIDTH = 12.0
SIGN_HEIGHT = 2.2
SIGN_DEPTH = 0.22
BULB_COUNT_TOP = 28
BULB_COUNT_SIDE = 8
BULB_SEGMENTS = 12
TEXT_EXTRUDE = 0.045
TEXT_BEVEL = 0.012
NEON_BEVEL = 0.025


# -----------------------------
# Helpers
# -----------------------------

def make_mat(name, color, metallic=0.0, roughness=0.35, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if emission:
            bsdf.inputs["Emission Color"].default_value = emission
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


MAT_GOLD = make_mat("M_Gold_Bevel", (1.0, 0.54, 0.03, 1), 1.0, 0.22)
MAT_DARK = make_mat("M_Dark_Backplate", (0.012, 0.01, 0.008, 1), 0.0, 0.55)
MAT_RED = make_mat("M_Red_Enamel", (0.75, 0.035, 0.0, 1), 0.2, 0.28)
MAT_RED_NEON = make_mat("M_Red_Neon", (1.0, 0.05, 0.0, 1), 0.0, 0.18, (1.0, 0.04, 0.0, 1), 5.5)
MAT_BLUE_NEON = make_mat("M_Blue_Neon", (0.0, 0.55, 1.0, 1), 0.0, 0.12, (0.0, 0.6, 1.0, 1), 4.0)
MAT_WARM_LIGHT = make_mat("M_Warm_Bulb", (1.0, 0.78, 0.22, 1), 0.0, 0.18, (1.0, 0.62, 0.12, 1), 3.4)
MAT_WHITE_HOT = make_mat("M_White_Hot_Core", (1.0, 0.94, 0.62, 1), 0.0, 0.1, (1.0, 0.86, 0.38, 1), 4.8)


def assign(obj, mat):
    obj.data.materials.append(mat)
    return obj


def cube_obj(name, loc, scale, mat):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, mat)
    bevel = obj.modifiers.new("small bevels", "BEVEL")
    bevel.width = 0.04
    bevel.segments = 2
    obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return obj


def cylinder_obj(name, loc, radius, depth, mat, vertices=32, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    bevel = obj.modifiers.new("rim bevel", "BEVEL")
    bevel.width = 0.035
    bevel.segments = 2
    obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return obj


def sphere_obj(name, loc, radius, mat, segments=BULB_SEGMENTS):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=6, radius=radius, location=loc)
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    return obj


def curve_polyline(name, points, mat, bevel=NEON_BEVEL, cyclic=False, z=0.2):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 2
    curve.bevel_depth = bevel
    curve.bevel_resolution = 2
    spl = curve.splines.new("POLY")
    spl.points.add(len(points) - 1)
    for p, co in zip(spl.points, points):
        p.co = (co[0], co[1], z, 1)
    spl.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


def add_text(name, text, loc, size, mat):
    bpy.ops.object.text_add(location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.data.body = text
    obj.data.align_x = "CENTER"
    obj.data.align_y = "CENTER"
    obj.data.size = size
    obj.data.extrude = TEXT_EXTRUDE
    obj.data.bevel_depth = TEXT_BEVEL
    obj.data.bevel_resolution = 1
    obj.data.resolution_u = 6
    obj.data.materials.append(mat)
    return obj


def add_starburst(prefix, y_center, z, flip=1):
    # Low-poly fan rays using triangular meshes.
    rays = 17
    for i in range(rays):
        t0 = math.radians(180 - (i + 0.5) * 180 / rays)
        length = 1.05 + 0.55 * (i % 2)
        width = 0.10
        base_x = math.cos(t0) * 0.25
        tip_x = math.cos(t0) * length
        tip_y = y_center + flip * abs(math.sin(t0)) * 0.72
        base_y = y_center + flip * 0.10
        verts = [
            (base_x - width, base_y, z),
            (base_x + width, base_y, z),
            (tip_x, tip_y, z),
        ]
        mesh = bpy.data.meshes.new(f"{prefix}_ray_mesh")
        mesh.from_pydata(verts, [], [(0, 1, 2)])
        mesh.update()
        obj = bpy.data.objects.new(f"{prefix}_ray_{i:02d}", mesh)
        bpy.context.collection.objects.link(obj)
        obj.data.materials.append(MAT_GOLD)
        solid = obj.modifiers.new("ray thickness", "SOLIDIFY")
        solid.thickness = 0.045
        bevel = obj.modifiers.new("ray bevel", "BEVEL")
        bevel.width = 0.018
        bevel.segments = 1


def add_horse_icon(x_offset):
    # A readable neon shorthand rather than a dense traced horse mesh.
    pts_body = [
        (x_offset - 0.35, -0.05), (x_offset - 0.15, 0.15), (x_offset + 0.18, 0.17),
        (x_offset + 0.38, 0.02), (x_offset + 0.24, -0.12), (x_offset - 0.15, -0.12),
        (x_offset - 0.35, -0.05),
    ]
    curve_polyline("Horse_Neon_Body_L" if x_offset < 0 else "Horse_Neon_Body_R", pts_body, MAT_RED_NEON, 0.018, False, 0.34)
    curve_polyline("Horse_Neon_Neck", [(x_offset + 0.24, 0.12), (x_offset + 0.42, 0.28), (x_offset + 0.58, 0.22)], MAT_RED_NEON, 0.017, False, 0.34)
    curve_polyline("Horse_Neon_Legs_A", [(x_offset - 0.18, -0.12), (x_offset - 0.28, -0.43), (x_offset - 0.18, -0.47)], MAT_RED_NEON, 0.016, False, 0.34)
    curve_polyline("Horse_Neon_Legs_B", [(x_offset + 0.05, -0.12), (x_offset + 0.14, -0.42), (x_offset + 0.32, -0.43)], MAT_RED_NEON, 0.016, False, 0.34)
    curve_polyline("Horse_Neon_Tail", [(x_offset - 0.37, 0.0), (x_offset - 0.62, 0.12), (x_offset - 0.70, -0.02)], MAT_RED_NEON, 0.017, False, 0.34)


def add_pointing_hand():
    # Stylized marquee pointer: rounded palm/finger blocks plus neon outline.
    base_x, base_y = 4.0, 2.15
    cube_obj("Pointer_Palm_Red", (base_x + 0.9, base_y, 0.18), (1.15, 0.55, 0.18), MAT_RED)
    cube_obj("Pointer_Finger_Red", (base_x + 0.05, base_y - 0.35, 0.18), (1.15, 0.28, 0.16), MAT_RED)
    cube_obj("Pointer_Cuff_Gold", (base_x + 1.65, base_y + 0.02, 0.19), (0.42, 0.86, 0.24), MAT_GOLD)
    curve_polyline(
        "Pointer_Red_Neon_Outline",
        [(base_x - 0.55, base_y - 0.52), (base_x + 0.18, base_y - 0.75), (base_x + 0.55, base_y - 0.20),
         (base_x + 1.45, base_y - 0.28), (base_x + 1.62, base_y + 0.28), (base_x + 0.95, base_y + 0.42),
         (base_x + 0.35, base_y + 0.28), (base_x - 0.55, base_y - 0.52)],
        MAT_RED_NEON,
        0.035,
        False,
        0.36,
    )
    for i in range(5):
        sphere_obj(f"Pointer_Cuff_Bulb_{i}", (base_x + 1.68, base_y - 0.32 + i * 0.16, 0.39), 0.075, MAT_WARM_LIGHT)
    for i, dy in enumerate([-0.18, 0.04, 0.26]):
        curve_polyline(f"Pointer_Action_Line_{i}", [(base_x - 0.8, base_y + dy), (base_x - 1.25, base_y + dy - 0.08)], MAT_RED_NEON, 0.022, False, 0.34)


def build_sign():
    if CLEAR_SCENE:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()

    # Main layered sign body.
    cube_obj("Main_Dark_Backplate", (0, 0, 0), (SIGN_WIDTH, SIGN_HEIGHT, SIGN_DEPTH), MAT_DARK)
    cube_obj("Outer_Red_Marquee", (0, 0, -0.03), (SIGN_WIDTH + 0.75, SIGN_HEIGHT + 0.52, 0.16), MAT_RED)
    cube_obj("Inner_Dark_Face", (0, 0, 0.14), (SIGN_WIDTH - 0.6, SIGN_HEIGHT - 0.52, 0.12), MAT_DARK)
    cube_obj("Gold_Top_Rail", (0, SIGN_HEIGHT * 0.62, 0.18), (SIGN_WIDTH + 0.9, 0.18, 0.24), MAT_GOLD)
    cube_obj("Gold_Bottom_Rail", (0, -SIGN_HEIGHT * 0.62, 0.18), (SIGN_WIDTH + 0.9, 0.18, 0.24), MAT_GOLD)
    cube_obj("Gold_Left_End_Cap", (-SIGN_WIDTH * 0.53, 0, 0.18), (0.42, SIGN_HEIGHT + 0.56, 0.24), MAT_GOLD)
    cube_obj("Gold_Right_End_Cap", (SIGN_WIDTH * 0.53, 0, 0.18), (0.42, SIGN_HEIGHT + 0.56, 0.24), MAT_GOLD)

    # Neon trims.
    curve_polyline("Outer_Red_Neon", [(-6.2, -1.22), (6.2, -1.22), (6.2, 1.22), (-6.2, 1.22)], MAT_RED_NEON, 0.035, True, 0.34)
    curve_polyline("Inner_Blue_Neon", [(-5.65, -0.87), (5.65, -0.87), (5.65, 0.87), (-5.65, 0.87)], MAT_BLUE_NEON, 0.020, True, 0.35)
    for y in [-0.55, -0.25, 0.25, 0.55]:
        curve_polyline(f"Horizontal_Gold_Pinstripe_{y}", [(-5.95, y), (5.95, y)], MAT_GOLD, 0.012, False, 0.26)

    # Center title.
    add_text("Title_HORSE_RACING_Gold", "HORSE RACING", (0, -0.03, 0.36), 1.08, MAT_WARM_LIGHT)
    add_text("Title_HORSE_RACING_Red_Backglow", "HORSE RACING", (0, -0.05, 0.30), 1.16, MAT_RED_NEON)

    # Bulbs along rails and end caps.
    for i in range(BULB_COUNT_TOP):
        x = -5.45 + i * (10.9 / (BULB_COUNT_TOP - 1))
        sphere_obj(f"Top_Bulb_{i:02d}", (x, 1.33, 0.38), 0.09, MAT_WARM_LIGHT)
        sphere_obj(f"Bottom_Bulb_{i:02d}", (x, -1.33, 0.38), 0.09, MAT_WARM_LIGHT)
    for side, x in [("Left", -6.35), ("Right", 6.35)]:
        for i in range(BULB_COUNT_SIDE):
            y = -0.84 + i * (1.68 / (BULB_COUNT_SIDE - 1))
            sphere_obj(f"{side}_Cap_Bulb_{i:02d}", (x, y, 0.39), 0.08, MAT_WARM_LIGHT)

    # Circular horse medallions.
    for x in [-5.95, 5.95]:
        cylinder_obj("Horse_Medallion_Gold_L" if x < 0 else "Horse_Medallion_Gold_R", (x, 0, 0.24), 0.76, 0.16, MAT_GOLD, 36, (math.radians(90), 0, 0))
        cylinder_obj("Horse_Medallion_Dark_L" if x < 0 else "Horse_Medallion_Dark_R", (x, 0, 0.34), 0.58, 0.06, MAT_DARK, 28, (math.radians(90), 0, 0))
        add_horse_icon(x)

    # Center ornaments.
    add_starburst("Top_Starburst", 1.12, 0.18, 1)
    add_starburst("Bottom_Starburst", -1.12, 0.18, -1)
    cylinder_obj("Center_Gold_Diamond", (0, 1.55, 0.25), 0.34, 0.11, MAT_GOLD, 4, (math.radians(90), 0, math.radians(45)))
    cylinder_obj("Bottom_Gold_Diamond", (0, -1.55, 0.25), 0.34, 0.11, MAT_GOLD, 4, (math.radians(90), 0, math.radians(45)))

    add_pointing_hand()

    # Add a few small point lights for viewport preview without making the asset heavy.
    for i, x in enumerate([-4, 0, 4]):
        bpy.ops.object.light_add(type="POINT", location=(x, -0.4, 2.5))
        lamp = bpy.context.object
        lamp.name = f"Preview_Warm_Point_{i}"
        lamp.data.energy = 120
        lamp.data.color = (1.0, 0.55, 0.18)

    # Camera and Eevee bloom preview settings.
    bpy.ops.object.camera_add(location=(0, -8.5, 3.2), rotation=(math.radians(68), 0, 0))
    bpy.context.scene.camera = bpy.context.object
    try:
        bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        bpy.context.scene.render.engine = "BLENDER_EEVEE"
    if hasattr(bpy.context.scene, "eevee"):
        eevee = bpy.context.scene.eevee
        if hasattr(eevee, "use_bloom"):
            eevee.use_bloom = True
        if hasattr(eevee, "bloom_intensity"):
            eevee.bloom_intensity = 0.08
        if hasattr(eevee, "bloom_radius"):
            eevee.bloom_radius = 6.0

    # Organize for Unreal export.
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0, 0, 0))
    root = bpy.context.object
    root.name = "SM_HorseRacing_NeonSign_Root"
    for obj in bpy.context.scene.objects:
        if obj != root and obj.type not in {"CAMERA", "LIGHT"}:
            obj.parent = root

    if CONVERT_CURVES_AND_TEXT_TO_MESH:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in bpy.context.scene.objects:
            if obj.type in {"CURVE", "FONT"}:
                obj.select_set(True)
        bpy.context.view_layer.objects.active = next((o for o in bpy.context.scene.objects if o.select_get()), None)
        if bpy.context.view_layer.objects.active:
            bpy.ops.object.convert(target="MESH")

    # Apply visible modifiers for predictable FBX geometry.
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH":
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)
            for mod in list(obj.modifiers):
                try:
                    bpy.ops.object.modifier_apply(modifier=mod.name)
                except RuntimeError:
                    pass
            obj.select_set(False)

    # Set origin and scale-friendly units.
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    root.rotation_euler[0] = math.radians(90)

    if EXPORT_FBX:
        bpy.ops.object.select_all(action="DESELECT")
        root.select_set(True)
        for child in root.children:
            child.select_set(True)
        bpy.context.view_layer.objects.active = root
        bpy.ops.export_scene.fbx(
            filepath=FBX_PATH,
            use_selection=True,
            apply_scale_options="FBX_SCALE_ALL",
            axis_forward="-Y",
            axis_up="Z",
            object_types={"EMPTY", "MESH"},
            bake_space_transform=False,
        )

    print("Created low/mid-poly Horse Racing neon sign asset.")
    print("Set EXPORT_FBX = True to export an Unreal-friendly FBX.")


if __name__ == "__main__":
    build_sign()
