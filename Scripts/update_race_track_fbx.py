from pathlib import Path

import bpy
from io_scene_fbx import import_fbx


# Blender 5.2 cannot read the legacy light's removed Cycles shadow property.
import_fbx.blen_read_light = lambda *args: bpy.data.lights.new('OriginalLight', 'SUN')
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath='C:/Users/user/Documents/Untitled.fbx')

track = bpy.data.objects['RACE_TRACK']
assert tuple(round(v, 3) for v in track.dimensions) == (60.0, 18.0, 0.4)
lane_count = 6
lane_width = track.dimensions.y / lane_count

for i in range(lane_count + 1):
    name = f'LANE_LINE_{i}'
    obj = bpy.data.objects.get(name)
    if obj is None:
        obj = bpy.data.objects['LANE_LINE_0'].copy()
        obj.data = obj.data.copy()
        obj.name = name
        bpy.context.collection.objects.link(obj)
    obj.location.y = -track.dimensions.y / 2 + i * lane_width

for i in range(lane_count):
    name = f'LANE_NUMBER_{i + 1}'
    obj = bpy.data.objects.get(name)
    if obj is None:
        obj = bpy.data.objects['LANE_NUMBER_1'].copy()
        obj.data = obj.data.copy()
        obj.name = name
        bpy.context.collection.objects.link(obj)
    obj.location.y = -track.dimensions.y / 2 + (i + 0.5) * lane_width

bpy.ops.object.select_all(action='DESELECT')
meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
for obj in meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = track
out = Path(__file__).resolve().parents[1] / 'Art' / 'HorseRacing' / 'Untitled.fbx'
out.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.fbx(
    filepath=str(out), use_selection=True, object_types={'MESH'},
    axis_forward='-Z', axis_up='Y', add_leaf_bones=False, bake_anim=False,
)
print('RACE_EXPORT_OK', out, '6 lanes; original geometry and materials retained')
