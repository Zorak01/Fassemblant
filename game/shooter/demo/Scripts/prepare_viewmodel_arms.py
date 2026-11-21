"""Derive textured first-person arms without Manny's camera-distance cutout.
The original mannequin mesh, skeleton, materials and textures stay untouched.
"""
from pathlib import Path
import json
import unreal

assets=unreal.EditorAssetLibrary
edit=unreal.MaterialEditingLibrary
root='/Game/demo/Character/ViewmodelMaterials'
source='/Game/Characters/Mannequins/Materials'

def copy_asset(original,destination):
    result=unreal.load_asset(destination) if assets.does_asset_exist(destination) else assets.duplicate_asset(original,destination)
    if not result: raise RuntimeError('Cannot derive viewmodel asset: '+destination)
    return result

base=copy_asset(source+'/M_Mannequin',root+'/M_ViewmodelMannequin')
# The template's masked material intentionally erases skin near the camera.
# Dedicated arms have no torso/head geometry, so this fade is unnecessary.
base.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
edit.recompile_material(base)
if not assets.save_loaded_asset(base,only_if_is_dirty=False): raise RuntimeError('Cannot save viewmodel base material')

instances={}
for name in ['MI_Manny_01_New','MI_Manny_02_New']:
    instance=copy_asset(source+'/Manny/'+name,root+'/'+name.replace('Manny','Viewmodel'))
    edit.set_material_instance_parent(instance,base)
    overrides=instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_blend_mode',True)
    overrides.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    instance.set_editor_property('base_property_overrides',overrides)
    edit.update_material_instance(instance)
    if not assets.save_loaded_asset(instance,only_if_is_dirty=False): raise RuntimeError('Cannot save '+name)
    instances[name]=instance

mesh=unreal.DemoEditorLibrary.prepare_viewmodel_arms('-DemoRebuildArms' in unreal.SystemLibrary.get_command_line())
if not mesh: raise RuntimeError('Viewmodel arms extraction failed')
slots=mesh.get_editor_property('materials')
for index,slot in enumerate(slots):
    original=slot.get_editor_property('material_interface')
    if original and original.get_name() in instances:
        slot.set_editor_property('material_interface',instances[original.get_name()])
        slots[index]=slot
mesh.set_editor_property('materials',slots)
if not assets.save_loaded_asset(mesh,only_if_is_dirty=False): raise RuntimeError('Cannot save first-person arms')

bp=unreal.load_asset('/Game/demo/Character/BP_DemoCharacter')
character=unreal.get_default_object(unreal.load_class(None,bp.get_path_name()+'_C'))
character.set_editor_property('arms_mesh',mesh)
arms_component=character.get_editor_property('first_person_arms')
for index,slot in enumerate(mesh.get_editor_property('materials')):
    surface=slot.get_editor_property('material_interface')
    unreal.log('VIEWMODEL_SLOT %d %s'%(index,surface.get_path_name() if surface else 'None'))
    arms_component.set_material(index,surface)
if not assets.save_loaded_asset(bp,only_if_is_dirty=False): raise RuntimeError('Cannot save character viewmodel')

manifest=Path(__file__).resolve().parents[1]/'FirstPersonAssets.json'
data=json.loads(manifest.read_text(encoding='utf-8'))
data['assets']=sorted(set(data['assets']+[mesh.get_path_name().split('.')[0],base.get_path_name().split('.')[0]]
    +[v.get_path_name().split('.')[0] for v in instances.values()]))
manifest.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
unreal.log('VIEWMODEL_ARMS_READY: original weights/textures, opaque first-person surfaces')
