"""Persist HISM shader usage for the existing palette without rebuilding any map."""
import unreal

names = ["M_TrainingFloor", "M_TrainingFrame", "M_TrainingArmor", "M_TrainingTeal",
         "M_TrainingOrange", "M_TrainingYellow", "M_TrainingWindows", "M_TrainingLight", "M_MuzzleEmitter"]
usage = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
for name in names:
    path = "/Game/demo/Materials/" + name
    material = unreal.load_asset(path)
    if not material:
        raise RuntimeError("Missing environment palette material: " + path)
    if not unreal.MaterialEditingLibrary.has_material_usage(material, usage):
        unreal.MaterialEditingLibrary.set_base_material_usage(material, usage)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not unreal.EditorAssetLibrary.save_loaded_asset(material):
            raise RuntimeError("Could not persist instanced material usage: " + path)
    unreal.log("ENVIRONMENT_MATERIAL_READY " + path)
