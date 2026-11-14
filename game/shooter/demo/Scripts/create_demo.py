"""Run once using demo/Build-Demo.ps1. Creates only assets below /Game/demo.

Refuses to overwrite an existing map so running setup cannot erase level edits.
"""
import unreal

ROOT = "/Game/demo"
MAP = ROOT + "/Maps/DemoRange"
assets = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def material(name, color, metallic=0.0, roughness=0.6, emission=0.0):
    path = ROOT + "/Materials/" + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset(name, ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    rgb = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    rgb.set_editor_property("constant", unreal.LinearColor(*color))
    edit.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, number in [(unreal.MaterialProperty.MP_METALLIC, metallic), (unreal.MaterialProperty.MP_ROUGHNESS, roughness)]:
        value = edit.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150)
        value.set_editor_property("r", number)
        edit.connect_material_property(value, "", prop)
    if emission:
        glow = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 300)
        glow.set_editor_property("constant", unreal.LinearColor(*(c * emission for c in color)))
        edit.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat


def blueprint(name, parent, folder):
    path = ROOT + "/" + folder + "/" + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = asset_tools.create_asset(name, ROOT + "/" + folder, unreal.Blueprint, factory)
    assets.save_loaded_asset(bp)
    return bp


if assets.does_asset_exist(MAP):
    unreal.log("Demo map already exists; preserving all existing assets.")
else:
    for folder in ["Maps", "Materials", "Character", "Weapons", "Game", "UI", "World", "Audio", "Animations"]:
        assets.make_directory(ROOT + "/" + folder)

    floor_mat = material("M_Floor", (0.105, 0.13, 0.15), 0.2, 0.7)
    wall_mat = material("M_Concrete", (0.24, 0.28, 0.30), 0.0, 0.9)
    dark_mat = material("M_Weapon", (0.035, 0.045, 0.055), 0.75, 0.32)
    mint_mat = material("M_Mint", (0.08, 0.75, 0.52), 0.1, 0.4, 0.5)
    orange_mat = material("M_Target", (0.95, 0.23, 0.06), 0.25, 0.5)
    white_mat = material("M_Light", (0.8, 0.92, 1.0), 0.0, 0.4, 3)

    character_bp = blueprint("BP_DemoCharacter", unreal.load_class(None, "/Script/shooter.DemoCharacter"), "Character")
    weapon_bp = blueprint("BP_DemoRifle", unreal.load_class(None, "/Script/shooter.DemoWeapon"), "Weapons")
    mode_bp = blueprint("BP_DemoGameMode", unreal.load_class(None, "/Script/shooter.DemoGameMode"), "Game")
    target_bp = blueprint("BP_DemoTarget", unreal.load_class(None, "/Script/shooter.DemoTarget"), "World")
    rifle_class = unreal.load_class(None, weapon_bp.get_path_name() + "_C")
    character_class = unreal.load_class(None, character_bp.get_path_name() + "_C")
    mode_class = unreal.load_class(None, mode_bp.get_path_name() + "_C")
    target_class = unreal.load_class(None, target_bp.get_path_name() + "_C")
    unreal.get_default_object(character_class).set_editor_property("weapon_class", rifle_class)
    unreal.get_default_object(mode_class).set_editor_property("default_pawn_class", character_class)
    assets.save_loaded_asset(character_bp)
    assets.save_loaded_asset(mode_bp)

    if not levels.new_level(MAP):
        raise RuntimeError("Could not create demo level")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")

    def block(name, position, size, mat, folder="Architecture"):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
        actor.set_actor_label(name)
        actor.set_folder_path(unreal.Name("demo/" + folder))
        mesh = actor.static_mesh_component
        mesh.set_static_mesh(cube)
        mesh.set_material(0, mat)
        actor.set_actor_scale3d(unreal.Vector(*(s / 100.0 for s in size)))
        return actor

    block("Range floor", (1500, 0, -25), (5000, 3200, 50), floor_mat)
    block("Backstop", (4000, 0, 240), (100, 3200, 480), wall_mat)
    for sign in [-1, 1]:
        block("Side wall", (1500, sign * 1600, 170), (5000, 70, 340), wall_mat)
        block("Perimeter light", (1500, sign * 1558, 80), (4900, 6, 7), mint_mat, "Lighting")
    block("Rear wall", (-1000, 0, 170), (50, 3200, 340), wall_mat)
    for lane in [-1000, -500, 0, 500, 1000]:
        block("Lane stripe", (1600, lane + 200, 1), (4400, 4, 1), mint_mat, "Markings")
    for distance in [500, 1500, 2500, 3500]:
        block("Distance line", (distance, 0, 1.2), (3, 2800, 1), mint_mat, "Markings")
        text = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(distance, -1400, 12), unreal.Rotator(90, 180, 0))
        text.set_actor_label("Distance marker")
        text.set_folder_path(unreal.Name("demo/Markings"))
        text.text_render.set_text(str(distance // 100) + " M")
        text.text_render.set_world_size(36)

    for index, (x, y, moving) in enumerate([(700, -850, 0), (1200, -380, 0), (1800, 100, 150), (2400, 650, 0), (3300, -700, 230), (3300, 900, 0)]):
        block("Target plinth", (x, y, 40), (100, 110, 80), dark_mat, "Targets")
        target = actors.spawn_actor_from_class(target_class, unreal.Vector(x, y, 145))
        target.set_actor_label("Target %02d" % (index + 1))
        target.set_folder_path(unreal.Name("demo/Targets"))
        target.set_editor_property("strafe_distance", float(moving))
        target.set_editor_property("strafe_speed", 0.8 + index * 0.1)
        for component in target.get_components_by_class(unreal.StaticMeshComponent):
            component.set_material(0, mint_mat if component.get_name() == "Bullseye" else orange_mat)

    for y in [-1000, 700]:
        for step in range(4):
            block("Jump practice step", (-300 + step * 120, y, 10 + step * 10), (105, 230, 20 + step * 20), wall_mat, "Movement")
    for y in [-550, 550]:
        block("Cover", (200, y, 55), (70, 220, 110), wall_mat, "Movement")
        block("Cover edge", (200, y, 112), (72, 224, 3), mint_mat, "Movement")

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-600, 0, 100), unreal.Rotator(0, 0, 0))
    start.set_actor_label("Demo start")
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1200), unreal.Rotator(-55, -25, 0))
    sun.light_component.set_editor_property("intensity", 4.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800))
    sky.light_component.set_editor_property("intensity", 1.0)
    sky.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    for x in [-500, 1000, 2500]:
        for y in [-1200, 1200]:
            block("Light rail", (x, y, 420), (12, 200, 6), white_mat, "Lighting")
            light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 390))
            light.light_component.set_editor_property("intensity", 5000.0)
            light.light_component.set_editor_property("attenuation_radius", 1800.0)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", mode_class)
    levels.save_current_level()
    assets.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
    unreal.log("DEMO_CREATED: " + MAP)
