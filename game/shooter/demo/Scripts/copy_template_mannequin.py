"""Copy only the Epic mannequin/rifle packages and their hard dependencies.
The First Person Sample project is never opened or imported.
"""
from pathlib import Path
import shutil
import unreal

unreal.DemoEditorLibrary.mount_template_resources()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/Characters/'], force_rescan=True)
requested = [
    '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS',
    '/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire',
    '/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload',
]
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=False, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
pending = list(requested)
packages = set()
while pending:
    package = str(pending.pop())
    if package in packages or not package.startswith('/Game/Characters/'):
        continue
    packages.add(package)
    pending.extend(registry.get_dependencies(package, options))
source = Path(unreal.Paths.engine_dir()).resolve().parent / 'Templates/TemplateResources/High/Characters/Content'
content = Path(unreal.Paths.project_content_dir()).resolve()
for package in sorted(packages):
    relative = package.removeprefix('/Game/Characters/')
    original = source / (relative + '.uasset')
    target = content / 'Characters' / (relative + '.uasset')
    if not original.is_file():
        raise RuntimeError('Missing required Epic template package: ' + str(original))
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, target)
unreal.log('EPIC_RIFLE_PACKAGES_READY: %d selected packages/dependencies' % len(packages))
