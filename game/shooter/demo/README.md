# First-person demo

Unreal 5.8 single-player first-person controller and firing range. The native
code is in `Source/shooter/demo`; all demo assets are under `Content/demo`.
No marketplace packs or external downloads are required.

## Launch

Open `shooter.uproject`, open `/Game/demo/Maps/DemoRange`, and press Play.
On a fresh checkout, build and generate the assets from PowerShell:

```powershell
.\demo\Build-Demo.ps1 -EngineRoot 'C:\Unreal\UE_5.8' -Open
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
| Scope (hold) | Right mouse | Left trigger |
| Fire (automatic) | Left mouse | Right trigger |
| Reload | R | X / Square |

Shift lowers the weapon; aiming exits sprint. Reload blocks shooting, aiming,
and sprinting. Empty fire starts a reload if reserves remain. Ammo transfers
only when the reload finishes. Targets reset after 2.5 seconds. Restart Play
to refill ammunition and reset the score.

Movement uses softer acceleration/braking. Mouse look has a small exponential
delay that preserves the total input, and normal pitch is the default. Scope
entry/exit uses a 0.32-second smoothstep with additional weapon lag. Crouching
changes the collision capsule with an obstruction check on standing and a
smoothed eye-height transition. The scope has a soft lens edge, fine holdover
reticle and FOV-derived magnification readout.

## Structure and tuning

| Folder | Purpose |
| --- | --- |
| `Source/shooter/demo/Character` | Enhanced Input, locomotion, jump buffer/coyote time, camera and weapon motion |
| `Source/shooter/demo/Weapons` | Fire cadence, two-stage traces, damage, reload state, ammo, procedural rifle |
| `Source/shooter/demo/UI` | Crosshair, scope, hit confirmation, ammo and reload HUD |
| `Source/shooter/demo/World` | Damageable, respawning, optionally moving practice targets |
| `Source/shooter/demo/Game` | Default pawn and HUD selection |
| `Content/demo` | Maps, Blueprint children, materials; reserved audio/animation folders |
| `demo/Scripts` | Repeatable, guarded editor asset generation |

Edit `BP_DemoCharacter` defaults for movement speed, FOV, sensitivity and
`MotionScale` (zero disables bob/sway/landing motion). Edit `BP_DemoRifle` for
damage, magazine/reserves, RPM, reload duration and optional audio assets.
Weapon Blueprint events expose shots and reload transitions for authored VFX,
sounds, and animation montages.

Camera defaults also expose `LookResponse` (higher is more immediate),
`AimTransitionSeconds`, `WeaponResponse`, and `InvertMouseY`. Mouse and gamepad
pitch signs are handled separately to keep both conventional by default.

## Art integration

The shipped rifle is assembled from engine primitives. The camera has an
optional `FirstPersonArms` skeletal mesh component. Import Epic's mannequin or
your own arms, assign a mesh and Animation Blueprint in `BP_DemoCharacter`,
then align the arms to the rifle. No mannequin animations or commercial sounds
are included. The default procedural reload moves the rifle and magazine;
it does not require an animation pack.

This is a polishable gameplay foundation, not a finished AAA asset pack.
It is local single-player: networking, replicated damage, advanced full-body
animation/IK, authored sound design and production weapon art remain separate
work. Debug impact/tracer helpers are intended for development builds.
