"""Author the demo's own carbine and derive arms from Epic's bundled Manny.
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

graphite = material("M_RifleGraphite", (0.032, 0.043, 0.052), 0.78, 0.3)
ceramic = material("M_RifleCeramic", (0.38, 0.33, 0.24), 0.40, 0.43)
steel = material("M_RifleSteel", (0.24, 0.29, 0.32), 0.92, 0.25)
rubber = material("M_RifleRubber", (0.013, 0.018, 0.021), 0.0, 0.82)
accent = material("M_RifleAccent", (0.85, 0.18, 0.035), 0.55, 0.4)
glow = material("M_OpticEmitter", (1.0, 0.02, 0.005), 0.0, 0.4, 10.0)
flash = material("M_MuzzleEmitter", (1.0, 0.39, 0.065), 0.0, 0.4, 15.0)
arms_material = material("M_ArmsCeramic", (0.32, 0.39, 0.42), 0.65, 0.4)
for skinned_surface in [arms_material, rubber]:
    unreal.MaterialEditingLibrary.set_material_usage(skinned_surface, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.MaterialEditingLibrary.recompile_material(skinned_surface)
    assets.save_loaded_asset(skinned_surface, only_if_is_dirty=False)

arms = unreal.DemoEditorLibrary.prepare_mannequin()
if not arms:
    raise RuntimeError("Mannequin arms extraction failed; inspect DemoArmsSetup.log / shooter.log")
slots = arms.get_editor_property("materials")
for slot, entry in enumerate(slots):
    entry.set_editor_property("material_interface", arms_material if slot == 0 else rubber)
arms.set_editor_property("materials", slots)
if not assets.save_loaded_asset(arms, only_if_is_dirty=False):
    raise RuntimeError("Cannot save mannequin arms (check file permissions).")


def shape(name, sections, surface, sides=8, wall=0.0):
    result = unreal.DemoEditorLibrary.create_profile_mesh(name, [unreal.Vector(*s) for s in sections], sides, wall, surface)
    if not result:
        raise RuntimeError("Weapon mesh build failed: " + name)
    return result


receiver = shape("SM_KestrelReceiver", [(-10, 2.25, 2.55), (-8.5, 3, 3.25), (9, 3, 3.25), (14, 2.5, 2.6)], ceramic)
handguard = shape("SM_KestrelHandguard", [(12, 2.6, 2.6), (15, 2.7, 2.5), (33, 2.3, 2.3), (36, 1.9, 2.1)], graphite, wall=0.4)
barrel = shape("SM_KestrelBarrel", [(31, 0.8, 0.8), (48, 0.8, 0.8), (49, 1.0, 1.0), (52, 1.0, 1.0)], steel, 16)
brake = shape("SM_KestrelBrake", [(51, 1.5, 1.5), (52, 1.8, 1.8), (59, 1.8, 1.8), (60, 1.4, 1.4)], graphite, 12, 0.45)
stock = shape("SM_KestrelStock", [(-29, 1.9, 4.2), (-26, 1.9, 4.0), (-21, 1.45, 2.3), (-11, 1.45, 1.6)], graphite)
stock_pad = shape("SM_KestrelPad", [(-30, 2.0, 4.4), (-28.8, 2.0, 4.4)], rubber)
grip = shape("SM_KestrelGrip", [(0, 1.65, 2.05), (2, 1.8, 2.15), (10, 1.5, 2.1), (11, 1.4, 1.8)], rubber)
magazine = shape("SM_KestrelMagazine", [(0, 1.7, 3.1), (3, 1.8, 3.0), (11, 1.7, 2.7), (15, 1.6, 2.5)], graphite)
mag_pad = shape("SM_KestrelMagPad", [(14.5, 1.9, 2.8), (16, 1.9, 2.8)], accent)
optic = shape("SM_KestrelOptic", [(-1, 2.4, 2.0), (0, 2.7, 2.3), (3, 2.7, 2.3), (4, 2.35, 2.0)], graphite, 12, 0.35)
dot = shape("SM_KestrelReticle", [(-0.02, 0.045, 0.045), (0.02, 0.045, 0.045)], glow, 12)
beam = shape("SM_KestrelBeam", [(-0.5, 0.45, 0.45), (0.5, 0.45, 0.45)], graphite)
bolt = shape("SM_KestrelBolt", [(0, 0.35, 0.7), (6, 0.35, 0.7)], steel)
screw = shape("SM_KestrelScrew", [(-0.1, 0.32, 0.32), (0.2, 0.32, 0.32)], steel, 8)
flash_mesh = shape("SM_KestrelMuzzleFlash", [(0, 0.3, 0.3), (1.6, 1.4, 0.5), (5.5, 0.03, 0.03)], flash, 8)

data_path = ROOT + "/Weapons/DA_Kestrel"
data = unreal.load_asset(data_path) if assets.does_asset_exist(data_path) else None
if not data:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DemoWeaponVisualData)
    data = tools.create_asset("DA_Kestrel", ROOT + "/Weapons", unreal.DemoWeaponVisualData, factory)
    parts = []

    def part(name, mesh, pos=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1), attach=unreal.DemoWeaponAttachment.BODY, surface=None):
        p = unreal.DemoWeaponPart()
        p.set_editor_property("name", name)
        p.set_editor_property("mesh", mesh)
        if surface:
            p.set_editor_property("material", surface)
        p.set_editor_property("transform", unreal.Transform(location=unreal.Vector(*pos), rotation=unreal.Rotator(*rot), scale=unreal.Vector(*scale)))
        p.set_editor_property("attachment", attach)
        parts.append(p)

    part("Receiver", receiver)
    part("Handguard", handguard)
    part("Barrel", barrel)
    part("Brake", brake)
    part("Stock", stock, (0, 0, -1.0))
    part("StockPad", stock_pad, (0, 0, -1.0))
    part("PistolGrip", grip, (-3, 0, -2.5), (-105, 0, 0))
    part("ForeGrip", grip, (18, 0, -1.5), (-75, 0, 0), (0.55, 1, 0.85))
    part("MagazineShell", magazine, rot=(-80, 0, 0), attach=unreal.DemoWeaponAttachment.MAGAZINE)
    part("MagazinePad", mag_pad, rot=(-80, 0, 0), attach=unreal.DemoWeaponAttachment.MAGAZINE)
    part("OpticHousing", optic, (2, 0, 8.2))
    part("OpticMount", beam, (3, 0, 4.8), scale=(8, 3, 2.8), surface=steel)
    part("OpticDot", dot, (11, 0, 8.2))
    part("EjectionSlot", beam, (3, -2.82, 1), scale=(12, 0.6, 2), surface=rubber)
    part("BoltCarrier", bolt, (0, -3.1, 1), attach=unreal.DemoWeaponAttachment.BOLT)
    part("ChargingLever", beam, (-5, -3.2, 3), scale=(3, 4, 1), attach=unreal.DemoWeaponAttachment.CHARGING_HANDLE, surface=steel)
    part("TriggerBlade", beam, (-0.5, 0, -5), (-35, 0, 0), (2.5, 0.8, 0.8), unreal.DemoWeaponAttachment.TRIGGER, steel)
    for side in [-1, 1]:
        part("TriggerGuardSide" + str(side), beam, (-0.2, side * 1.2, -6), scale=(7, 0.7, 0.7))
        for index in range(5):
            part("HandguardVent%s_%s" % (side, index), beam, (17 + index * 3.5, side * 2.35, 0.1), scale=(2.3, 0.7, 1.9), surface=rubber)
        for index in range(3):
            part("ReceiverScrew%s_%s" % (side, index), screw, (-7 + index * 6, side * 2.95, 0), (0, side * 90, 0))
    for index in range(10):
        part("RailTooth%s" % index, beam, (-7 + index * 3.8, 0, 3.2), scale=(1.2, 4.0, 0.5), surface=steel)
    for index in range(3):
        part("BarrelCollar%s" % index, brake, (-16 + index * 3, 0, 0), scale=(1, 0.65, 0.65), surface=steel)
    part("MuzzleBurst", flash_mesh, attach=unreal.DemoWeaponAttachment.MUZZLE_FLASH)
    part("MuzzleBurstCross", flash_mesh, rot=(0, 0, 90), attach=unreal.DemoWeaponAttachment.MUZZLE_FLASH)
    data.set_editor_property("parts", parts)
    data.set_editor_property("right_hand", unreal.Transform(location=unreal.Vector(-1, 3.3, -6), rotation=unreal.Rotator(0, 0, 90)))
    data.set_editor_property("left_hand", unreal.Transform(location=unreal.Vector(18, -2.8, -4), rotation=unreal.Rotator(0, 0, -90)))
    assets.save_loaded_asset(data)


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
character.set_editor_property("scope_fov", 78.0)
motion = character.get_editor_property("motion")
motion.set_editor_property("footstep_sound", step_sound)
motion.set_editor_property("landing_sound", land_sound)
if not assets.save_loaded_asset(character_bp, only_if_is_dirty=False):
    raise RuntimeError("Cannot save BP_DemoCharacter; make the asset writable or check it out first.")
assets.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
required = [ROOT + path for path in [
    "/Character/Mannequin/SK_DemoArms", "/Character/Mannequin/SK_DemoSkeleton",
    "/Character/BP_DemoCharacter", "/Weapons/BP_DemoRifle", "/Weapons/DA_Kestrel",
    "/Audio/S_KestrelFire", "/Audio/S_KestrelReload", "/Audio/S_ConcreteStep", "/Audio/S_ConcreteLand",
    "/Materials/M_ArmsCeramic", "/Materials/M_RifleRubber"]]
required += [str(p).split(".")[0] for p in assets.list_assets(ROOT + "/Weapons/Meshes", recursive=True, include_folder=False)]
marker = Path(__file__).resolve().parents[1] / "FirstPersonAssets.json"
marker.write_text(json.dumps({"schema": 1, "assets": sorted(set(required))}, indent=2) + "\n", encoding="utf-8")
unreal.log("DEMO_FIRST_PERSON_READY: physical optic, Kestrel carbine, mannequin arms, synchronized audio")
