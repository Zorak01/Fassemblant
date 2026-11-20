"""Author original layered PCM cues. No audio taken from commercial games."""
from pathlib import Path
import array
import math
import random
import wave
import unreal

ROOT = Path(__file__).resolve().parents[4] / 'assets/demo/Audio/Training'
ROOT.mkdir(parents=True, exist_ok=True)
RATE = 44100
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
REBUILD = '-DemoRebuildAudio' in unreal.SystemLibrary.get_command_line()

def cue(name, kind, duration, variant=0, loop=False):
    destination='/Game/demo/Audio/Training/'+name
    if assets.does_asset_exist(destination) and not REBUILD: return unreal.load_asset(destination)
    rng=random.Random(3179+variant*173+sum(map(ord,name)))
    count=int(RATE*duration)
    samples=[]; low=0.; slow=0.; body=0.; grit=0.
    for i in range(count):
        t=i/RATE; n=rng.uniform(-1,1)
        low+=(n-low)*.14; slow+=(n-slow)*.018
        if kind=='fire':
            # Sharp high-frequency crack, pitched pressure body, bolt travel and
            # a restrained early-reflection tail. Attack stays under one ms.
            v=(n-low)*.6*math.exp(-t*340)+low*3.8*math.exp(-t*65)
            v+=math.sin(2*math.pi*(105*t-45*t*t))*.85*math.exp(-t*26)
            for onset,gain,decay in [(.026,.5,170),(.075,.23,150),(.115,.10,70),(.19,.06,45)]:
                d=t-onset
                if d>=0: v+=(n*.4+math.sin(d*6500)*.25)*gain*math.exp(-d*decay)
            v+=slow*.9*math.exp(-t*12)
        elif kind=='step' or kind=='land':
            # Three overlapping sole contacts: dull heel, broad weight transfer,
            # then a quiet toe scuff. No pitched drum/sine in the footfall.
            body+=(n-body)*.035
            grit+=(n-grit)*.38
            heavy=kind=='land'
            heel=math.exp(-t*(38 if heavy else 65))*(1-math.exp(-t*900))
            d=max(0.,t-(.025+variant*.002))
            sole=math.exp(-d*(25 if heavy else 48))*(1-math.exp(-d*230))
            d=max(0.,t-(.075+variant*.004))
            toe=math.exp(-d*38)*(1-math.exp(-d*110))
            v=body*(5.0 if heavy else 3.5)*heel+(low-body)*1.2*sole
            v+=(grit-low)*.24*toe
        elif kind=='slide':
            body+=(n-body)*.045
            grit+=(n-grit)*.32
            # Cloth/sole friction with slow, irregular contact pressure.
            pressure=.65+.16*math.sin(2*math.pi*t/duration*3)+.09*math.sin(2*math.pi*t/duration*7)
            v=((low-body)*1.1+(grit-low)*.16)*pressure
        elif kind=='slide_start':
            body+=(n-body)*.04
            attack=1-math.exp(-t*180)
            v=(body*2.4*math.exp(-t*28)+(low-body)*1.1*math.exp(-t*12))*attack
        elif kind=='reload':
            v=0.
            for onset,gain in [(.1,.8),(.48,.45),(1.15,.7),(1.57,.5),(1.72,.9)]:
                d=t-onset
                if d>=0: v+=(low*2+n*.12+math.sin(d*7100)*.25)*math.exp(-d*90)*gain
        elif kind=='impact':
            v=(n*.4+low*.8+math.sin(t*7900)*.22+math.sin(t*12600)*.1)*math.exp(-t*58)
        elif kind=='empty':
            v=(n*.3+math.sin(t*3800)*.4)*math.exp(-t*170)
        elif kind=='hit':
            v=(math.sin(t*2*math.pi*1450)*.28+math.sin(t*2*math.pi*2100)*.12)*math.exp(-t*48)
            v+=n*.15*math.exp(-t*140)
        elif kind=='kill':
            v=math.sin(t*2*math.pi*1100)*.28*math.exp(-t*25)
            if t>.065: v+=math.sin((t-.065)*2*math.pi*1750)*.35*math.exp(-(t-.065)*28)
        else: v=0.
        v*=min(1.,t*3500) if not loop else 1.
        if not loop: v*=min(1.,(duration-t)*150)
        samples.append(math.tanh(v)*.85)
    if loop:
        # Overlap the tail into the head; avoid a rhythmic dip to silence.
        overlap=int(RATE*.12)
        for i in range(overlap):
            alpha=.5-.5*math.cos(math.pi*i/(overlap-1))
            samples[i]=samples[count-overlap+i]*(1-alpha)+samples[i]*alpha
        samples=samples[:-overlap]
    pcm=array.array('h',(int(v*32767) for v in samples))
    source=ROOT/(name+'.wav')
    with wave.open(str(source),'wb') as out:
        out.setnchannels(1); out.setsampwidth(2); out.setframerate(RATE); out.writeframes(pcm.tobytes())
    task=unreal.AssetImportTask(); task.filename=str(source); task.destination_path='/Game/demo/Audio/Training'
    task.destination_name=name; task.automated=True; task.save=True
    task.replace_existing=True
    tools.import_asset_tasks([task])
    sound=unreal.load_asset(destination)
    if not sound: raise RuntimeError('Audio import failed: '+name)
    if loop: sound.set_editor_property('looping',True)
    assets.save_loaded_asset(sound)
    return sound

sounds={}
for name,kind,duration in [('S_R17Shot','fire',.65),('S_R17Reload','reload',1.9),('S_R17Empty','empty',.09),('S_HitConfirm','hit',.12),('S_KillConfirm','kill',.29),('S_BallisticImpact','impact',.19),('S_HeavyLand','land',.32),('S_SlideStart','slide_start',.4)]:
    sounds[name]=cue(name,kind,duration)
sounds['S_SlideLoop']=cue('S_SlideLoop','slide',1.5,loop=True)
steps=[cue('S_ConcreteStep_%d'%i,'step',.24,i) for i in range(4)]
bp=unreal.load_asset('/Game/demo/Weapons/BP_DemoRifle')
weapon=unreal.get_default_object(unreal.load_class(None,bp.get_path_name()+'_C'))
for prop,name in [('fire_sound','S_R17Shot'),('reload_sound','S_R17Reload'),('empty_sound','S_R17Empty'),('hit_sound','S_HitConfirm'),('kill_sound','S_KillConfirm'),('impact_sound','S_BallisticImpact')]: weapon.set_editor_property(prop,sounds[name])

decal_path='/Game/demo/Materials/M_BallisticMark'
decal=unreal.load_asset(decal_path)
if not decal:
    decal=tools.create_asset('M_BallisticMark','/Game/demo/Materials',unreal.Material,unreal.MaterialFactoryNew())
    decal.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    decal.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    edit=unreal.MaterialEditingLibrary
    uv=edit.create_material_expression(decal,unreal.MaterialExpressionTextureCoordinate,-600,0)
    center=edit.create_material_expression(decal,unreal.MaterialExpressionConstant2Vector,-600,100)
    center.set_editor_property('r',.5); center.set_editor_property('g',.5)
    mask=edit.create_material_expression(decal,unreal.MaterialExpressionSphereMask,-350,0)
    mask.set_editor_property('attenuation_radius',.45); mask.set_editor_property('hardness_percent',60.)
    edit.connect_material_expressions(uv,'',mask,'A'); edit.connect_material_expressions(center,'',mask,'B')
    edit.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
    base=edit.create_material_expression(decal,unreal.MaterialExpressionConstant3Vector,-350,200)
    base.set_editor_property('constant',unreal.LinearColor(.009,.012,.014))
    edit.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    edit.recompile_material(decal); assets.save_loaded_asset(decal)
weapon.set_editor_property('impact_decal',decal)
if not assets.save_loaded_asset(bp,only_if_is_dirty=False): raise RuntimeError('Cannot save rifle audio. Check out BP_DemoRifle.')
bp=unreal.load_asset('/Game/demo/Character/BP_DemoCharacter')
character=unreal.get_default_object(unreal.load_class(None,bp.get_path_name()+'_C'))
motion=character.get_editor_property('motion')
motion.set_editor_property('footstep_variations',steps)
motion.set_editor_property('landing_sound',sounds['S_HeavyLand'])
motion.set_editor_property('slide_start_sound',sounds['S_SlideStart'])
motion.set_editor_property('slide_loop_sound',sounds['S_SlideLoop'])
if not assets.save_loaded_asset(bp,only_if_is_dirty=False): raise RuntimeError('Cannot save movement audio. Check out BP_DemoCharacter.')
unreal.log('TRAINING_AUDIO_READY: 13 original layered cues')
