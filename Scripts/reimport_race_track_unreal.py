from pathlib import Path
import shutil

import unreal


root = Path(__file__).resolve().parents[1]
asset_path = '/Game/KCU/HorseRacing/Untitled'
mesh = unreal.load_asset(asset_path)
if not mesh:
    raise RuntimeError('Existing race mesh was not found')
backup = root / 'Saved' / 'RaceTrackBackup' / 'Untitled.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(root / 'Content/KCU/HorseRacing/Untitled.uasset', backup)

before_bounds = mesh.get_bounds()
before_triangles = mesh.get_num_triangles(0)
materials = list(mesh.static_materials)
body = mesh.get_editor_property('body_setup')
collision = body.get_editor_property('agg_geom')
collision_shapes = {name: list(collision.get_editor_property(name)) for name in
                    ('box_elems', 'sphere_elems', 'sphyl_elems', 'convex_elems')}
collision_mode = body.get_editor_property('collision_trace_flag')
def collision_count(geometry):
    return tuple(len(geometry.get_editor_property(name)) for name in
                 ('box_elems', 'sphere_elems', 'sphyl_elems', 'convex_elems'))


before_collision = collision_count(collision)
print('RACE_REIMPORT_BEFORE', before_triangles, before_bounds, before_collision)

data = mesh.get_editor_property('asset_import_data')
data.scripted_add_filename(str(root / 'Art/HorseRacing/Untitled.fbx'), 0, '')
params = unreal.ImportAssetParameters()
params.set_editor_property('is_automated', True)
params.set_editor_property('reimport_asset', mesh)
params.set_editor_property('reimport_source_index', 0)
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
result = manager.reimport_asset(mesh, params)
print('RACE_REIMPORT_RESULT', result)
if not result or (isinstance(result, tuple) and not result[0]):
    raise RuntimeError('Interchange reimport failed; asset was not saved')

mesh = unreal.load_asset(asset_path)
old_materials = {str(m.material_slot_name): m.material_interface for m in materials}
slots = list(mesh.static_materials)
assert set(str(m.material_slot_name) for m in slots) == set(old_materials)
for slot in slots:
    slot.material_interface = old_materials[str(slot.material_slot_name)]
mesh.set_editor_property('static_materials', slots)
body = mesh.get_editor_property('body_setup')
restored_collision = body.get_editor_property('agg_geom')
for name, shapes in collision_shapes.items():
    restored_collision.set_editor_property(name, shapes)
body.set_editor_property('agg_geom', restored_collision)
body.set_editor_property('collision_trace_flag', collision_mode)
after_bounds = mesh.get_bounds()
after_triangles = mesh.get_num_triangles(0)
after_collision = collision_count(body.get_editor_property('agg_geom'))
print('RACE_REIMPORT_AFTER', after_triangles, after_bounds, after_collision)
assert after_triangles > before_triangles, (before_triangles, after_triangles)
for axis in ('x', 'y', 'z'):
    assert abs(getattr(after_bounds.origin, axis) - getattr(before_bounds.origin, axis)) < 0.1
    assert abs(getattr(after_bounds.box_extent, axis) - getattr(before_bounds.box_extent, axis)) < 0.1
assert after_collision == before_collision
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
    raise RuntimeError('Could not save updated race mesh')
print('RACE_REIMPORT_SAVED', asset_path, after_triangles, after_bounds, after_collision)
