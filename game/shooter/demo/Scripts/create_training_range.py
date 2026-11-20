"""Build the new training facility. Original DemoRange and edited maps are preserved."""
from pathlib import Path
import runpy
import unreal

assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAP = '/Game/demo/Maps/TrainingFacility'
helpers = runpy.run_path(str(Path(__file__).with_name('create_demo.py')))
material = helpers['material']

if assets.does_asset_exist(MAP) and '-DemoRebuildTraining' not in unreal.SystemLibrary.get_command_line():
    unreal.log('TRAINING_FACILITY_READY: preserving existing map')
else:
    floor = material('M_TrainingFloor', (.055,.075,.09), .08,.85)
    concrete = material('M_TrainingConcrete', (.22,.255,.27), .05,.83)
    frame = material('M_TrainingFrame', (.027,.042,.052), .65,.4)
    armor = material('M_TrainingArmor', (.42,.46,.47), .55,.45)
    black = material('M_TrainingRubber', (.009,.014,.017), .0,.85)
    teal = material('M_TrainingTeal', (.035,.44,.35), .45,.4,1.4)
    orange = material('M_TrainingOrange', (.75,.19,.045), .35,.5)
    yellow = material('M_TrainingYellow', (.85,.55,.08), .2,.6)
    light = material('M_TrainingLight', (.45,.75,.85), .1,.4,3)
    glass = material('M_TrainingWindows', (.025,.1,.14), .8,.18)
    # Low-cost world-space variation prevents a single flat slab appearance.
    edit = unreal.MaterialEditingLibrary
    edit.delete_all_material_expressions(floor)
    world = edit.create_material_expression(floor, unreal.MaterialExpressionWorldPosition, -700,-180)
    noise = edit.create_material_expression(floor, unreal.MaterialExpressionNoise,-460,-180)
    noise.set_editor_property('scale', .005)
    noise.set_editor_property('quality', 1)
    edit.connect_material_expressions(world,'',noise,'Position')
    tint_a = edit.create_material_expression(floor,unreal.MaterialExpressionConstant3Vector,-460,0)
    tint_a.set_editor_property('constant',unreal.LinearColor(.055,.075,.09))
    tint_b = edit.create_material_expression(floor,unreal.MaterialExpressionConstant3Vector,-460,100)
    tint_b.set_editor_property('constant',unreal.LinearColor(.065,.085,.10))
    mix = edit.create_material_expression(floor,unreal.MaterialExpressionLinearInterpolate,-180,0)
    for node,pin in [(tint_a,'A'),(tint_b,'B'),(noise,'Alpha')]: edit.connect_material_expressions(node,'',mix,pin)
    edit.connect_material_property(mix,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=edit.create_material_expression(floor,unreal.MaterialExpressionConstant,-180,180)
    rough.set_editor_property('r',.85)
    edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(floor)
    assets.save_loaded_asset(floor)
    if assets.does_asset_exist(MAP):
        if not levels.load_level(MAP): raise RuntimeError('Cannot load facility for explicit rebuild')
        for actor in actors.get_all_level_actors():
            if not isinstance(actor, (unreal.WorldSettings, unreal.LevelScriptActor)):
                actors.destroy_actor(actor)
    elif not levels.new_level(MAP):
        raise RuntimeError('Cannot create training facility')
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')

    def block(name,pos,size,mat,folder='Architecture',pitch=0,yaw=0,mesh=None,collision=True):
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),unreal.Rotator(pitch=pitch,yaw=yaw,roll=0))
        a.set_actor_label(name); a.set_folder_path(unreal.Name('demo/'+folder))
        c=a.static_mesh_component; c.set_static_mesh(mesh or cube); c.set_material(0,mat)
        c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
        a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
        return a

    def sign(value,pos,size=45,yaw=180,color=(.5,.85,.85)):
        a=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*pos),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
        a.set_actor_label('Sign / '+value); a.set_folder_path(unreal.Name('demo/Wayfinding'))
        c=a.text_render; c.set_text(value); c.set_world_size(size)
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_text_render_color(unreal.Color(*(int(v*255) for v in color),255))
        return a

    block('Facility foundation',(3200,0,-30),(11600,8800,60),floor)
    # Expansion joints and lane markings, all flush with collision floor.
    for x in range(-2000,9001,1000): block('Expansion joint',(x,0,.25),(2,8600,.4),black,'Markings',collision=False)
    for y in range(-4000,4001,1000): block('Expansion joint',(3200,y,.25),(11400,2,.4),black,'Markings',collision=False)
    for y in [-1500,1250,3500]:
        block('Range boundary stripe',(3400,y,1),(10500,8,1),teal,'Markings',collision=False)
    block('Movement boundary stripe',(3000,-3500,1),(10200,9,1),yellow,'Markings',collision=False)
    for distance in [10,25,50,75]:
        x=-900+distance*100
        block('Distance stripe %dm'%distance,(x,0,1.1),(5,2300,1),teal,'Markings',collision=False)
        sign('%02d M'%distance,(x,-1180,12),40,yaw=180)
    # Concrete backstop with alternating inset impact panels.
    for y in range(-4000,4001,800):
        block('Backstop buttress',(8900,y,450),(130,90,900),frame)
        block('Ballistic backstop',(9020,y+350,380),(160,660,760),concrete)
        block('Backstop panel',(8925,y+350,300),(24,600,430),black)
        block('Backstop identification',(8908,y+350,600),(4,420,8),orange,'Wayfinding')
    for side in [-1,1]:
        for x in range(-2000,9001,1000):
            block('Perimeter panel',(x,side*4400,190),(970,65,380),concrete)
            block('Perimeter cap',(x,side*4400,390),(985,85,15),frame)
            block('Perimeter inset',(x,side*4360,110),(860,4,6),teal,'Lighting',collision=False)
    block('Rear barrier',(-2500,0,230),(80,8800,460),concrete)
    # Covered briefing deck and clear central firing entrance.
    block('Briefing canopy',(-1300,0,590),(2300,8400,60),frame)
    for y in [-4000,-1500,1500,4000]:
        block('Canopy column',(-200,y,285),(90,90,570),frame)
        block('Column safety band',(-200,y,120),(94,94,50),yellow,'Wayfinding')
    block('Entrance header',(-200,0,450),(120,2900,100),frame)
    sign('P U L S E  /  TRAINING',(-270,0,440),54)
    sign('01   PRECISION',(-270,-1000,330),42)
    sign('02   TRACKING',(-270,2350,410),48)
    sign('03   MOMENTUM',(-270,-2700,410),48,color=(.95,.65,.25))
    for y in [-2900,0,2700]:
        block('Deck strip light',(-1300,y,552),(1600,16,9),light,'Lighting',collision=False)
        lamp=actors.spawn_actor_from_class(unreal.RectLight,unreal.Vector(-1000,y,525),unreal.Rotator(pitch=-90,yaw=0,roll=0))
        lamp.set_actor_label('Briefing deck area light')
        lamp.set_folder_path(unreal.Name('demo/Lighting'))
        lamp.light_component.set_editor_property('intensity',150000.)
        lamp.light_component.set_editor_property('attenuation_radius',1800.)
        lamp.light_component.set_editor_property('source_width',650.)
        lamp.light_component.set_editor_property('source_height',100.)
    # Firing pads and benches stand to the sides, leaving space to slide and strafe.
    for y in [-950,950,2300]:
        block('Firing pad',(-300,y,1.5),(650,500,2),frame,'Stations',collision=False)
        block('Pad leading edge',(30,y,3),(5,500,3),teal,'Stations',collision=False)
        block('Bench',(-1350,y,75),(80,370,12),armor,'Stations')
        for sy in [-140,140]: block('Bench support',(-1350,y+sy,35),(55,25,70),frame,'Stations')
    sign('SHIFT + CTRL : SLIDE\nSPACE : SLIDE JUMP\nE : RESET / REFILL\nT : 60 SECOND DRILL',(-2300,0,310),45,yaw=0)
    # Targets have real head hitboxes and reset independently.
    target_class=unreal.load_class(None,'/Script/shooter.DemoTarget')
    def target(x,y,moving=0,speed=1):
        block('Target pedestal',(x,y,18),(130,180,36),frame,'Targets')
        a=actors.spawn_actor_from_class(target_class,unreal.Vector(x,y,125))
        a.set_actor_label('Tracking dummy' if moving else 'Precision dummy')
        a.set_folder_path(unreal.Name('demo/Targets'))
        a.set_editor_property('strafe_distance',float(moving)); a.set_editor_property('strafe_speed',float(speed))
        a.set_editor_property('reset_seconds',1.8)
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            c.set_material(0, teal if c.get_name() in ['Bullseye','HealthBar'] else orange if c.get_name()=='Head' else armor if c.get_name()=='TargetBody' else frame)
        return a
    for x,y in [(500,-600),(500,600),(1800,-450),(1800,450),(4100,-600),(4100,600),(6600,0)]: target(x,y)
    for x,y,d,s in [(1100,2300,340,1.4),(2800,2300,450,1.0),(4700,2300,550,.8)]:
        block('Tracking track',(x,y,3),(50,d*2+220,6),frame,'Targets')
        for sy in [-1,1]: block('Track marker',(x,y+sy*(d+110),15),(90,12,30),orange,'Targets')
        target(x,y,d,s)
    # Slide gates and a ramp/jump line; standing capsule cannot fit the gates.
    for x in [300,1700,4600]:
        for sy in [-1,1]:
            block('Slide gate pillar',(x,-2600+sy*380,130),(65,65,260),frame,'Movement')
            block('Slide gate stripe',(x,-2600+sy*380,75),(70,70,45),yellow,'Movement')
        block('Slide gate beam',(x,-2600,185),(80,825,55),frame,'Movement')
        sign('SLIDE',(x-43,-2600,205),37,color=(.95,.65,.25))
        for offset in [-80,80]: block('Slide approach chevron',(x-350,-2600+offset,1.4),(120,12,1),yellow,'Markings',yaw=35 if offset<0 else -35,collision=False)
    block('Acceleration ramp',(2750,-2600,100),(900,700,45),concrete,'Movement',pitch=12)
    block('Raised landing deck',(3380,-2600,177),(400,700,80),frame,'Movement')
    for x,h in [(5400,90),(5950,150),(6500,210)]:
        block('Jump platform',(x,-2600,h/2),(300,650,h),concrete,'Movement')
        block('Jump platform edge',(x,-2600,h+2),(302,652,4),yellow,'Movement')
    target(7300,-2600)
    # Cover positions for shooting out of a slide, plus environmental depth.
    for x,y in [(950,-1100),(2500,1000),(3500,-1200),(5500,1100)]:
        block('Training cover',(x,y,62),(100,260,124),frame,'Cover')
        block('Cover rim',(x,y,126),(108,268,5),orange,'Cover')
    for side in [-1,1]:
        for x in [1000,4000,7000]:
            block('Service building',(x,side*5100,520),(1800,1000,1040),frame,'Surroundings')
            for z in [300,650,920]:
                block('Service glazing',(x,side*4580,z),(1600,20,170),glass,'Surroundings')
                block('Service floor band',(x,side*4555,z-100),(1820,35,25),armor,'Surroundings')
    start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-900,0,100),unreal.Rotator(pitch=0,yaw=0,roll=0))
    start.set_actor_label('Training start')
    actors.spawn_actor_from_class(unreal.load_class(None,'/Script/shooter.DemoRangeDirector'),unreal.Vector())
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,2500),unreal.Rotator(pitch=-48,yaw=-35,roll=0))
    sun.light_component.set_editor_property('intensity',35000.)
    sun.light_component.set_editor_property('light_color',unreal.Color(255,235,210,255))
    sun.light_component.set_editor_property('light_source_angle',2.5)
    sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1500))
    sky.light_component.set_editor_property('intensity',1.2)
    sky.light_component.set_editor_property('real_time_capture',True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
    pp=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
    pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    for key,value in [('auto_exposure_min_brightness',11.5),('auto_exposure_max_brightness',11.5),('motion_blur_amount',0.),('bloom_intensity',.25),('vignette_intensity',.18)]:
        settings.set_editor_property('override_'+key,True); settings.set_editor_property(key,value)
    pp.set_editor_property('settings',settings)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Game/demo/Game/BP_DemoGameMode.BP_DemoGameMode_C'))
    if not levels.save_current_level(): raise RuntimeError('Cannot save TrainingFacility')
    unreal.log('TRAINING_FACILITY_CREATED')
