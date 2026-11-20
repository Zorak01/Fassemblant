"""Repair/setup training content; shipped assets let normal launches skip this."""
from pathlib import Path
import json
import runpy
import unreal

scripts=Path(__file__).resolve().parent
for name in ['import_carbine_v2.py','create_training_audio.py','create_training_range.py']:
    runpy.run_path(str(scripts/name))
manifest=scripts.parent/'FirstPersonAssets.json'
required=json.loads(manifest.read_text(encoding='utf-8'))['assets'] if manifest.exists() else []
required=[s for s in required if not s.startswith('/Game/demo/Maps/')]
content=Path(unreal.Paths.project_content_dir()).resolve()
for folder in ['demo/Audio/Training','demo/Weapons/CarbineV2','Characters']:
    required += ['/Game/'+p.relative_to(content).with_suffix('').as_posix() for p in (content/folder).rglob('*.uasset')]
required += ['/Game/demo/Materials/'+p.stem for p in (content/'demo/Materials').glob('M_Training*.uasset')]
required += ['/Game/demo/Materials/M_BallisticMark']
manifest.write_text(json.dumps({'schema':1,'assets':sorted(set(required))},indent=2)+'\n',encoding='utf-8')
unreal.log('TRAINING_SETUP_COMPLETE')
