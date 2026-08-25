# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DaeRune is a multiplayer cooperative PvE action game built with Unreal Engine 5.5, using the Gameplay Ability System (GAS) for combat mechanics. The game features a 2-phase mission structure: players first secure a Cleanser Site and install parts, then defend it against enemy waves.

## Building and Running

### Build Commands
- **Generate Project Files**: Right-click `DaeRune.uproject` → "Generate Visual Studio project files"
- **Build from Visual Studio**: Open `DaeRune.sln` and build the "DaeRune" project (Development Editor configuration)
- **Build from Unreal Editor**: Open `DaeRune.uproject`, Editor will compile on launch if needed

### Running
- **Editor**: Launch `DaeRune.uproject` with Unreal Editor 5.5
- **Play in Editor (PIE)**: Use Editor's Play button, select "Net Mode" for multiplayer testing
- **Standalone**: Package via File → Package Project → Windows

### Testing Multiplayer
- In Editor: Play → Number of Players → Set to 2+ for local multiplayer testing
- Use "Listen Server" for hosting, additional instances auto-connect as clients
- TestMap1.umap is the primary development map

## Core Architecture

### Phase System
The game progresses through a phase-based mission structure managed by `ADRStageGameMode`:

**Phase Hierarchy** (2 phases; class names are legacy and do NOT match runtime phase indices):
- `UDRPhaseBase` (abstract) - Base class with lifecycle methods (`Initialize`, `OnPhaseStart`, `OnPhaseEnd`)
- `UDRPhase1` (phase index 0) - Secure the single Cleanser Site AND collect/install 2 parts (secure + collect merged)
- `UDRPhase3` (phase index 1) - Defend the site by clearing all waves (acts as the "new Phase 2")
- `UDRPhase2` - Legacy, unused in the current 2-phase structure (part collection was merged into `UDRPhase1`)

Completion conditions live in `ADRStageGameMode::ValidatePhaseCompletion()` (case 0: 2 parts installed + site activated, case 1: all waves cleared).

**Key Patterns**:
- GameMode orchestrates phase transitions via `TransitionToNextPhase()`
- GameState (`ADRStageGameState`) replicates phase data to all clients
- Each phase manages enemy spawning using `TWeakObjectPtr<AActor>` arrays
- CleanserSite actors persist across phases, changing state (Inactive → Active → Operational)

### Gameplay Ability System (GAS)

**Ownership Model**:
- PlayerState owns `UAbilitySystemComponent` for players (persists through respawn)
- Enemies own ASC directly on character
- CleanserSites create ASC in constructor (Minimal replication mode); its AttributeSet is only used for health tracking in Phase 3

**Attribute Sets**:
- `UDRPlayerAttributeSet` - Container health system, corruption mechanics
- `UDREnemyAttributeSet` - Enemy health, water rewards
- `UDRCleanserSiteAttributeSet` - Cleanser Site health (created at construction, used in Phase 3 only)

**Custom Systems**:
- Water resource parallel to health (used for ability costs via `ExecCalc_WaterCost`)
- Tag-driven debuff system (fire, lightning, arcane, physical, bleed)
- Container-based damage for players (4 containers, overflow between containers)

### Character Hierarchy
```
ADRCharacterBase (implements IAbilitySystemInterface, ICombatInterface)
├── ADRCharacter - Players with container health, corruption, part carrying
└── ADREnemy - AI with water rewards, part drops, wall stun mechanics
```

**Key Features**:
- Replicated debuff states with Niagara VFX (stun, burn, shock)
- Multicast death handling for synchronized effects
- Tagged animation montages for abilities
- Move speed bound to attributes via CharacterMovement component

### UI System (MVC Pattern)
```
ADRHUD
├── Creates UOverlayWidgetController
│   ├── Observes ADRStageGameState (phase objectives)
│   ├── Observes UAbilitySystemComponent (attributes)
│   └── Broadcasts to UDRUserWidget (Blueprint)
└── Manages widget lifecycle
```

**Important**: Widget controllers are cached singletons. Always use `ADRHUD::GetOverlayWidgetController()` to access.

### Multiplayer Architecture

**Authority Distribution**:
- Server: Phase logic, enemy spawning, ability validation, part installation
- Client: Input prediction, VFX, UI updates

**Replication Patterns**:
- Property replication with RepNotify for state changes
- Server RPCs for player requests (pickup part, install part)
- Multicast RPCs for synchronized effects (death, VFX)

**Team System**:
- Players (Team 1) via `ADRPlayerController` implementing `IGenericTeamAgentInterface`
- AI (Team 2) via `ADRAIController`

## Gameplay Tag Structure

Tags are centrally managed in `FDRGameplayTags` singleton. Initialize via `InitializeNativeGameplayTags()`.

**Critical Tag Categories**:
- `Cost.Water` - Water resource costs
- `InputTag.*` - Input bindings (LMB, RMB, Q, E)
- `Damage.*` - Damage types (Fire, Lightning, Arcane, Physical, Bite)
- `Debuff.*` - Status effects with meta tags (Chance, Damage, Duration)
- `Abilities.*` - Ability identification and categorization
- `CombatSocket.*` - Animation socket locations (Weapon, RightHand, LeftHand, Tail)
- `State.*` - Player states (Corrupt, Carrying)

**Tag-to-Attribute Mapping**: Use `DamageTypesToDebuffs` map for automatic debuff application in `ExecCalc_Damage`.

## Key Subsystems

### CleanserSite Actor System
`ADRCleanserSite` actors are discovered automatically by "CleanserSite" actor tag in `ADRStageGameMode::BeginPlay()`.

**State Machine**: Inactive → Active → PartsCollected → Operational → Completed

**Phase-Specific Behavior**:
- Secure phase (`UDRPhase1`): a single site is kept and the others are destroyed (`UDRPhase1::KeepSingleCleanserSite()`); the remaining site is used for enemy spawning and accepts part installation (2 parts), triggering completion delegates
- Defense phase (`UDRPhase3`): Health attributes initialized (ASC exists from construction), become damageable, broadcast health thresholds

### Part Collection System (secure phase, `UDRPhase1`)
`ADRCleanserPart` lifecycle:
1. Attached to enemy as PartMeshComponent
2. On enemy death, spawned at a ground point found by downward line trace +80uu (`ADREnemy::DropPart()`, no physics impulse)
3. Player detection via line trace (`PlayerController::FindPartByLineTrace()`)
4. Server-authoritative pickup via `ServerRequestPickupPart()`
5. Attached to player hand socket, disables move speed
6. Installation at CleanserSite via overlap + interact key

Carrying state (`bIsCarryingPart` + `State.Carrying` tag + visuals) is toggled server-side only through `ADRCharacter::SetCarryingState()`; clients sync the tag in `OnRep_bIsCarryingPart`. Part/Site interaction UI is local-only via the `IDRInteractable` interface (no RPCs).

**Important**: Detection uses line trace, not collision, for precision.

### Wave System (defense phase, `UDRPhase3`)
The phase completes when all waves (`TotalWaves`) are cleared — there is no fixed defense timer. Wave levels (1-5) exist for difficulty scaling, but the escalation trigger (CleanserSite health below 50%) is currently commented out/disabled in `DRPhase3.cpp`.

**Wave Data Structure**: `FWaveData` with PlayDuration, RestDuration, SpawnInterval, MonstersPerPlayer

**Modifiers Per Level**: MonsterCountMultiplier, SpawnIntervalMultiplier, special enemy chances, environmental hazards, elite boss

**Spawning**: Enemies spawn around players (500-2000 units away), limited to 100 total (triggers game over)

### Water Resource System
Unique resource system for ability costs:

**Sources**:
- Enemy kills grant water in AoE radius (500 units, or global for bosses)
- Reduced per enemy attack (prevents farming same enemy)

**Uses**:
- Ability costs via `WaterCost` property on `UDRGameplayAbility`
- Cost validation in `CheckCost()`, execution via `ApplyCost()` using `ExecCalc_WaterCost`

**Container Integration**: Water affects corruption state in container health system

### Combat State Management
`ADRPlayerState` tracks combat state for health regeneration:

**State Machine**: Normal → (Take Damage) → In Combat → (10s) → Normal → (Auto Heal)

**Health Regen**: Applies infinite duration GE with `MMC_HealthRegen` calculation, removed on combat entry

### Debuff System
Tag-driven automatic debuff application:

1. Damage tagged with type (e.g., `Damage.Fire`)
2. `ExecCalc_Damage` reads chance/damage/duration from effect context
3. Maps damage type to debuff effect via `DebuffEffectMap`
4. Applies debuff GE with replicated cues
5. Character shows Niagara VFX via RepNotify (`OnRep_Stunned`, `OnRep_Burned`)

**Debuff Effects**: Burn (DoT), Stun (immobilize), Arcane (slow), Physical (armor break), Bleed (health drain)

## Important Implementation Details

### Container Health System
Players have 4 health containers (100 HP each by default):

**Damage Handling**:
- Damage current container first
- If damage > 10% of current container health, overflow to next container
- Corruption state bypasses containers, damages health directly with 1.5x elite modifier

**Corruption**:
- Triggered when water depletes completely
- Affects damage taken, team visibility, communication
- Can be purified via excess healing

### Wall Stun Mechanic (Enemies)
Enemies can be stunned by knockback into walls:

**Requirements**:
- Velocity >= `MinSpeedForStun` (typically from knockback abilities)
- Hit actor tagged "Wall" or similar
- Not currently stun-immune

**Execution**: `ADREnemy::OnHit()` callback applies stun GE via ASC

### Character Class System
Two separate class enums (both in `AbilitySystem/Data/CharacterClassInfo.h`):

- **Players** use `EPlayerCharacterClass` (Gardener, VendingMachine), initialized via `UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes()` with player class info data
- **Enemies** use `ECharacterClass` (Elementalist, Warrior, Ranger, Bear, PartEnemy) with the `UCharacterClassInfo` data asset

**Per-Class Data**:
- Primary Attributes GE (MaxHealth, MaxWater, MoveSpeed)
- Vital Attributes GE (initializes Health/Water)
- StartupAbilities array

**Initialization Flow**: `UDRAbilitySystemLibrary::InitializeDefaultAttributes()` (enemies) / `InitializePlayerDefaultAttributes()` (players) → Lookup class info → Apply GEs → Grant abilities

### Input System
Enhanced Input System with GAS integration:

**Binding Pattern**:
1. `UDRInputConfig` data asset maps InputTags to InputActions
2. PlayerController binds actions to Started/Triggered/Completed events
3. Events call `ASC::AbilityInputTagPressed/Held/Released()`
4. ASC iterates ability specs, activates matching abilities

**Important**: Input tags must match `StartupInputTag` on ability classes.

## Common Development Patterns

### Adding a New Ability
1. Create ability class inheriting from `UDRGameplayAbility` or subclass
2. Set `StartupInputTag` to match input config (e.g., `InputTag.Q`)
3. Set `WaterCost` if applicable
4. Override `ActivateAbility()` for ability logic
5. Add to character class info's StartupAbilities or CommonAbilities array
6. Create GE for damage/healing if needed with appropriate tags

### Adding a New Phase
1. Create class inheriting from `UDRPhaseBase`
2. Override `OnPhaseStart()` for initialization logic
3. Override `OnPhaseEnd()` for cleanup
4. Implement enemy spawning using `SpawnedEnemies` tracking array
5. Bind to enemy death delegate for progress tracking
6. Add to `PhaseClasses` array in `BP_DRStageGameMode`
7. Add objective data to `DT_PhaseObjective` DataTable

### Adding Status Effect UI
1. Create delegate in `UOverlayWidgetController` for new effect
2. Bind to ASC's `OnGameplayEffectAppliedDelegateToSelf` in `BindCallbacksToDependencies()`
3. Filter by effect tag in callback
4. Broadcast to widget with effect data (duration, stacks, etc.)
5. Widget Blueprint implements visual representation

### Creating New Attribute
1. Add `FGameplayAttributeData` property to appropriate AttributeSet
2. Use `ATTRIBUTE_ACCESSORS` macro for getters/setters
3. Add to `GetLifetimeReplicatedProps()` with `DOREPLIFETIME_CONDITION_NOTIFY`
4. Handle in `PreAttributeChange()` for clamping
5. Handle in `PostGameplayEffectExecute()` for side effects
6. Create gameplay tag in `FDRGameplayTags`
7. Add to `TagsToAttributes` map if needed for dynamic lookup

## Code Location Reference

### Core Systems
- Game Mode/State: `Source/DaeRune/Public/Game/`
- Phase System: `Source/DaeRune/Public/Phase/`
- Characters: `Source/DaeRune/Public/Character/`
- Player/AI Controllers: `Source/DaeRune/Public/Player/`, `Source/DaeRune/Public/AI/`

### GAS
- Abilities: `Source/DaeRune/Public/AbilitySystem/Abilities/`
- Attribute Sets: `Source/DaeRune/Public/AbilitySystem/`
- Execution Calcs: `Source/DaeRune/Public/AbilitySystem/ExecCalc/`
- Gameplay Tags: `Source/DaeRune/Public/DRGameplayTags.h`

### UI
- HUD: `Source/DaeRune/Public/UI/HUD/`
- Widget Controllers: `Source/DaeRune/Public/UI/WidgetController/`
- Widgets (Blueprint): `Content/Blueprints/UI/`

### Actors
- CleanserSite: `Source/DaeRune/Public/Actor/DRCleanserSite.h`
- CleanserPart: `Source/DaeRune/Public/Actor/DRCleanserPart.h`

### Blueprints
- Game Mode: `Content/Blueprints/Game/BP_DRStageGameMode.uasset`
- Characters: `Content/Blueprints/Character/`
- Abilities: `Content/Blueprints/AbilitySystem/`
- UI: `Content/Blueprints/UI/`
- Phases: `Content/Blueprints/Phase/`

## Critical Files
- `DRGameplayTags.h/.cpp` - Gameplay tag definitions, modify for new tags
- `DRAbilitySystemLibrary.h/.cpp` - Static helper functions for GAS
- `DRStageGameMode.h/.cpp` - Phase orchestration, game flow
- `DRStageGameState.h/.cpp` - Replicated game state, objective tracking
- `OverlayWidgetController.h/.cpp` - UI data binding hub
- `DRAttributeSet.h/.cpp` - Base attribute logic, debuff system
- `DRCharacterBase.h/.cpp` - Character foundation, debuff visuals

## Plugin Dependencies
- **GameplayAbilities** - Core GAS framework (required)
- **MultiplayerSessions** - Custom multiplayer plugin for session management
- **OnlineSubsystemSteam** - Steam integration (enabled but may not be fully configured)
- **MotionWarping** - Character animation warping
- **EnhancedInput** - Modern input system
- **Niagara** - VFX system for debuffs and effects

## Debugging Tips

### Phase Issues
Check `ADRStageGameMode::TransitionToNextPhase()` logs, verify phase completion conditions in respective phase classes

### Ability Not Activating
1. Verify InputTag matches in InputConfig and Ability class
2. Check ASC initialization (client vs server timing)
3. Verify water cost with `CheckCost()` override
4. Check ability tags not blocked by active effects

### Replication Issues
1. Ensure property has `UPROPERTY(Replicated)` or `ReplicatedUsing`
2. Verify `GetLifetimeReplicatedProps()` includes property
3. Check server authority before modifying (use `HasAuthority()`)
4. Use `ROLE_Authority` checks in multicast RPCs

### Cleanser Site Not Responding
1. Verify actor has "CleanserSite" tag
2. Check phase index and site state
3. Verify health attribute initialization in Phase 3 (ASC itself is created in the constructor)
4. Check interaction box overlap events

### Widget Not Updating
1. Verify widget controller binding in HUD
2. Check delegate broadcasting in widget controller
3. Verify RepNotify functions calling delegates
4. Check GameState replication of phase data

## Development Branch Structure
- `main` - Main development branch
- Feature branches like `feat/PlayExpo` (current) are branched off `main`

Use descriptive branch names like `feat/`, `fix/`, `refactor/` for clear intent.
