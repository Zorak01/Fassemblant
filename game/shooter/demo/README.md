# First-person demo

Unreal 5.8 single-player first-person controller and firing range. The native
code is in `Source/shooter/demo`; all demo assets are under `Content/demo`.
No marketplace packs or external downloads are required.

## Launch

Double-click the repository's `Launch.bat`, then press Enter. Or open
`shooter.uproject`, open `/Game/demo/Maps/TrainingFacility`, and press Play.
The launcher detects the engine and builds/generates only missing pieces:

```powershell
.\tools\Dev.ps1 -Action Play
.\tools\Dev.ps1 -Action VisualTest -NoInstall
```

Requires Visual Studio 2022 C++ tools and a Windows SDK. Setup preserves an
existing range map. Generated binaries and caches are ignored by Git.

## Descending interior cells

Normal play generates a sealed modular interior extending into lower decks.
There are no outdoor rooms, open-sky variants or cosmetic horizon floor. The
runtime world removes the old outdoor architecture and sky sources; saved maps
are preserved. Every cell has an opaque shell, ceiling and connected subrooms.
All procedural geometry uses the cube mesh, including sloped/diagonal modules.
Targets and the grapple head also use angular box meshes.

Default cells are 40 meters wide with a 14-meter deck spacing. A deterministic
4x4 district graph starts with a randomized spanning tree, adds loops and connects
to neighboring districts through shared boundary gates. Closed graph edges are
solid walls. Routes include side rooms, elbows, junctions and branching wings.
Each district combines a 2x2 chamber, up to three two-cell wings and standalone
rooms. Wide openings join cells in one chamber; they share purpose, spatial
grammar and ceiling height. Geometry still streams in bounded cells.

Three dedicated descent columns per district have split floors and switchback
stairs: two walkable diagonal flights and a turning landing. Other rooms have
uninterrupted floors. Stair positions and orientation persist across decks.
Ceiling openings and enclosed shaft liners prevent views into service voids.
Door signs name the destination room; DOWN ROUTE labels follow the shortest
connected path to stairs, including turns around closed walls. The HUD identifies
the current room and deck.

Connections stay opaque and collidable until the neighboring cell exists. When
an outer cell unloads, surviving neighbors close their seals first. Upper and
lower shaft caps prevent views into unloaded space. Deck zero is the sealed top;
new decks continue downward. The working set includes nearby cells above/below
and streams additional geometry with a per-frame budget. World-origin rebasing
includes Z, and patrol/grapple anchors follow those shifts.

There are 24 room purposes: concourse, storage, reactor, server hall, fabrication,
foundry, transfer, combat, hydroponics, laboratory, medical, crew quarters, mess
hall, security, armory, archives, power station, coolant exchange, maintenance,
freight depot, control room, ventilation, training course and checkpoint. Equipment
is built for each purpose, including bunks, growing trays, medical beds, weapon
racks, conveyors and monitoring consoles. Six independent spatial grammars compose
open halls, partitioned suites, galleries, machine bays, diagonal corners and
colonnades. Ceiling heights range from 6.5 to 11 meters, limited by deck clearance.
Furniture variation is seeded separately per cell. There are no spheres or
cylinders in generated architecture. `RoomPalette` reuses existing materials and
interior area lights provide illumination.

Tune `WorldSeed`, `ChunkSize`, `LayerHeight`, `LoadRadius`, `LoadAbove`, `LoadBelow`
and `ChunksPerUpdate` on `ADemoProceduralEnvironment`. Cells regenerate from their
XYZ coordinate and seed. This is a local single-player system. Set the game
mode's `bInfiniteEnvironment=false` or pass `-DemoFiniteRange` to use the retained
legacy facility; its location-specific presentation tests explicitly do so.

`Demo.Gameplay.Environment.*` covers deck addressing, deterministic regeneration,
stair alignment, floor openings, walkable ramp angles, bounded box modules,
reciprocal graph edges, connected districts, branches/loops, coherent multi-cell
chambers, descent-sign routing and player clearance to every doorway.
Compilation alone does not execute those tests or verify the rendered interior.

The opt-in `-DemoEnvironmentTest` game argument runs a separate rendered traversal
check in the generated world: six room captures, floor/ceiling traces, three
physical doorway crossings and the complete switchback to the next deck. Run it
with `UnrealEditor-Cmd.exe`, the project/map, `-game -RenderOffscreen -unattended`
and `-DemoEnvironmentTest`. Screenshots and `report.txt` are written beneath
`Saved/Automation/EnvironmentPlay`; the process exits with a failure code if a
check fails. The room palette assets have persisted instanced-mesh shader usage;
`prepare_environment_materials.py` repairs older palettes without recreating maps.

## Controls

| Action | Keyboard / mouse | Gamepad |
| --- | --- | --- |
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Run (hold, forward) | Left Shift | Left stick click |
| Jump (variable height) | Space | A / Cross |
| Crouch (hold) | Ctrl or C | Right stick click |
| Slide from sprint | Shift + Ctrl / C | Sprint + crouch |
| Jump out of slide | Space | A / Cross |
| Fire/release grapple | Q | Left bumper / L1 |
| Steer while grappling | WASD + look | Left stick + look |
| Aim down the physical sight (hold) | Right mouse | Left trigger |
| Fire (automatic) | Left mouse | Right trigger |
| Reload | R | X / Square |
| Refill ammo and reset targets/statistics | E | â€” |
| Start/restart 60-second drill | T | â€” |

## Grapple

Q launches toward the aimed solid surface and remains attached after the key is
released. Q again cuts the rope. A short hook flight precedes attachment; valid
anchors get reticle brackets and a distance/readiness indicator. The initial
defaults are a 28-meter range, 19 m/s winch speed and a five-second cooldown.
These are project tuning values, not a claim of matching Apex's internal values.

The winch engages with a 4.5 m/s inward impulse, accelerates at up to 120 m/s²,
and compensates for gravity along the pull direction. Its speed envelope is
32 m/s. The retracting rope preserves tangential velocity. WASD steers in the rope's
tangent plane; looking and holding forward also permits vertical swing control.
A jump within 0.18 seconds of latching adds a hop; a later jump releases. Cutting
the rope retains velocity into normal air movement. Holding crouch for landing
can chain into a slide without clamping fast grapple momentum to sprint speed.
Weapon aiming and firing remain available while attached. There is no forced
camera turn or interpolation of the player along a straight line.

The hook releases on arrival, timeout, sustained look-away, blocked rope, or an
anchor becoming invalid/non-collidable. Moving anchors use component-local latch
coordinates, and cached positions shift during procedural world rebasing. E resets
the grapple cooldown with the range. Components/actors tagged `NoGrapple` reject
attachment. Missed shots do not consume the full cooldown.

`UDemoGrappleComponent` owns targeting, flight, the engine CableComponent visual
and state. `UDemoMovementComponent` integrates a swept, substepped custom movement
mode. `DemoGrapplePhysics` supplies the pure rope step used by regression tests.
Pull, steering, gravity, speed, hop and release tuning are exposed on those
components. Existing project sounds provide launch/latch cues. This remains a
local single-player ability. `Demo.Gameplay.Grapple.*` covers rope tension, steering,
integration, origin-shift invariance, release and slide momentum. The grapple was
compiled without opening Unreal; runtime/collision/cable appearance and precise
feel still require an in-game tuning pass.

Shift lowers the weapon; aiming exits sprint. Reload blocks shooting, aiming,
and sprinting. Empty fire starts a reload if reserves remain. Ammo transfers
only when the reload finishes. Facility targets reset after 1.8 seconds; head
hits deal double damage. E resets the range and gives 600 reserve rounds.

The facility has precision lanes, moving-target tracks, standing-height slide
gates, an acceleration ramp, jump platforms and cover. The original `DemoRange`
map is preserved. Normal setup never overwrites an existing facility; the
authoring script's explicit `-DemoRebuildTraining` flag rebuilds its layout.

`UDemoMovementComponent` retains Unreal's floor/collision physics while applying
slide-specific friction, limited lateral steering, downhill acceleration and
an entry boost with a cooldown. Forward input cannot accelerate a slide. Jumping
preserves momentum; holding crouch through descent can chain a landing slide.
Holding Shift resumes sprint when crouch is released. Slide tuning is exposed
on the character's movement component.

The outlined reticle projects the weapon's actual spread cone, including speed,
crouch, air state and shot bloom. It pulses on fire, fades for sprint/reload/ADS,
and adds fading hit markers and critical-damage readouts. The range panel reports
accuracy, applied damage per second, kills and headshots; T starts a timed trial.

Thirteen original PCM cues under `assets/demo/Audio/Training` add layered firing,
reload and empty mechanisms, four footstep variations, landing, slide onset/loop,
spatial impacts and separate hit/kill confirmations. Camera shot impulses,
sprint gait, landing and slide feedback are cosmetic and do not steer traces.

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

The active first-person mesh is an arms-only derivative of Epic's original
`SKM_Manny_Simple`, retaining its skinning, textures, skeleton and rifle clips.
Dedicated opaque material derivatives remove the third-person camera-distance
cutout that otherwise erases parts of the wrists and forearms near the eye.
The source mannequin and its materials remain unchanged. Regenerate the viewmodel
with `demo/Scripts/prepare_viewmodel_arms.py` (`-DemoRebuildArms` rebuilds the geometry).
Only the 16 selected packages and their hard dependencies are copied from the
installed UE 5.8 template resources into `Content/Characters`. Head and legs
are hidden at runtime. No First Person Sample project is used.

`UDemoArmsAnimInstance` samples weapon palm targets on the game thread, then
evaluates the authored rifle clips, anchors the original firing arm to the
weapon, then solves support-arm IK using the authored elbow position outside reload. Mesh-space
additive firing is accumulated over the rifle idle pose. Palm-space calibration
handles Manny's mirrored bone axes; fingers retain the authored pose. The support
hand transitions from the foregrip to the moving magazine and charging handle
during reload. Recoil, ADS, sprint and idle motion move the rifle and both grip
targets together on a regular `USkeletalMeshComponent`.

The replacement carbine's editable source is `assets/demo/Weapons/Carbine.blend`.
`build_carbine.py` builds beveled, machined surfaces and exports five mechanical
groups. `demo/Scripts/import_carbine_v2.py` imports them and assigns the materials
and physical sight. The optic has a real opening; the reticle is weapon geometry.

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

Movement cues use unpitched heel/sole/toe layers and an overlapped friction loop.
Footstep selection avoids immediate repeats; sliding suppresses footsteps and
fades its quieter loop in/out. To regenerate existing cues deliberately, run
`create_training_audio.py` with the editor command-line flag `-DemoRebuildAudio`.
Normal setup preserves existing audio assets.

The rail has an integral handguard saddle and ends behind its front face.
Reload samples the original rifle reload clip, anchoring the full firing palm
rotation and position to the weapon. Support-arm IK fades out while the clip
articulates the elbow, wrist and fingers. After bone evaluation, a finalized-pose
callback attaches the magazine to the animated hand during contact on the same
frame. The actual pickup grip is calibrated as contact begins; the magazine
returns to its seated pose before the final weapon manipulation.
When the third-person clip reaches behind the eye, the complete support-arm
branch moves forward while preserving its authored forearm, wrist and finger
articulation. The shoulder stays outside the view, with a continuous upper arm.
The final wrist rotation relative to the forearm is checked against the source
clip, alongside visual inspection of the rendered phases.

The player camera overrides its near plane to 0.3 cm. The editor/global setting
stays at 3 cm; with native first-person scale 0.3, that global plane otherwise
clips viewmodel geometry within 10 authored centimeters. Presentation checks
measure rendered hand depth against the active camera near plane and save
intermediate reload PNGs with bone-transform CSVs alongside the phase captures.

The carbine receiver uses a tapered eight-sided profile, recessed machining
details and darker, rougher ceramic/metal surfaces. Gait uses a smooth cosine
plant/rebound with reduced camera amplitude instead of a cusp at every footfall.

## Verification

The launcher runs `Demo.Gameplay` for ammo/reload, damage/reset, raw input,
recoil direction/recovery and crouch collision. Its **V / VisualTest** action
loads the real range in a standalone rendered game, drives ADS/firing/reload,
sprint/jump/strafe, checks sight and hand/palm alignment, and saves phase PNGs
plus a JSON report under `Saved/Automation/Presentation`. It uses the renderer
directly, so desktop screenshot capture is not required. Inspect those images
alongside the numeric checks when changing animation or art.
