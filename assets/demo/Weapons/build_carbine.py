"""Blender source authoring for the replacement carbine. Run with blender -b -P.
Coordinates are centimeters, +X forward, +Y right, +Z up.
Mechanical groups export separately for Unreal animation.
"""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'Export'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
bpy.context.preferences.filepaths.save_version = 0
groups = {}

def material(name, color, metal, rough):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    s = m.node_tree.nodes.get('Principled BSDF')
    s.inputs['Base Color'].default_value = (*color, 1)
    s.inputs['Metallic'].default_value = metal
    s.inputs['Roughness'].default_value = rough
    return m

metal = material('Graphite anodized aluminum', (.035,.046,.057), .8,.28)
coat = material('Warm grey ceramic', (.085,.105,.12), .35,.46)
steel = material('Machined steel', (.16,.19,.22), .88,.34)
rubber = material('Grip polymer', (.013,.018,.021), .05,.7)
orange = material('Safety markings', (.7,.085,.018), .35,.4)

def finish(o, name, surface, group='Body', bevel=.12):
    o.name = name
    o.data.materials.append(surface)
    if bevel:
        mod = o.modifiers.new('Manufactured edge radii','BEVEL')
        mod.width = bevel
        mod.segments = 4
        mod.limit_method = 'ANGLE'
    for p in o.data.polygons:
        p.use_smooth = True
    normal = o.modifiers.new('Weighted surface normals','WEIGHTED_NORMAL')
    normal.keep_sharp = True
    groups.setdefault(group, []).append(o)
    return o

def box(name, pos, size, surface=metal, bevel=.12, group='Body', rotation=(0,0,0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    o = bpy.context.object
    o.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.rotation_euler = [math.radians(v) for v in rotation]
    return finish(o,name,surface,group,bevel)

def cylinder(name,pos,radius,length,surface=steel,axis='X',group='Body',vertices=64):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=length, location=pos)
    o=bpy.context.object
    if axis=='X': o.rotation_euler[1]=math.pi/2
    elif axis=='Y': o.rotation_euler[0]=math.pi/2
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(o,name,surface,group,.06)

def cut(target, tool):
    bpy.context.view_layer.objects.active=target
    # Cut before the edge finishing stack, retaining rounded cutout edges.
    mod=target.modifiers.new('Machined cutout','BOOLEAN')
    mod.operation='DIFFERENCE'
    mod.solver='EXACT'
    mod.object=tool
    bpy.ops.object.modifier_move_up(modifier=mod.name)
    bpy.ops.object.modifier_move_up(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for objects in groups.values():
        if tool in objects: objects.remove(tool)
    bpy.data.objects.remove(tool,do_unlink=True)

# An eight-sided extrusion with tapered ends replaces the slab-like receiver.
def receiver():
    sections=[(-11.5,1.65,2.05,.8),(-8.5,2.2,2.7,1),(10.5,2.2,2.7,1),(13.5,1.95,2.35,.7)]
    vertices=[]
    for x,w,h,z in sections:
        chamfer=.42
        for y,dz in [(-w+chamfer,-h),(w-chamfer,-h),(w,-h+chamfer),(w,h-chamfer),
                     (w-chamfer,h),(-w+chamfer,h),(-w,h-chamfer),(-w,-h+chamfer)]:
            vertices.append((x,y,z+dz))
    faces=[tuple(reversed(range(8))),tuple(range(24,32))]
    for ring in range(3):
        for j in range(8):
            a=ring*8+j; b=ring*8+(j+1)%8
            faces.append((a,b,b+8,a+8))
    mesh=bpy.data.meshes.new('Machined receiver profile'); mesh.from_pydata(vertices,[],faces); mesh.update()
    obj=bpy.data.objects.new('Faceted upper receiver',mesh); scene.collection.objects.link(obj)
    return finish(obj,'Faceted upper receiver',coat,bevel=.10)

upper=receiver()
lower=box('Lower receiver', (-1,0,-2.2),(19,3.8,3.8),metal,.23)
box('Upper seam', (1,0,3.6),(23,3.9,.35),metal,.08)
box('Magwell', (7,0,-4.3),(7,4.1,4.7),coat,.3)
box('Magwell flare', (7,0,-6.5),(7.8,4.6,.7),metal,.16)
slot=box('Port cutter',(3,-2.2,1.3),(9,1.5,2.0),rubber,0)
cut(upper,slot)
box('Port internal shadow',(3,-1.7,1.3),(9,.2,1.8),rubber,.03)
box('Visible bolt carrier',(3,-2.05,1.3),(7,.25,1.3),steel,.08,'Bolt')
box('Dust cover hinge',(3,-2.4,.05),(9,.3,.3),metal,.08)
for side in [-1,1]:
    cut(upper,box('Rear receiver relief',(-5.7,side*2.15,.9),(5.0,.5,1.6),rubber,.12))
    box('Inset receiver panel',(-5.7,side*1.94,.9),(4.6,.10,1.24),metal,.08)
    box('Receiver lower machining seam',(-2.5,side*2.18,-.9),(12,.08,.16),steel,.025)

# Slim free-float handguard with real side vents and a central barrel.
guard=box('Free float handguard',(25,0,0),(25,4.0,4.6),metal,.25)
cut(guard,box('Guard bore',(25,0,0),(27,3.1,3.6),rubber,0))
for x in [16,20,24,28,32,35]:
    for side in [-1,1]:
        cut(guard,box('Vent cutter',(x,side*2,0),(2.6,1.4,1.45),rubber,0))
        box('Vent edge lower',(x,side*2.06,-1.1),(2.4,.1,.18),steel,.04)
cylinder('Cold hammer forged barrel',(36,0,0),.73,40)
cylinder('Gas block',(39,0,0),1.05,2.2,metal)
cylinder('Gas tube',(27,0,1.4),.22,24,steel)
for x in [13.2,37]: cylinder('Handguard lock ring',(x,0,0),2.0,1.1,metal)
brake=cylinder('Ported muzzle brake',(56,0,0),1.25,6.5,metal)
cut(brake,cylinder('Bore cutter',(56,0,0),.48,8,rubber))
for x in [54.3,56.2,58]:
    cut(brake,box('Brake port',(x,0,0),(.8,4,.9),rubber,0))

# Picatinny rail dimensions, restrained tooth height and edge chamfers.
# Keep every tooth within the receiver/handguard support (rear face X=-10).
# The old rail began at -13.5 and floated behind the receiver.
# Raised integral saddle connects the rail to the lower handguard roof.
# Leave a visible setback before the handguard's front face (X=37.5).
box('Handguard rail saddle',(24,0,2.85),(23.5,2.0,1.15),metal,.10)
box('Continuous rail spine',(13.1,0,3.6),(45.0,2.0,.45),metal,.08)
for i in range(27):
    box('Rail tooth %02d'%i,(-8.5+i*1.65,0,3.98),(1.05,2.3,.32),steel,.07)

# Adjustable skeletonized stock and ribbed elastomer butt pad.
cylinder('Buffer tube',(-20,0,-.3),1.15,17,metal)
stock=box('Stock shell',(-27,0,-1.6),(13,3.5,7.5),coat,.4)
cut(stock,box('Stock window',(-24.5,0,-2.4),(7.0,6,3.0),rubber,0))
box('Cheek rest',(-27,0,2.1),(11,3.6,1.3),rubber,.35)
box('Butt pad',(-34,0,-1.6),(1.4,4.0,9),rubber,.35)
for z in [-4.6,-3.3,-2,-.7,.6,1.9]:
    box('Pad traction',(-34.8,0,z),(.2,3.6,.45),rubber,.07)
box('Stock adjustment latch',(-25,0,-5.2),(5.5,2,.6),metal,.12)

# Ergonomic pistol grip with inset traction panels.
box('Pistol grip',(-5,0,-8.2),(4.5,3.2,10.5),rubber,.6,rotation=(0,-16,0))
for side in [-1,1]:
    box('Grip panel',(-5,side*1.6,-8.5),(3.0,.18,7.0),metal,.2,rotation=(0,-16,0))
    for z in [-6,-7,-8,-9,-10,-11]:
        box('Grip traction groove',(-5,side*1.75,z),(2.6,.14,.13),rubber,.035)
box('Trigger guard lower',(-.5,0,-6.2),(5.8,1.9,.45),metal,.15)
box('Trigger guard front',(2.2,0,-4.8),(.5,1.9,3.0),metal,.14)
box('Trigger blade',(-.5,0,-4.6),(.55,.7,2.6),steel,.14,'Trigger',rotation=(0,-12,0))

# Detachable magazine and visible plate seam.
box('Magazine body',(7,0,-12.5),(6.2,3.3,14),metal,.45,'Magazine',rotation=(0,8,0))
box('Magazine base',(8,0,-19.5),(6.7,3.9,1.2),rubber,.25,'Magazine',rotation=(0,8,0))
for side in [-1,1]:
    for x in [5.2,7,8.8]:
        box('Magazine pressed rib',(x,side*1.7,-12),( .5,.2,10.5),steel,.13,'Magazine',rotation=(0,8,0))

# Charging handle, selector, takedown pins, fasteners and gas system detail.
box('Charging handle',(-9,-2.7,2.0),(3.8,2,.65),metal,.15,'ChargingHandle')
for side in [-1,1]:
    for x in [-8,-1,10,14]:
        cylinder('Receiver fastener',(x,side*2.25,-.6),.32,.35,steel,'Y',vertices=32)
    cylinder('Selector hub',(-6,side*2.2,-2.4),.5,.4,metal,'Y')
    box('Selector lever',(-5,side*2.45,-2.4),(2.3,.25,.35),steel,.1)
box('Safety witness mark',(-4,-2.25,-2.4),(.3,.1,.6),orange,.05)

# Low-profile enclosed reflex optic: a true through-aperture with rounded edges.
for side in [-1,1]:
    box('Reflex optic side',(2,side*2.08,7.8),(4.6,.55,4.6),metal,.24)
box('Reflex optic upper arch',(2,0,9.85),(4.6,4.7,.55),metal,.24)
box('Reflex optic lower sill',(2,0,5.75),(4.6,4.7,.55),metal,.24)
box('Optic mounting shoe',(2,0,5.0),(6.5,2.4,.9),steel,.14)
for side in [-1,1]:
    cylinder('Optic adjustment cap',(2,side*2.4,7.5),.65,.4,metal,'Y')
    cylinder('Mount cross bolt',(.5,side*1.4,5),.35,.45,steel,'Y')

# Source scene, exports and a neutral studio preview are reproducible.
for group, objects in groups.items():
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects: obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.export_scene.fbx(filepath=str(OUT/(group+'.fbx')),use_selection=True,
            object_types={'MESH'},apply_unit_scale=True,axis_forward='X',axis_up='Z',
            bake_anim=False,use_mesh_modifiers=True,add_leaf_bones=False,mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Carbine.blend'))
print('CARBINE_SOURCE_READY',sum(len(o.data.polygons) for g in groups.values() for o in g),'base polygons',len(groups),'mechanical groups')

# A real shaded preview for checking the silhouette and finishing before import.
bpy.ops.object.select_all(action='DESELECT')
scene.world.color=(.12,.12,.12)
def area(name,loc,energy,size):
    data=bpy.data.lights.new(name,'AREA'); data.energy=energy; data.shape='DISK'; data.size=size
    obj=bpy.data.objects.new(name,data); scene.collection.objects.link(obj); obj.location=loc
    obj.rotation_euler=(Vector((8,0,0))-obj.location).to_track_quat('-Z','Y').to_euler()
area('Key',(10,-55,65),12000,55)
area('Fill',(-20,35,30),7000,40)
area('Edge',(45,25,50),15000,35)
camera_data=bpy.data.cameras.new('InspectionCamera')
camera=bpy.data.objects.new('InspectionCamera',camera_data); scene.collection.objects.link(camera)
camera.location=(100,-125,75)
camera.rotation_euler=(Vector((10,0,-3))-camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.type='ORTHO'; camera_data.ortho_scale=112
scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32
scene.render.resolution_x=1600; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Carbine-preview.png')
scene.view_settings.view_transform='AgX'
bpy.ops.render.render(write_still=True)
