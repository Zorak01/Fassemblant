# First-person demo

Unreal 5.8 single-player first-person controller and firing range. The native
code is in `Source/shooter/demo`; all demo assets are under `Content/demo`.
No marketplace packs or external downloads are required.

## Launch

Double-click the repository's `Launch.bat`, then press Enter. Or open
`shooter.uproject`, open `/Game/demo/Maps/DemoRange`, and press Play.
The launcher detects the engine and builds/generates only missing pieces:

```powershell
.\tools\Dev.ps1 -Action Play
.\tools\Dev.ps1 -Action VisualTest -NoInstall
```

Requires Visual Studio 2022 C++ tools and a Windows SDK. Setup preserves an
existing range map. Generated binaries and caches are ignored by Git.

## Controls

| Action | Keyboard / mouse | Gamepad |
| --- | --- | --- |
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Run (hold, forward) | Left Shift | Left stick click |
| Jump (variable height) | Space | A / Cross |
| Crouch (hold) | Ctrl or C | Right stick click |
| Aim down the physical sight (hold) | Right mouse | Left trigger |
| Fire (automatic) | Left mouse | Right trigger |
| Reload | R | X / Square |

Shift lowers the weapon; aiming exits sprint. Reload blocks shooting, aiming,
and sprinting. Empty fire starts a reload if reserves remain. Ammo transfers
only when the reload finishes. Targets reset after 2.5 seconds. Restart Play
to refill ammunition and reset the score.

Movement uses softer acceleration/braking. Captured relative mouse input is
applied immediately with no smoothing buffer, acceleration or hidden axis gain.
ADS entry/exit uses a 0.16-second smoothstep with responsive weapon motion. Crouching
changes the collision capsule with an obstruction check on standing and a
smoothed eye-height transition. ADS aligns the rifle using its authored sight
transform and eye relief. The red dot is an emissive mesh mounted in the optic.
The rifle stays visible; there is no scope mask, screen reticle or scope overlay.

## Structure and tuning

| Folder | Purpose |
| --- | --- |
| `Source/shooter/demo/Character` | Input, locomotion, motion springs, native mannequin animation/IK |
| `Source/shooter/demo/Weapons` | Gameplay, mechanical animation, replaceable weapon visual data |
| `Source/shooter/demo/UI` | Hip crosshair, hit confirmation, ammo and reload HUD |
| `Source/shooter/demo/World` | Damageable, respawning, optionally moving practice targets |
| `Source/shooter/demo/Game` | Default pawn and HUD selection |
| `Source/shooter/demo/Editor` | Local mannequin derivation and mesh authoring tools |
| `Source/shooter/demo/Tests` | Gameplay tests and opt-in rendered standalone play test |
| `Content/demo` | Map, Blueprints, Kestrel meshes/data, skinned mannequin arms and audio |
| `demo/Scripts` | Repeatable, guarded editor asset generation |

Edit `BP_DemoCharacter` defaults for movement speed, FOV and
`MotionScale` (zero disables bob/sway/landing motion). Edit `BP_DemoRifle` for
damage, magazine/reserves, RPM, reload duration and optional audio assets.
Weapon Blueprint events expose shots and reload transitions for authored VFX,
sounds, and animation montages.

Camera defaults expose `AimTransitionSeconds` and `WeaponResponse`. Mouse input
settings are per-user and persist across runs. Open Unreal's console with `~`:

```text
DemoSensitivity 0.10
DemoADSSensitivity 0.22
DemoInvertMouse false
DemoInputSettings
```

Sensitivity is degrees per raw mouse count; ADS is an explicit multiplier, with
no extra FOV multiplier. Unity mouse axis gain replaces the previous 0.07 scale,
so default hip input is approximately 12 times stronger than the old effective
gain. The view rotates on the same input frame. Only cosmetic sway and poses
are interpolated.

Aim recoil uses a repeatable editable pattern (`X` yaw, `Y` upward world pitch),
with a short post-burst recovery delay. Counter-steering consumes the remaining
recoverable kick so recovery does not undo deliberate compensation. The weapon
has an independent critically damped visual kick, applied after pose smoothing.
Traces use control rotation, never the cosmetic weapon transform. ADS reduces
spread and recoil without adding movement-based aiming lag.

The reference principles are configurable sensitivity/ADS with acceleration off
([Apex's official settings](https://www.ea.com/able/resources/apex-legends/pc/features))
and coherent weapon mechanics, silhouette and firing/reload feedback
([Respawn's weapon-design interview](https://news.xbox.com/en-us/2014/02/13/games-titanfall-weapons/)).
Our recoil/animation implementation is an adaptation of those priorities, not a
claim to reproduce either game's proprietary tuning or code.

## Connected weapon and arms animation

`DA_Kestrel` describes the custom shaped weapon meshes, materials, sight, eye
relief and palm targets. Magazine, bolt carrier, charging handle, trigger and
muzzle flash have separate attachment groups driven by the same fire/reload
timeline as gameplay. Changing a grip/sight in the data asset changes the
corresponding hand/ADS alignment. The asset can be replaced with authored art
without changing movement or weapon gameplay.

`SK_DemoArms` is an arms-only derivative of Epic's bundled Manny Simple mesh.
It preserves the mannequin skeleton, skin weights and finger hierarchy. Body,
head and leg triangles are removed from each source LOD. Local skeleton and
materials are used so the Mover plugins do not need to be enabled at runtime.
No First Person Sample project or assets are used.

`UDemoArmsAnimInstance` samples weapon palm targets on the game thread, then
solves two-bone IK during animation evaluation. Palm-space calibration handles
Manny's mirrored bone axes; finger curl follows the palm plane. The support
hand transitions from the foregrip to the moving magazine and charging handle
during reload. Recoil, ADS, sprint and idle motion move the rifle and both grip
targets together. This is a native procedural animation layer on a regular
`USkeletalMeshComponent`; it can be replaced/extended with authored mannequin
Animation Blueprints and clips.

The first-person mesh and weapon use Unreal's native first-person FOV/scale
rendering to reduce wall clipping. The world and viewmodel FOVs converge in ADS.
See [Epic's first-person rendering documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-rendering).

## Movement weight

`UDemoMotionComponent` uses distance-traveled gait, acceleration-driven weapon
inertia and a critically damped landing spring. Sprint increases bob and roll;
direction changes and stops shift the weapon. Footsteps use the same stride
distance as the gait. ADS settles these effects so the sight remains readable.
Higher gravity and a shorter jump hold remove the long floaty apex. Motion is
deterministic, with no random shake. `MotionScale=0` disables cosmetic motion.

The rifle and audio are created by this project. Audio is synthesized placeholder
sound design, not sampled from either reference game. Epic mannequin derivatives
remain Epic Unreal Engine content. Networking, a full third-person body, authored
cinematic animation clips, and production sound design remain future work.

## Verification

The launcher runs `Demo.Gameplay` for ammo/reload, damage/reset, raw input,
recoil direction/recovery and crouch collision. Its **V / VisualTest** action
loads the real range in a standalone rendered game, drives ADS/firing/reload,
sprint/jump/strafe, checks sight and hand/palm alignment, and saves seven PNGs
plus a JSON report under `Saved/Automation/Presentation`. It uses the renderer
directly, so desktop screenshot capture is not required. Inspect those images
alongside the numeric checks when changing animation or art.
