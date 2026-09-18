"""
Front-facing 3D ladder machine generator for Blender.

Usage:
  1. Open Blender.
  2. Scripting > Open this file > Run Script.
  3. Optional: set EXPORT_FBX = True below before running.

The model follows a casino arcade/vending-machine silhouette: upright red
cabinet, black glossy sides, gold trim, bulb marquee, front glass play window,
button deck, coin door, and a 3D ladder board viewed from the front.
"""

import math

import bpy


CLEAR_SCENE = True
CONVERT_CURVES_AND_TEXT_TO_MESH = True
EXPORT_FBX = False
FBX_PATH = r"C:\Users\user\Desktop\Casino\casino-simulator\Content\LJH\Map\SM_LadderMachine_3D.fbx"

RAILS = 5
ROWS = 12
LADDER_WIDTH = 2.65
LADDER_HEIGHT = 2.55
RUNG_RADIUS = 0.022
RAIL_RADIUS = 0.026
MARBLE_RADIUS = 0.08

FRONT_Y = -0.64
SCREEN_Y = -0.72
DISPLAY_TILT_DEGREES = -4.5

PREVIEW_START_RAIL = 1
PREVIEW_PATH_RAILS = [1, 2, 2, 3, 3, 2, 2, 1]
PREVIEW_CROSS_ROWS = [1, 3, 5, 7, 8, 10, 11]
PREVIEW_PAYOUTS = [0, 2, 0, 5, 3]


def make_mat(name, color, metallic=0.0, roughness=0.35, emission=None, emission_strength=0.0, alpha=1.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        rgba = (color[0], color[1], color[2], alpha)
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if "Alpha" in bsdf.inputs:
            bsdf.inputs["Alpha"].default_value = alpha
        if emission:
            bsdf.inputs["Emission Color"].default_value = emission
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    if alpha < 1.0:
        mat.blend_method = "BLEND"
        mat.use_screen_refraction = True
        mat.show_transparent_back = True
    return mat


MAT_BLACK = make_mat("M_Cabinet_Gloss_Black", (0.006, 0.005, 0.006), 0.0, 0.22)
MAT_RED = make_mat("M_Cabinet_Red_Enamel", (0.86, 0.02, 0.015), 0.12, 0.26)
MAT_GOLD = make_mat("M_Brushed_Gold_Trim", (1.0, 0.58, 0.12), 1.0, 0.2)
MAT_DARK_GLASS = make_mat("M_Smoked_Glass_Dark", (0.04, 0.015, 0.05), 0.0, 0.06, alpha=0.38)
MAT_WHITE_SCREEN = make_mat("M_Bright_Screen", (1.0, 0.96, 0.88), 0.0, 0.18, (1.0, 0.9, 0.72, 1), 1.8)
MAT_RAIL = make_mat("M_Chrome_Ladder_Rails", (0.78, 0.72, 0.62), 1.0, 0.16)
MAT_BLUE_NEON = make_mat("M_Blue_Neon", (0.04, 0.5, 1.0), 0.0, 0.12, (0.02, 0.5, 1.0, 1), 4.0)
MAT_RED_NEON = make_mat("M_Red_Neon", (1.0, 0.04, 0.02), 0.0, 0.16, (1.0, 0.03, 0.01, 1), 4.5)
MAT_GREEN_NEON = make_mat("M_Green_Button_Neon", (0.45, 1.0, 0.08), 0.0, 0.18, (0.45, 1.0, 0.06, 1), 3.2)
MAT_WARM_BULB = make_mat("M_Warm_Marquee_Bulb", (1.0, 0.78, 0.22), 0.0, 0.14, (1.0, 0.45, 0.08, 1), 4.0)
MAT_MARBLE = make_mat("M_Red_Marble", (1.0, 0.02, 0.01), 0.0, 0.18, (1.0, 0.02, 0.01, 1), 2.5)
MAT_PATH = make_mat("M_Marble_Path_Glow", (1.0, 0.06, 0.02), 0.0, 0.12, (1.0, 0.04, 0.01, 1), 3.5)
MAT_SEGMENT_ON = make_mat("M_SevenSegment_On", (1.0, 0.12, 0.02), 0.0, 0.14, (1.0, 0.08, 0.01, 1), 4.2)
MAT_SEGMENT_OFF = make_mat("M_SevenSegment_Off", (0.08, 0.015, 0.01), 0.0, 0.45, (0.12, 0.01, 0.0, 1), 0.15)


def assign(obj, mat):
    obj.data.materials.append(mat)
    return obj


def cube_obj(name, loc, scale, mat, bevel_width=0.035, bevel_segments=2):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, mat)
    if bevel_width > 0.0:
        bevel = obj.modifiers.new("soft bevels", "BEVEL")
        bevel.width = bevel_width
        bevel.segments = bevel_segments
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return obj


def cube_obj_rot(name, loc, scale, rotation, mat, bevel_width=0.035, bevel_segments=2):
    obj = cube_obj(name, loc, scale, mat, bevel_width, bevel_segments)
    obj.rotation_euler = rotation
    return obj


def sphere_obj(name, loc, radius, mat, segments=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=7, radius=radius, location=loc)
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    return obj


def cylinder_between(name, start, end, radius, mat, vertices=16):
    from mathutils import Vector

    start_vec = Vector(start)
    end_vec = Vector(end)
    mid = (start_vec + end_vec) * 0.5
    direction = end_vec - start_vec
    length = direction.length
    if length <= 0.0001:
        return None

    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=length, location=mid)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    assign(obj, mat)
    obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return obj


def curve_polyline(name, points, mat, bevel=0.035, cyclic=False):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 2
    curve.bevel_depth = bevel
    curve.bevel_resolution = 2
    spl = curve.splines.new("POLY")
    spl.points.add(len(points) - 1)
    for point, co in zip(spl.points, points):
        point.co = (co[0], co[1], co[2], 1)
    spl.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


def add_text(name, text, loc, size, mat, align="CENTER"):
    bpy.ops.object.text_add(location=loc, rotation=(math.radians(90), 0, 0))
    obj = bpy.context.object
    obj.name = name
    obj.data.body = text
    obj.data.align_x = align
    obj.data.align_y = "CENTER"
    obj.data.size = size
    obj.data.extrude = 0.018
    obj.data.bevel_depth = 0.003
    obj.data.resolution_u = 4
    obj.data.materials.append(mat)
    return obj


def rail_x(index):
    if RAILS <= 1:
        return 0.0
    return -LADDER_WIDTH * 0.5 + index * (LADDER_WIDTH / (RAILS - 1))


def row_z(row):
    if ROWS <= 1:
        return 2.8
    top = 4.18
    bottom = top - LADDER_HEIGHT
    return top - row * ((top - bottom) / (ROWS - 1))


def ladder_point(rail, row, y=SCREEN_Y):
    return (rail_x(rail), y, row_z(row))


SEVEN_SEGMENT_MAP = {
    0: {"A", "B", "C", "D", "E", "F"},
    1: {"B", "C"},
    2: {"A", "B", "G", "E", "D"},
    3: {"A", "B", "C", "D", "G"},
    4: {"F", "G", "B", "C"},
    5: {"A", "F", "G", "C", "D"},
    6: {"A", "F", "E", "D", "C", "G"},
    7: {"A", "B", "C"},
    8: {"A", "B", "C", "D", "E", "F", "G"},
    9: {"A", "B", "C", "D", "F", "G"},
}


def add_seven_segment_digit(prefix, digit, center, scale=1.0):
    x, y, z = center
    lit_segments = SEVEN_SEGMENT_MAP.get(digit, set())
    segment_defs = {
        "A": ((x, y, z + 0.19 * scale), (0.26 * scale, 0.035, 0.045 * scale)),
        "G": ((x, y, z), (0.26 * scale, 0.035, 0.045 * scale)),
        "D": ((x, y, z - 0.19 * scale), (0.26 * scale, 0.035, 0.045 * scale)),
        "F": ((x - 0.15 * scale, y, z + 0.095 * scale), (0.045 * scale, 0.035, 0.17 * scale)),
        "B": ((x + 0.15 * scale, y, z + 0.095 * scale), (0.045 * scale, 0.035, 0.17 * scale)),
        "E": ((x - 0.15 * scale, y, z - 0.095 * scale), (0.045 * scale, 0.035, 0.17 * scale)),
        "C": ((x + 0.15 * scale, y, z - 0.095 * scale), (0.045 * scale, 0.035, 0.17 * scale)),
    }

    for segment, (loc, size) in segment_defs.items():
        mat = MAT_SEGMENT_ON if segment in lit_segments else MAT_SEGMENT_OFF
        cube_obj(f"{prefix}_Seg_{segment}", loc, size, mat, 0.008, 1)


def add_bulb_frame(prefix, x0, x1, z0, z1, y=FRONT_Y - 0.07):
    count_top = 24
    count_side = 13
    for i in range(count_top):
        t = i / (count_top - 1)
        x = x0 + (x1 - x0) * t
        sphere_obj(f"{prefix}_Top_Bulb_{i:02d}", (x, y, z1), 0.055, MAT_WARM_BULB)
        sphere_obj(f"{prefix}_Bottom_Bulb_{i:02d}", (x, y, z0), 0.055, MAT_WARM_BULB)
    for i in range(1, count_side - 1):
        t = i / (count_side - 1)
        z = z0 + (z1 - z0) * t
        sphere_obj(f"{prefix}_Left_Bulb_{i:02d}", (x0, y, z), 0.055, MAT_WARM_BULB)
        sphere_obj(f"{prefix}_Right_Bulb_{i:02d}", (x1, y, z), 0.055, MAT_WARM_BULB)


def add_cabinet():
    display_rot = (math.radians(DISPLAY_TILT_DEGREES), 0, 0)

    cube_obj("Back_Black_Cabinet_Box", (0, 0.03, 2.65), (4.72, 0.86, 5.5), MAT_BLACK, 0.08, 3)
    cube_obj("Lower_Red_Cabinet_Front", (0, FRONT_Y - 0.02, 0.95), (4.35, 0.28, 1.46), MAT_RED, 0.07, 3)
    cube_obj("Upper_Display_Housing", (0, FRONT_Y - 0.02, 3.38), (4.45, 0.32, 3.0), MAT_BLACK, 0.08, 3)

    cube_obj("Left_Gold_Side_Trim", (-2.34, FRONT_Y - 0.09, 2.65), (0.13, 0.16, 5.22), MAT_GOLD, 0.025, 1)
    cube_obj("Right_Gold_Side_Trim", (2.34, FRONT_Y - 0.09, 2.65), (0.13, 0.16, 5.22), MAT_GOLD, 0.025, 1)
    cube_obj("Top_Black_Cap", (0, FRONT_Y - 0.04, 5.48), (4.75, 0.42, 0.34), MAT_BLACK, 0.07, 3)
    cube_obj("Bottom_Black_Platform", (0, FRONT_Y - 0.03, 0.06), (4.86, 0.52, 0.22), MAT_BLACK, 0.05, 2)
    cube_obj("Bottom_Gold_Toe_Trim", (0, FRONT_Y - 0.31, 0.2), (4.65, 0.08, 0.08), MAT_GOLD, 0.02, 1)

    cube_obj_rot("Gold_Display_Outer_Frame", (0, FRONT_Y - 0.16, 3.48), (3.86, 0.18, 2.78), display_rot, MAT_GOLD, 0.045, 2)
    cube_obj_rot("Dark_Display_Recess", (0, FRONT_Y - 0.25, 3.48), (3.54, 0.12, 2.42), display_rot, MAT_BLACK, 0.035, 2)
    cube_obj_rot("Smoked_Glass_Play_Window", (0, FRONT_Y - 0.33, 3.2), (2.7, 0.045, 1.92), display_rot, MAT_DARK_GLASS, 0.025, 1)
    cube_obj_rot("Bright_Back_Screen", (0, FRONT_Y - 0.38, 3.22), (2.38, 0.035, 1.54), display_rot, MAT_WHITE_SCREEN, 0.015, 1)

    add_bulb_frame("Main_Display", -1.92, 1.92, 2.15, 4.83)
    curve_polyline(
        "Red_Neon_Display_Border",
        [(-2.03, FRONT_Y - 0.42, 2.03), (2.03, FRONT_Y - 0.42, 2.03),
         (2.03, FRONT_Y - 0.42, 4.94), (-2.03, FRONT_Y - 0.42, 4.94)],
        MAT_RED_NEON,
        0.022,
        True,
    )
    add_text("Title_Ladder_Machine", "LADDER MACHINE", (0, FRONT_Y - 0.43, 4.93), 0.17, MAT_WARM_BULB)


def add_ladder_board():
    for r in range(RAILS):
        cylinder_between(f"Rail_{r}", ladder_point(r, ROWS - 1), ladder_point(r, 0), RAIL_RADIUS, MAT_RAIL, 18)
        sphere_obj(f"Rail_Top_Cap_{r}", ladder_point(r, 0), RAIL_RADIUS * 1.65, MAT_GOLD)
        sphere_obj(f"Rail_Bottom_Cap_{r}", ladder_point(r, ROWS - 1), RAIL_RADIUS * 1.65, MAT_GOLD)

    rung_cells = set()
    real_cells = set()
    for i, cross_row in enumerate(PREVIEW_CROSS_ROWS):
        left = min(PREVIEW_PATH_RAILS[i], PREVIEW_PATH_RAILS[i + 1])
        real_cells.add((cross_row, left))
        rung_cells.add((cross_row, left))
    for cell in [(2, 0), (2, 3), (4, 1), (6, 0), (6, 3), (9, 2)]:
        if cell[1] < RAILS - 1:
            rung_cells.add(cell)

    for row, gap in sorted(rung_cells):
        if 0 <= row < ROWS and 0 <= gap < RAILS - 1:
            mat = MAT_RAIL if (row, gap) in real_cells else MAT_BLUE_NEON
            cylinder_between(f"Rung_R{row:02d}_G{gap:02d}", ladder_point(gap, row), ladder_point(gap + 1, row), RUNG_RADIUS, mat, 14)

    start = ladder_point(PREVIEW_START_RAIL, 0, SCREEN_Y - 0.08)
    bpy.ops.mesh.primitive_cone_add(vertices=3, radius1=0.14, radius2=0.0, depth=0.22, location=(start[0], start[1], start[2] + 0.28), rotation=(math.radians(90), 0, math.radians(30)))
    arrow = bpy.context.object
    arrow.name = "Start_Rail_Selector_Arrow"
    assign(arrow, MAT_RED_NEON)


def add_preview_marble_path():
    points = [ladder_point(PREVIEW_PATH_RAILS[0], 0, SCREEN_Y - 0.08)]
    for i, cross_row in enumerate(PREVIEW_CROSS_ROWS):
        points.append(ladder_point(PREVIEW_PATH_RAILS[i], cross_row, SCREEN_Y - 0.08))
        points.append(ladder_point(PREVIEW_PATH_RAILS[i + 1], cross_row, SCREEN_Y - 0.08))
    points.append(ladder_point(PREVIEW_PATH_RAILS[-1], ROWS - 1, SCREEN_Y - 0.08))

    curve_polyline("Preview_Marble_Path_Glow", points, MAT_PATH, 0.018, False)
    for i, idx in enumerate([0, max(1, len(points) // 3), max(2, len(points) * 2 // 3), len(points) - 1]):
        sphere_obj(f"Preview_Marble_{i}", points[idx], MARBLE_RADIUS, MAT_MARBLE, 18)


def add_payout_panel():
    y = FRONT_Y - 0.38
    z = 1.96
    cube_obj("Payout_SevenSeg_Gold_Rail", (0, y + 0.03, z), (3.1, 0.08, 0.16), MAT_GOLD, 0.025, 1)
    cube_obj("Payout_SevenSeg_Backplate", (0, y - 0.02, z - 0.28), (3.2, 0.055, 0.62), MAT_BLACK, 0.02, 1)
    for rail in range(RAILS):
        x = rail_x(rail)
        payout = PREVIEW_PAYOUTS[rail] if rail < len(PREVIEW_PAYOUTS) else 0
        cube_obj(f"Payout_SevenSeg_Cell_{rail}", (x, y - 0.055, z - 0.28), (0.46, 0.035, 0.54), MAT_SEGMENT_OFF, 0.012, 1)
        add_seven_segment_digit(f"Payout_Rail_{rail}_Digit", payout, (x, y - 0.09, z - 0.28), 0.78)


def add_control_panel():
    cube_obj("Button_Deck_Black", (0, FRONT_Y - 0.3, 1.58), (4.0, 0.42, 0.34), MAT_BLACK, 0.055, 2)
    cube_obj("Button_Deck_Gold_Lip", (0, FRONT_Y - 0.54, 1.73), (3.8, 0.08, 0.08), MAT_GOLD, 0.018, 1)

    button_specs = [
        ("Bet_Button_0", -1.35, MAT_GREEN_NEON, "BET"),
        ("Rail_Left_Button", -0.45, MAT_WARM_BULB, "<"),
        ("Rail_Right_Button", 0.35, MAT_WARM_BULB, ">"),
        ("Play_Button", 1.28, MAT_GREEN_NEON, "PLAY"),
    ]
    for name, x, mat, label in button_specs:
        sphere_obj(name, (x, FRONT_Y - 0.58, 1.78), 0.14 if label != "PLAY" else 0.18, mat, 18)
        add_text(f"{name}_Label", label, (x, FRONT_Y - 0.72, 1.46), 0.12, MAT_GOLD)

    cube_obj("Coin_Door_Gold_Frame", (1.62, FRONT_Y - 0.24, 0.82), (0.68, 0.06, 0.92), MAT_GOLD, 0.025, 1)
    cube_obj("Coin_Door_Black", (1.62, FRONT_Y - 0.3, 0.82), (0.54, 0.05, 0.76), MAT_BLACK, 0.03, 2)
    cube_obj("Coin_Slot", (1.62, FRONT_Y - 0.34, 0.98), (0.26, 0.035, 0.055), MAT_RED_NEON, 0.01, 1)


def add_lights_and_camera():
    for i, loc in enumerate([(-2.3, -3.0, 4.4), (2.3, -3.0, 4.4), (0, -3.2, 2.2)]):
        bpy.ops.object.light_add(type="POINT", location=loc)
        lamp = bpy.context.object
        lamp.name = f"Preview_Light_{i}"
        lamp.data.energy = 180 if i < 2 else 120
        lamp.data.color = (1.0, 0.58, 0.2)

    bpy.ops.object.camera_add(location=(0, -7.2, 2.95), rotation=(math.radians(72), 0, 0))
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
            eevee.bloom_intensity = 0.12
        if hasattr(eevee, "bloom_radius"):
            eevee.bloom_radius = 6.0


def finalize_asset():
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0, 0, 0))
    root = bpy.context.object
    root.name = "SM_LadderMachine_3D_Root"

    for obj in bpy.context.scene.objects:
        if obj != root and obj.type not in {"CAMERA", "LIGHT"}:
            obj.parent = root

    if CONVERT_CURVES_AND_TEXT_TO_MESH:
        bpy.ops.object.select_all(action="DESELECT")
        active = None
        for obj in bpy.context.scene.objects:
            if obj.type in {"CURVE", "FONT"}:
                obj.select_set(True)
                active = active or obj
        if active:
            bpy.context.view_layer.objects.active = active
            bpy.ops.object.convert(target="MESH")

    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH":
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            for mod in list(obj.modifiers):
                try:
                    bpy.ops.object.modifier_apply(modifier=mod.name)
                except RuntimeError:
                    pass

    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0

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


def build_ladder_machine():
    if CLEAR_SCENE:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()

    add_cabinet()
    add_ladder_board()
    add_preview_marble_path()
    add_payout_panel()
    add_control_panel()
    add_lights_and_camera()
    finalize_asset()

    print("Created front-facing 3D ladder machine asset.")
    print("Set EXPORT_FBX = True to export an Unreal-friendly FBX.")


if __name__ == "__main__":
    build_ladder_machine()
