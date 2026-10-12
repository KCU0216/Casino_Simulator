import bpy
import math
import random
from mathutils import Vector

# =========================================================
# OLD PEOPLE RACE TRACK
# Blender 5.2
#
# 유지:
# - 6레인 트랙
# - 레인선
# - 스타트 / 피니쉬
# - 입구 구조물
# - 피니쉬 구조물
# - 낮은 바리케이트
# - 바리케이트 받침
# - 파이프 / 누수
#
# 삭제:
# - 매표소
# - 자갈 / 쓰레기
# - OUTSIDE_GROUND
# =========================================================


# =========================================================
# 초기화
# =========================================================

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

random.seed(42)


# =========================================================
# 재질
# =========================================================

def make_mat(name, color, roughness=0.8, metallic=0.0):

    material = bpy.data.materials.new(name=name)

    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True

    bsdf = None

    for node in material.node_tree.nodes:
        if node.bl_idname == "ShaderNodeBsdfPrincipled":
            bsdf = node
            break

    if bsdf:

        if "Base Color" in bsdf.inputs:
            bsdf.inputs["Base Color"].default_value = (*color, 1.0)

        if "Roughness" in bsdf.inputs:
            bsdf.inputs["Roughness"].default_value = roughness

        if "Metallic" in bsdf.inputs:
            bsdf.inputs["Metallic"].default_value = metallic

    return material


MAT_DIRT = make_mat(
    "TRACK_DIRT",
    (0.12, 0.055, 0.025),
    1.0
)

MAT_DIRT_LIGHT = make_mat(
    "TRACK_DIRT_LIGHT",
    (0.20, 0.10, 0.045),
    1.0
)

MAT_WHITE = make_mat(
    "OLD_WHITE",
    (0.55, 0.50, 0.40),
    0.9
)

MAT_METAL = make_mat(
    "OLD_METAL",
    (0.07, 0.065, 0.055),
    0.85,
    0.35
)

MAT_RUST = make_mat(
    "RUST",
    (0.25, 0.045, 0.015),
    0.95,
    0.1
)

MAT_CONCRETE = make_mat(
    "CONCRETE",
    (0.13, 0.12, 0.105),
    1.0
)

MAT_WOOD = make_mat(
    "OLD_WOOD",
    (0.12, 0.055, 0.02),
    1.0
)

MAT_SIGN = make_mat(
    "OLD_SIGN",
    (0.20, 0.15, 0.08),
    0.95
)

MAT_BLACK = make_mat(
    "BLACK",
    (0.01, 0.008, 0.006),
    1.0
)

MAT_WATER = make_mat(
    "DIRTY_WATER",
    (0.015, 0.06, 0.07),
    0.2
)


# =========================================================
# 기본 Cube
# =========================================================

def cube(name, location, scale, material=None, bevel=0.0):

    bpy.ops.mesh.primitive_cube_add(
        location=location
    )

    obj = bpy.context.object
    obj.name = name
    obj.scale = scale

    bpy.ops.object.transform_apply(
        location=False,
        rotation=False,
        scale=True
    )

    if material:
        obj.data.materials.append(material)

    if bevel > 0:

        modifier = obj.modifiers.new(
            "BEVEL",
            "BEVEL"
        )

        modifier.width = bevel
        modifier.segments = 2

    return obj


# =========================================================
# Cylinder
# =========================================================

def cylinder(
    name,
    location,
    radius,
    depth,
    material=None,
    rotation=(0, 0, 0)
):

    bpy.ops.mesh.primitive_cylinder_add(
        vertices=16,
        radius=radius,
        depth=depth,
        location=location,
        rotation=rotation
    )

    obj = bpy.context.object
    obj.name = name

    if material:
        obj.data.materials.append(material)

    return obj


# =========================================================
# 경기장 크기
# =========================================================

TRACK_LENGTH = 60
TRACK_WIDTH = 18
TRACK_HEIGHT = 0.4

TRACK_START = -TRACK_LENGTH / 2
TRACK_END = TRACK_LENGTH / 2


# =========================================================
# 트랙
# =========================================================

# 트랙 상단이 Z = 0
cube(
    "RACE_TRACK",
    (
        0,
        0,
        -TRACK_HEIGHT / 2
    ),
    (
        TRACK_LENGTH / 2,
        TRACK_WIDTH / 2,
        TRACK_HEIGHT / 2
    ),
    MAT_DIRT
)


# =========================================================
# 6레인
# =========================================================

LANE_COUNT = 6
LANE_WIDTH = TRACK_WIDTH / LANE_COUNT

for i in range(LANE_COUNT + 1):

    y = -TRACK_WIDTH / 2 + i * LANE_WIDTH

    cube(
        f"LANE_LINE_{i}",
        (
            0,
            y,
            0.025
        ),
        (
            TRACK_LENGTH / 2,
            0.035,
            0.025
        ),
        MAT_WHITE
    )


# =========================================================
# 스타트 라인
# =========================================================

START_LINE_X = TRACK_START + 5

cube(
    "START_LINE",
    (
        START_LINE_X,
        0,
        0.05
    ),
    (
        0.12,
        TRACK_WIDTH / 2,
        0.035
    ),
    MAT_WHITE
)


# =========================================================
# 1~6번 스타트 번호판
# =========================================================

for i in range(LANE_COUNT):

    y = -TRACK_WIDTH / 2 + LANE_WIDTH * (i + 0.5)

    cube(
        f"LANE_NUMBER_{i+1}",
        (
            START_LINE_X - 1.5,
            y,
            0.075
        ),
        (
            0.8,
            0.55,
            0.04
        ),
        MAT_WHITE
    )


# =========================================================
# 피니쉬 라인
# =========================================================

FINISH_LINE_X = TRACK_END - 5

cube(
    "FINISH_LINE",
    (
        FINISH_LINE_X,
        0,
        0.05
    ),
    (
        0.12,
        TRACK_WIDTH / 2,
        0.035
    ),
    MAT_WHITE
)


# =========================================================
# 낮은 바리케이트
# =========================================================

BARRIER_HEIGHT = 1.0

# 트랙 바로 바깥
BARRIER_Y = TRACK_WIDTH / 2 + 0.8


def create_barrier(side):

    y = side * BARRIER_Y

    # -----------------------------------------------------
    # 세로 기둥
    # 바닥 = Z 0
    # -----------------------------------------------------

    for x in range(-30, 31, 3):

        cylinder(
            f"BARRIER_POST_{side}_{x}",
            (
                x,
                y,
                BARRIER_HEIGHT / 2
            ),
            0.065,
            BARRIER_HEIGHT,
            MAT_METAL
        )

        # -------------------------------------------------
        # 바닥 받침
        # -------------------------------------------------

        base_height = 0.10

        cube(
            f"BARRIER_BASE_{side}_{x}",
            (
                x,
                y,
                base_height / 2
            ),
            (
                0.28,
                0.22,
                base_height / 2
            ),
            MAT_CONCRETE,
            0.03
        )

    # -----------------------------------------------------
    # 상단 파이프
    # -----------------------------------------------------

    cylinder(
        f"BARRIER_TOP_{side}",
        (
            0,
            y,
            0.88
        ),
        0.075,
        TRACK_LENGTH,
        MAT_METAL,
        (
            0,
            math.radians(90),
            0
        )
    )

    # -----------------------------------------------------
    # 중간 파이프
    # -----------------------------------------------------

    cylinder(
        f"BARRIER_MIDDLE_{side}",
        (
            0,
            y,
            0.48
        ),
        0.055,
        TRACK_LENGTH,
        MAT_RUST,
        (
            0,
            math.radians(90),
            0
        )
    )


create_barrier(-1)
create_barrier(1)


# =========================================================
# 트랙 바깥쪽 발판 / 구조물
# =========================================================
# 큰 흙바닥은 만들지 않고,
# 경기장에 필요한 발판만 남김
# =========================================================

PLATFORM_Y = TRACK_WIDTH / 2 + 2.5

for side in [-1, 1]:

    y = side * PLATFORM_Y

    cube(
        f"TRACK_PLATFORM_{side}",
        (
            0,
            y,
            -0.10
        ),
        (
            TRACK_LENGTH / 2,
            1.0,
            0.10
        ),
        MAT_CONCRETE,
        0.08
    )


# =========================================================
# 입구 구조물
# =========================================================

ENTRANCE_X = TRACK_START - 1


# 왼쪽 기둥
cube(
    "ENTRANCE_LEFT",
    (
        ENTRANCE_X,
        -TRACK_WIDTH / 2 - 1.5,
        3
    ),
    (
        0.45,
        0.45,
        3
    ),
    MAT_RUST,
    0.1
)


# 오른쪽 기둥
cube(
    "ENTRANCE_RIGHT",
    (
        ENTRANCE_X,
        TRACK_WIDTH / 2 + 1.5,
        3
    ),
    (
        0.45,
        0.45,
        3
    ),
    MAT_RUST,
    0.1
)


# 입구 상단
cube(
    "ENTRANCE_TOP",
    (
        ENTRANCE_X,
        0,
        6
    ),
    (
        0.45,
        TRACK_WIDTH / 2 + 2,
        0.45
    ),
    MAT_RUST,
    0.1
)


# 입구 간판
cube(
    "ENTRANCE_SIGN",
    (
        ENTRANCE_X,
        0,
        5.9
    ),
    (
        0.25,
        4.5,
        0.9
    ),
    MAT_SIGN,
    0.1
)


# =========================================================
# 피니쉬 게이트
# =========================================================

FINISH_X = TRACK_END


# 왼쪽
cube(
    "FINISH_POST_LEFT",
    (
        FINISH_X,
        -TRACK_WIDTH / 2 - 0.8,
        3.5
    ),
    (
        0.35,
        0.35,
        3.5
    ),
    MAT_RUST
)


# 오른쪽
cube(
    "FINISH_POST_RIGHT",
    (
        FINISH_X,
        TRACK_WIDTH / 2 + 0.8,
        3.5
    ),
    (
        0.35,
        0.35,
        3.5
    ),
    MAT_RUST
)


# 피니쉬 상단
cube(
    "FINISH_TOP",
    (
        FINISH_X,
        0,
        7
    ),
    (
        0.35,
        TRACK_WIDTH / 2 + 1.2,
        0.35
    ),
    MAT_RUST
)


# 피니쉬 간판
cube(
    "FINISH_SIGN",
    (
        FINISH_X,
        0,
        6.6
    ),
    (
        0.25,
        3.5,
        0.65
    ),
    MAT_SIGN,
    0.08
)


# =========================================================
# 트랙 양쪽 파이프
# =========================================================

for side in [-1, 1]:

    y = side * 9.5

    cylinder(
        f"LEAK_PIPE_{side}",
        (
            0,
            y,
            0.30
        ),
        0.15,
        TRACK_LENGTH,
        MAT_METAL,
        (
            0,
            math.radians(90),
            0
        )
    )

    # 파이프 연결부
    for x in range(-27, 28, 6):

        cylinder(
            f"PIPE_JOINT_{side}_{x}",
            (
                x,
                y,
                0.30
            ),
            0.2,
            0.35,
            MAT_RUST,
            (
                0,
                math.radians(90),
                0
            )
        )


# =========================================================
# 누수
# =========================================================

for i in range(18):

    x = random.uniform(-28, 28)

    side = random.choice([-1, 1])

    y = side * 9.5

    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=8,
        ring_count=6,
        radius=random.uniform(0.04, 0.1),
        location=(
            x,
            y,
            random.uniform(0.05, 0.35)
        )
    )

    drop = bpy.context.object
    drop.name = "WATER_DROP"

    drop.data.materials.append(MAT_WATER)


# =========================================================
# 조명
# =========================================================

world = bpy.context.scene.world

if world:

    world.color = (
        0.025,
        0.025,
        0.02
    )


bpy.ops.object.light_add(
    type="SUN",
    location=(0, 0, 20)
)

sun = bpy.context.object

sun.name = "SUN"

sun.rotation_euler = (
    math.radians(25),
    math.radians(-20),
    math.radians(-25)
)

sun.data.energy = 1.8


# =========================================================
# 카메라
# =========================================================

bpy.ops.object.camera_add(
    location=(-30, -24, 10)
)

camera = bpy.context.object

target = Vector(
    (5, 0, 1)
)

direction = target - camera.location

camera.rotation_euler = direction.to_track_quat(
    "-Z",
    "Y"
).to_euler()

camera.data.lens = 38

bpy.context.scene.camera = camera


# =========================================================
# 렌더
# =========================================================

scene = bpy.context.scene

try:
    scene.render.engine = "BLENDER_EEVEE_NEXT"
except TypeError:
    scene.render.engine = "BLENDER_EEVEE"

scene.render.resolution_x = 1920
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100

scene.render.image_settings.file_format = "PNG"


# =========================================================
# 완료
# =========================================================

print("==============================================")
print(" RACE TRACK CLEANUP COMPLETE")
print("==============================================")
print("6 LANE TRACK       : YES")
print("LOW BARRIERS       : YES")
print("TRACK PLATFORM     : YES")
print("ENTRANCE STRUCTURE : YES")
print("FINISH LINE        : YES")
print("FINISH STRUCTURE   : YES")
print("PIPES / LEAK       : YES")
print("----------------------------------------------")
print("TICKET BOOTH       : REMOVED")
print("GRAVEL / DEBRIS    : REMOVED")
print("OUTSIDE GROUND     : REMOVED")
print("==============================================")
