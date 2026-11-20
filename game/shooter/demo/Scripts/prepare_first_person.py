"""Prepare demo audio and fallbacks; use Epic's original template mannequin.
No First Person Sample assets or projects are read or imported.
Existing art assets and user-edited maps are preserved.
"""
import array
import math
import json
from pathlib import Path
import random
import runpy
import wave
import unreal

helpers = runpy.run_path(str(Path(__file__).with_name("create_demo.py")))
material = helpers["material"]
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/demo"

glow = material("M_OpticEmitter", (1.0, 0.02, 0.005), 0.0, 0.4, 10.0)
flash = material("M_MuzzleEmitter", (1.0, 0.39, 0.065), 0.0, 0.4, 15.0)

epic_mesh = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
if not assets.does_asset_exist(epic_mesh):
    runpy.run_path(str(Path(__file__).with_name("copy_template_mannequin.py")))
arms = unreal.load_asset(epic_mesh)
if not arms:
    raise RuntimeError("Install Unreal Templates and Feature Packs to prepare the original mannequin.")


def shape(name, sections, surface, sides=8, wall=0.0):
    result = unreal.DemoEditorLibrary.create_profile_mesh(name, [unreal.Vector(*s) for s in sections], sides, wall, surface)
    if not result:
        raise RuntimeError("Weapon mesh build failed: " + name)
    return result


# Only the tiny emissive effects use generated profile geometry. The rifle
# itself is authored in Blender and imported as five mechanical groups.
dot = shape("SM_KestrelReticle", [(-0.02, 0.045, 0.045), (0.02, 0.045, 0.045)], glow, 12)
flash_mesh = shape("SM_KestrelMuzzleFlash", [(0, 0.3, 0.3), (1.6, 1.4, 0.5), (5.5, 0.03, 0.03)], flash, 8)
data_path = ROOT + "/Weapons/DA_Kestrel"
data = unreal.load_asset(data_path) if assets.does_asset_exist(data_path) else None
if not data:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DemoWeaponVisualData)
    data = tools.create_asset("DA_Kestrel", ROOT + "/Weapons", unreal.DemoWeaponVisualData, factory)
parts = list(data.get_editor_property("parts"))
for name, mesh, attachment in [
    ("OpticDot", dot, unreal.DemoWeaponAttachment.BODY),
    ("MuzzleEmitter", flash_mesh, unreal.DemoWeaponAttachment.MUZZLE_FLASH),
]:
    if not any(str(p.name) == name for p in parts):
        part = unreal.DemoWeaponPart()
        part.name, part.mesh, part.attachment = name, mesh, attachment
        parts.append(part)
data.set_editor_property("parts", parts)
assets.save_loaded_asset(data, only_if_is_dirty=True)


def sound(name, kind):
    destination = ROOT + "/Audio/" + name
    if assets.does_asset_exist(destination):
        return unreal.load_asset(destination)
    source = Path(__file__).resolve().parents[4] / "assets" / "demo" / "Audio" / (name + ".wav")
    source.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(823 + kind)
    rate = 44100
    duration = [0.20, 1.9, 0.16, 0.26][kind]
    samples = array.array("h")
    filtered = 0.0
    for i in range(int(rate * duration)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        filtered = filtered * 0.82 + noise * 0.18
        if kind == 0:
            envelope = math.exp(-t * 28)
            value = (filtered * 1.5 + noise * math.exp(-t * 120) * 0.6 + math.sin(2 * math.pi * (105 * t - 80 * t * t)) * 0.5) * envelope
        elif kind == 1:
            value = 0
            for event, gain in [(0.10, 0.7), (0.48, 0.4), (1.15, 0.7), (1.57, 0.65), (1.72, 0.9)]:
                dt = t - event
                if dt >= 0:
                    value += (noise * 0.6 + math.sin(dt * 2200) * 0.3) * math.exp(-dt * 90) * gain
        else:
            value = (filtered * 1.8 + math.sin(2 * math.pi * (90 if kind == 2 else 60) * t) * 0.65) * math.exp(-t * (36 if kind == 2 else 22))
        value *= min(1.0, t * 2500)
        samples.append(int(max(-0.98, min(0.98, value)) * 27000))
    with wave.open(str(source), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(samples.tobytes())
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = ROOT + "/Audio"
    task.destination_name = name
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    result = unreal.load_asset(destination)
    if not result:
        raise RuntimeError("Sound import failed: " + name)
    return result


fire_sound = sound("S_KestrelFire", 0)
reload_sound = sound("S_KestrelReload", 1)
step_sound = sound("S_ConcreteStep", 2)
land_sound = sound("S_ConcreteLand", 3)
rifle_bp = unreal.load_asset(ROOT + "/Weapons/BP_DemoRifle")
rifle_class = unreal.load_class(None, rifle_bp.get_path_name() + "_C")
rifle = unreal.get_default_object(rifle_class)
rifle.set_editor_property("visual_data", data)
rifle.set_editor_property("fire_sound", fire_sound)
rifle.set_editor_property("reload_sound", reload_sound)
if not assets.save_loaded_asset(rifle_bp, only_if_is_dirty=False):
    raise RuntimeError("Cannot save BP_DemoRifle; make the asset writable or check it out first.")
character_bp = unreal.load_asset(ROOT + "/Character/BP_DemoCharacter")
character_class = unreal.load_class(None, character_bp.get_path_name() + "_C")
character = unreal.get_default_object(character_class)
character.set_editor_property("arms_mesh", arms)
for prop, clip in [("arms_idle_pose", "MF_Rifle_Idle_ADS"), ("arms_fire_pose", "MM_Rifle_Fire"), ("arms_reload_pose", "MM_Rifle_Reload")]:
    character.set_editor_property(prop, unreal.load_asset("/Game/Characters/Mannequins/Anims/Rifle/" + clip))
character.set_editor_property("scope_fov", 78.0)
motion = character.get_editor_property("motion")
motion.set_editor_property("footstep_sound", step_sound)
motion.set_editor_property("landing_sound", land_sound)
if not assets.save_loaded_asset(character_bp, only_if_is_dirty=False):
    raise RuntimeError("Cannot save BP_DemoCharacter; make the asset writable or check it out first.")
assets.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
runpy.run_path(str(Path(__file__).with_name("configure_template_arms.py")))
runpy.run_path(str(Path(__file__).with_name("setup_training.py")))
required = json.loads((Path(__file__).resolve().parents[1] / "FirstPersonAssets.json").read_text(encoding="utf-8"))["assets"]
required += [ROOT + path for path in [
    "/Character/BP_DemoCharacter", "/Weapons/BP_DemoRifle", "/Weapons/DA_Kestrel",
    "/Audio/S_KestrelFire", "/Audio/S_KestrelReload", "/Audio/S_ConcreteStep", "/Audio/S_ConcreteLand",
    "/Weapons/Meshes/SM_KestrelReticle", "/Weapons/Meshes/SM_KestrelMuzzleFlash",
    "/Materials/M_OpticEmitter", "/Materials/M_MuzzleEmitter"]]
content = Path(unreal.Paths.project_content_dir()).resolve()
for folder in [content / "Characters", content / "demo/Weapons/CarbineV2"]:
    required += ["/Game/" + str(p.relative_to(content).with_suffix("")).replace("\\", "/") for p in folder.rglob("*.uasset")]
required += [ROOT + "/Materials/M_V2" + name for name in ["Anodized", "Ceramic", "Steel", "Polymer", "Safety"]]
marker = Path(__file__).resolve().parents[1] / "FirstPersonAssets.json"
marker.write_text(json.dumps({"schema": 1, "assets": sorted(set(required))}, indent=2) + "\n", encoding="utf-8")
unreal.log("DEMO_FIRST_PERSON_READY: physical optic, Kestrel carbine, mannequin arms, synchronized audio")
