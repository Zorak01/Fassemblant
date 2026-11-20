"""Select Epic's actual mannequin and rifle animation assets for the demo."""
from pathlib import Path
import runpy
import unreal

runpy.run_path(str(Path(__file__).with_name('copy_template_mannequin.py')))
assets = unreal.EditorAssetLibrary
bp = unreal.load_asset('/Game/demo/Character/BP_DemoCharacter')
character = unreal.get_default_object(unreal.load_class(None, bp.get_path_name() + '_C'))
character.set_editor_property('arms_mesh', unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'))
for prop, name in [('arms_idle_pose','MF_Rifle_Idle_ADS'),('arms_fire_pose','MM_Rifle_Fire'),('arms_reload_pose','MM_Rifle_Reload')]:
    sequence = unreal.load_asset('/Game/Characters/Mannequins/Anims/Rifle/' + name)
    if not sequence:
        raise RuntimeError('Required Epic animation missing: ' + name)
    character.set_editor_property(prop, sequence)
    unreal.log('EPIC_POSE_SELECTED %s additive=%s' % (name, sequence.get_editor_property('additive_anim_type')))
if not assets.save_loaded_asset(bp, only_if_is_dirty=False):
    raise RuntimeError('Cannot save character Blueprint; check out the asset first.')
data = unreal.load_asset('/Game/demo/Weapons/DA_Kestrel')
data.set_editor_property('hip_position', unreal.Vector(37,12,-13))
# Use named Euler arguments: Unreal's Python constructor order differs from C++.
# The dominant palm basis is measured from the original rifle idle pose after
# the mesh's -90 degree yaw, so the authored wrist never needs to be twisted.
right_rotation = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(9.013,2.061,2.363), unreal.Vector(.111,-.639,7.032))
data.set_editor_property('right_hand', unreal.Transform(location=unreal.Vector(-12,3,-9),rotation=right_rotation))
data.set_editor_property('left_hand', unreal.Transform(location=unreal.Vector(19,-4.5,-5.5),rotation=unreal.Rotator(roll=-90,pitch=0,yaw=0)))
if not assets.save_loaded_asset(data, only_if_is_dirty=False):
    raise RuntimeError('Cannot save replacement rifle grip calibration.')
unreal.log('EPIC_TEMPLATE_ARMS_CONFIGURED')
