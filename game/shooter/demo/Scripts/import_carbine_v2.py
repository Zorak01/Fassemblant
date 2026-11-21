"""Import the Blender-authored replacement; does not touch character/arm assets."""
from pathlib import Path
import runpy
import unreal

source = Path(__file__).resolve().parents[4] / 'assets/demo/Weapons/Export'
destination = '/Game/demo/Weapons/CarbineV2'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
material = runpy.run_path(str(Path(__file__).with_name('create_demo.py')))['material']
surfaces = {
    'graphite': material('M_V2Anodized', (.035,.046,.057), .8,.28),
    'ceramic': material('M_V2Ceramic', (.085,.105,.12), .35,.46),
    'steel': material('M_V2Steel', (.16,.19,.22), .88,.34),
    'polymer': material('M_V2Polymer', (.013,.018,.021), .05,.7),
    'safety': material('M_V2Safety', (.7,.085,.018), .35,.4),
}
# Update the existing surfaces too; create-only helpers otherwise leave the
# shipped pale receiver and chrome-like rail unchanged on repeat imports.
edit=unreal.MaterialEditingLibrary
for key,color,metallic,roughness in [
    ('ceramic',(.085,.105,.12),.35,.46),('steel',(.16,.19,.22),.88,.34),
    ('graphite',(.035,.046,.057),.8,.38),('polymer',(.013,.018,.021),.05,.72)]:
    mat=surfaces[key]
    node=edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
    node.set_editor_property('constant',unreal.LinearColor(*color))
    for prop,value in [(unreal.MaterialProperty.MP_METALLIC,metallic),(unreal.MaterialProperty.MP_ROUGHNESS,roughness)]:
        edit.get_material_property_input_node(mat,prop).set_editor_property('r',value)
    edit.recompile_material(mat)
    if not assets.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError('Cannot save carbine material: '+key)
meshes = {}
for group in ['Body','Magazine','Bolt','ChargingHandle','Trigger']:
    task = unreal.AssetImportTask()
    task.filename = str(source / (group + '.fbx'))
    task.destination_path = destination
    task.destination_name = 'SM_Carbine_' + group
    task.automated = True
    task.save = True
    task.replace_existing = True
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.transform_vertex_to_absolute = True
    options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task.options = options
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(destination + '/SM_Carbine_' + group)
    if not mesh:
        raise RuntimeError('Import failed: ' + group)
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        name = str(slot.get_editor_property('imported_material_slot_name')).lower()
        chosen = next((v for k,v in surfaces.items() if k in name), surfaces['graphite'])
        mesh.set_material(index, chosen)
    if not assets.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError('Cannot save mesh: ' + group)
    meshes[group] = mesh

data = unreal.load_asset('/Game/demo/Weapons/DA_Kestrel')
old = data.get_editor_property('parts')
parts = []
attachments = {
    'Body':unreal.DemoWeaponAttachment.BODY,
    'Magazine':unreal.DemoWeaponAttachment.MAGAZINE,
    'Bolt':unreal.DemoWeaponAttachment.BOLT,
    'ChargingHandle':unreal.DemoWeaponAttachment.CHARGING_HANDLE,
    'Trigger':unreal.DemoWeaponAttachment.TRIGGER,
}
for group, mesh in meshes.items():
    part = unreal.DemoWeaponPart()
    part.name = 'CarbineV2_' + group
    part.mesh = mesh
    part.attachment = attachments[group]
    # The authored groups share the weapon origin; the magazine animation pivot
    # is offset in the existing weapon actor, so cancel its rest translation.
    part.transform = unreal.Transform(location=unreal.Vector(-8,0,5) if group=='Magazine' else unreal.Vector())
    parts.append(part)
for p in old:
    if p.attachment == unreal.DemoWeaponAttachment.MUZZLE_FLASH:
        parts.append(p)
    elif str(p.name) == 'OpticDot':
        p.transform = unreal.Transform(location=unreal.Vector(4.6,0,7.8))
        parts.append(p)
data.set_editor_property('parts', parts)
data.set_editor_property('sight', unreal.Transform(location=unreal.Vector(2,0,7.8)))
data.set_editor_property('muzzle', unreal.Vector(59.5,0,0))
# Magazine fingers run upward and curl inward, unlike the horizontal foregrip.
# The left hand's finger/index-to-pinky basis has its Z axis on the dorsal side.
data.set_editor_property('magazine_hand', unreal.Transform(
    location=unreal.Vector(8,-4,-11), rotation=unreal.Rotator(roll=-90,pitch=70,yaw=0)))
if not assets.save_loaded_asset(data, only_if_is_dirty=False):
    raise RuntimeError('Cannot save rifle visual data; check out DA_Kestrel first.')
unreal.log('CARBINE_V2_IMPORTED')
