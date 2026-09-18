"""Ladder machine based on the user's side-profile sketch. Blender 5.2."""

import bpy
from mathutils import Vector


W = 5.0
Y0, Z0 = -1.55, 1.65       # low/front end of sloped playfield
Y1, Z1 = 1.35, 5.25        # high/rear end
DY, DZ = Y1 - Y0, Z1 - Z0
SLOPE_LEN = (DY * DY + DZ * DZ) ** 0.5
NY, NZ = -DZ / SLOPE_LEN, DY / SLOPE_LEN  # outward normal toward camera


def mat(name, color, metallic=0.0, roughness=0.3, emission=None, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, alpha)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if bsdf.inputs.get("Alpha"):
        bsdf.inputs["Alpha"].default_value = alpha
    if emission:
        socket = bsdf.inputs.get("Emission Color") or bsdf.inputs.get("Emission")
        if socket:
            socket.default_value = (*emission, 1.0)
        if bsdf.inputs.get("Emission Strength"):
            bsdf.inputs["Emission Strength"].default_value = 5.0
    if alpha < 1.0:
        if hasattr(m, "surface_render_method"):
            m.surface_render_method = "DITHERED"
        else:
            m.blend_method = "BLEND"
    return m


RED = mat("Deep red enamel", (0.62, 0.008, 0.018), 0.1, 0.2)
BLACK = mat("Gloss black", (0.006, 0.007, 0.009), 0.0, 0.17)
GOLD = mat("Polished gold", (0.9, 0.42, 0.045), 0.9, 0.16)
GLASS = mat("Smoked glass", (0.05, 0.08, 0.10), 0.0, 0.08, alpha=0.16)
AMBER = mat("Amber segment", (1.0, 0.48, 0.03), 0.0, 0.16, (1.0, 0.18, 0.005))
OFF = mat("Unlit segment", (0.06, 0.018, 0.005), 0.0, 0.4)
FLOOR = mat("Studio floor", (0.13, 0.14, 0.17), 0.0, 0.75)


def clear():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def mesh_obj(name, verts, faces, material, bevel=0.0):
    mesh = bpy.data.meshes.new(name + "Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    if bevel:
        mod = obj.modifiers.new("Rounded edges", "BEVEL")
        mod.width = bevel
        mod.segments = 3
    return obj


def box(name, loc, size, material, bevel=0.04):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    if bevel:
        mod = obj.modifiers.new("Rounded edges", "BEVEL")
        mod.width = bevel
        mod.segments = 3
    return obj


def cyl(name, a, b, radius, material, vertices=16):
    a, b = Vector(a), Vector(b)
    d = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=d.length, location=(a + b) / 2)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    obj.data.materials.append(material)
    return obj


def surf(x, t, offset=0.0):
    return (x, Y0 + DY * t + NY * offset, Z0 + DZ * t + NZ * offset)


def sphere(name, p, radius, material):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=12,
                                        radius=radius, location=p)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def build_body():
    # One closed cabinet copied from the side-view sketch. The five-point
    # profile is extruded across X, so there is no floating triangular panel.
    x = W / 2
    profile = [
        (-2.62, 0.16),  # front-bottom
        (-2.62, 1.56),  # low vertical front
        (Y0, Z0),       # short control-deck ledge, then slope begins
        (Y1, Z1),       # top of the long sloped playfield
        (1.58, Z1),     # short flat top and vertical rear
        (1.58, 0.16),   # rear-bottom
    ]
    verts = [(-x, y, z) for y, z in profile] + [(x, y, z) for y, z in profile]
    count = len(profile)
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    for i in range(count):
        j = (i + 1) % count
        faces.append((i, j, count + j, count + i))
    mesh_obj("Single closed side-profile cabinet", verts, faces, RED, 0.07)

    box("Black bottom plinth", (0, -0.52, 0.10), (W + 0.18, 4.34, 0.20), BLACK, 0.05)
    box("Gold toe trim", (0, -2.64, 0.24), (W - 0.18, 0.08, 0.09), GOLD, 0.02)

    # Black inset playfield and gold perimeter on the actual slope.
    t0, t1, half = 0.12, 0.91, 2.15
    mesh_obj("Black sloped playfield",
             [surf(-half, t0, 0.06), surf(half, t0, 0.06),
              surf(half, t1, 0.06), surf(-half, t1, 0.06)],
             [(0, 1, 2, 3)], BLACK)
    corners = [surf(-half, t0, 0.10), surf(half, t0, 0.10),
               surf(half, t1, 0.10), surf(-half, t1, 0.10)]
    for i in range(4):
        cyl(f"Gold playfield border {i}", corners[i], corners[(i + 1) % 4],
            0.045, GOLD, 20)


def build_ladder():
    xs = [-1.55, -0.78, 0.0, 0.78, 1.55]
    for i, x in enumerate(xs):
        cyl(f"Vertical ladder rail {i}", surf(x, 0.34, 0.14),
            surf(x, 0.82, 0.14), 0.034, GOLD, 18)

    pattern = [(0, 0), (0, 3), (1, 1), (2, 0), (2, 2),
               (3, 1), (3, 3), (4, 0), (4, 2), (5, 1),
               (6, 0), (6, 3), (7, 2)]
    for row, gap in pattern:
        t = 0.38 + row * 0.055
        cyl(f"Ladder rung {row}-{gap}", surf(xs[gap], t, 0.15),
            surf(xs[gap + 1], t, 0.15), 0.028, GOLD, 16)

    # Small pinball pegs make the interior read through the glass.
    for row in range(7):
        t = 0.405 + row * 0.057
        for gap in range(4):
            sphere(f"Pin {row}-{gap}", surf((xs[gap] + xs[gap + 1]) / 2,
                                            t, 0.16), 0.047, GOLD)

    # Glass cover lies above every ladder element.
    mesh_obj("Visible pinball glass",
             [surf(-1.83, 0.29, 0.22), surf(1.83, 0.29, 0.22),
              surf(1.83, 0.86, 0.22), surf(-1.83, 0.86, 0.22)],
             [(0, 1, 2, 3)], GLASS)


DIGITS = {
    0: set("ABCDEF"), 1: set("BC"), 2: set("ABGED"),
    3: set("ABCDG"), 4: set("FBCG"), 5: set("AFGCD"),
}


def segment_digit(prefix, digit, cx, ct, scale=1.0):
    # Seven separate glowing bars laid directly on the slope.
    dx, dt = 0.15 * scale, 0.027 * scale
    defs = {
        "A": ((-dx, ct + 2 * dt), (dx, ct + 2 * dt)),
        "G": ((-dx, ct), (dx, ct)),
        "D": ((-dx, ct - 2 * dt), (dx, ct - 2 * dt)),
        "F": ((cx - dx, ct + 2 * dt), (cx - dx, ct)),
        "B": ((cx + dx, ct + 2 * dt), (cx + dx, ct)),
        "E": ((cx - dx, ct), (cx - dx, ct - 2 * dt)),
        "C": ((cx + dx, ct), (cx + dx, ct - 2 * dt)),
    }
    active = DIGITS[digit]
    for key, (a, b) in defs.items():
        if key in "AGD":
            p1, p2 = surf(cx + a[0], a[1], 0.18), surf(cx + b[0], b[1], 0.18)
        else:
            p1, p2 = surf(a[0], a[1], 0.18), surf(b[0], b[1], 0.18)
        cyl(f"{prefix} {key}", p1, p2, 0.025,
            AMBER if key in active else OFF, 10)


def multiply(prefix, x, t):
    cyl(prefix + " a", surf(x - 0.10, t - 0.025, 0.18),
        surf(x + 0.10, t + 0.025, 0.18), 0.022, AMBER, 10)
    cyl(prefix + " b", surf(x - 0.10, t + 0.025, 0.18),
        surf(x + 0.10, t - 0.025, 0.18), 0.022, AMBER, 10)


def sign_digit(prefix, digit, cx, cz, scale=1.0):
    dx, dz, y = 0.13 * scale, 0.16 * scale, 1.035
    defs = {
        "A": ((cx - dx, cz + dz), (cx + dx, cz + dz)),
        "G": ((cx - dx, cz), (cx + dx, cz)),
        "D": ((cx - dx, cz - dz), (cx + dx, cz - dz)),
        "F": ((cx - dx, cz + dz), (cx - dx, cz)),
        "B": ((cx + dx, cz + dz), (cx + dx, cz)),
        "E": ((cx - dx, cz), (cx - dx, cz - dz)),
        "C": ((cx + dx, cz), (cx + dx, cz - dz)),
    }
    active = DIGITS[digit]
    for key, (a, b) in defs.items():
        cyl(f"{prefix} {key}", (a[0], y, a[1]), (b[0], y, b[1]),
            0.025, AMBER if key in active else OFF, 10)


def build_displays_and_controls():
    values = [0, 2, 3, 5]
    centers = [-1.45, -0.48, 0.48, 1.45]
    for i, (x, value) in enumerate(zip(centers, values)):
        t, hw, ht = 0.215, 0.40, 0.065
        mesh_obj(f"Multiplier display {i}",
                 [surf(x - hw, t - ht, 0.14), surf(x + hw, t - ht, 0.14),
                  surf(x + hw, t + ht, 0.14), surf(x - hw, t + ht, 0.14)],
                 [(0, 1, 2, 3)], BLACK)
        multiply(f"Multiply {i}", x - 0.18, t)
        segment_digit(f"Digit {i}", value, x + 0.17, t, 0.82)

    # Four controls on the horizontal front deck, matching the sketch.
    box("Horizontal control deck", (0, -2.38, 1.46), (4.62, 0.70, 0.28), BLACK, 0.07)
    for i, x in enumerate(centers):
        box(f"Gold button base {i}", (x, -2.70, 1.58), (0.62, 0.36, 0.14), GOLD, 0.05)
        box(f"Black button cap {i}", (x, -2.76, 1.65), (0.46, 0.26, 0.10), BLACK, 0.04)


def build_sign():
    # Two visible posts and a rectangular top scoreboard.
    for x in (-1.55, 1.55):
        cyl("Sign post", (x, 1.34, 5.08), (x, 1.34, 5.82), 0.055, GOLD, 18)
    box("Top red sign case", (0, 1.34, 6.02), (3.95, 0.48, 0.82), RED, 0.08)
    box("Top black score display", (0, 1.07, 6.02), (3.48, 0.055, 0.49), BLACK, 0.035)
    for i in range(4):
        sign_digit(f"Top score {i}", 0, -0.50 + i * 0.34, 6.02, 0.75)


def studio():
    box("Floor", (0, 0, -0.12), (14, 14, 0.20), FLOOR, 0)
    for name, loc, energy, color, size in [
        ("Key light", (-4, -5, 8), 1100, (1.0, 0.78, 0.58), 4.0),
        ("Rim light", (4, 2, 6), 850, (1.0, 0.22, 0.05), 3.0),
    ]:
        bpy.ops.object.light_add(type="AREA", location=loc)
        light = bpy.context.object
        light.name = name
        light.data.energy = energy
        light.data.color = color
        light.data.shape = "DISK"
        light.data.size = size
        target = Vector((0, 0, 2.8))
        light.rotation_euler = (target - light.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.camera_add(location=(8.2, -11.5, 7.1))
    cam = bpy.context.object
    cam.name = "Sketch presentation camera"
    cam.rotation_euler = (Vector((0, -0.15, 3.0)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 58
    bpy.context.scene.camera = cam
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = "//ladder_machine_sketch_preview.png"
    scene.world.color = (0.025, 0.025, 0.035)


def build():
    clear()
    build_body()
    build_ladder()
    build_displays_and_controls()
    build_sign()
    studio()
    bpy.ops.object.select_all(action="DESELECT")
    print("Built sketch-style ladder machine. Press F12 to render.")


if __name__ == "__main__":
    build()
