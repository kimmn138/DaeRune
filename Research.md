# WaterPump (물대포) 어빌리티 상세 분석 보고서

## 1. 개요

WaterPump는 GardenRobot 캐릭터의 Skill2 슬롯에 배정된 **연속 빔(Continuous Beam) 어빌리티**이다. 플레이어가 입력을 누르고 있는 동안 물줄기를 전방으로 발사하여, 적에게는 **데미지**를 주고 아군에게는 **물 자원(Water)**을 회복시켜 주는 이중 기능을 수행한다.

---

## 2. 클래스 계층 구조

```
UGameplayAbility (엔진)
  └── UDRGameplayAbility (커스텀 베이스)
        - StartupInputTag: 입력 태그 바인딩
        - WaterCost: 물 자원 소모량
        - CheckCost() / ApplyCost() 오버라이드
        └── UDRDamageGameplayAbility (데미지 전용 베이스)
              - DamageEffectClass: 데미지 GE 클래스
              - DamageType: 데미지 태그 (Damage.*)
              - Damage: 레벨별 스케일러블 데미지
              - Debuff 관련: Chance, Damage, Duration, Frequency
              - Knockback/DeathImpulse 설정
              - MakeDamageEffectParamsFromClassDefaults(): 데미지 파라미터 생성
              - CauseDamage(): 직접 데미지 적용
              └── UDRWaterPump (본 클래스)
                    - 빔 감지 로직 (LineTrace + BoxOverlap)
                    - 틱 기반 데미지/효과 시스템
                    - Niagara 빔 이펙트 관리
                    - 타겟 추적 시스템
```

---

## 3. 파일 구조

### C++ 소스
| 파일 | 역할 |
|------|------|
| `Source/DaeRune/Public/AbilitySystem/Abilities/DRWaterPump.h` | 헤더 - 프로퍼티, 함수 선언 |
| `Source/DaeRune/Private/AbilitySystem/Abilities/DRWaterPump.cpp` | 구현 - 빔 로직, 타겟 감지, 이펙트 |
| `Source/DaeRune/Public/DRGameplayTags.h` | 태그 선언 |
| `Source/DaeRune/Private/DRGameplayTags.cpp` | 태그 등록 |

### 블루프린트 에셋
| 파일 | 역할 |
|------|------|
| `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/Skill2/GA_WaterPump.uasset` | 메인 어빌리티 블루프린트 |
| `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/Skill2/GE_Cost_WaterPump.uasset` | 물 자원 소모 GE |
| `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/Skill2/GE_WaterPump_SlowSelf.uasset` | 자기 이동속도 감소 GE |
| `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/Skill2/GE_WaterPump_GrantWater.uasset` | 아군 물 자원 회복 GE |
| `Content/Blueprints/AbilitySystem/GameplayCueNotifies/GC_WaterPump.uasset` | 루프 사운드 큐 (사운드 전용) |

### 문서
| 파일 | 역할 |
|------|------|
| `GA_WaterPump.md` | 블루프린트 노드 흐름 기술 |

---

## 4. 게임플레이 태그

| 태그 | 용도 |
|------|------|
| `Abilities.GardenRobot.WaterPump` | 어빌리티 식별 태그 |
| `GameplayCue.Skill.WaterPump` | 루프 사운드 큐 태그 |
| `Event.Montage.WaterPump` | 몽타주 이벤트 트리거 태그 |
| `CombatSocket.RightHand` | 무기 소켓 위치 태그 |

---

## 5. 핵심 설정값 (C++ Default Properties)

| 프로퍼티 | 기본값 | 설명 |
|----------|--------|------|
| `WeaponRange` | 1000.0 | 빔 최대 사거리 (Unreal Unit) |
| `BeamWidth` | 25.0 | 빔 감지 박스 가로 반경 |
| `BeamHeight` | 25.0 | 빔 감지 박스 세로 반경 |
| `TickInterval` | 0.1초 | 서버 데미지 틱 주기 |
| `DamageApplicationInterval` | 10 (틱) | 데미지 적용 간격 = 10 × 0.1초 = **1초** |
| `MuzzleSocketName` | "TestRightHand" | Niagara 이펙트 부착 소켓 |
| `bShowDebugVisualization` | false | 디버그 시각화 토글 |

---

## 6. 네트워크 아키텍처 (서버/클라이언트 역할 분리)

### 6.1 설계 원칙

**"Server does logic, Client does visuals"**

GAS에서 어빌리티는 서버와 Owning Client 양쪽에서 `ActivateAbility`가 실행된다. WaterPump는 `HasAuthority()`와 `IsLocallyControlled()`를 사용하여 각 역할을 명확히 분리한다.

```
서버 (HasAuthority)                 클라이언트 (IsLocallyControlled)
┌────────────────────┐              ┌────────────────────┐
│ 타겟 감지          │              │ 빔 끝점 계산       │
│ 데미지 틱 카운팅   │              │ Niagara 이펙트     │
│ OnDamageTickReached│              │ 빔 위치/회전 갱신  │
│ GameplayCue 관리   │              │ OnBeamEndPointUpdated│
│ GE 적용 (데미지/물)│              │                    │
└────────────────────┘              └────────────────────┘
```

### 6.2 시나리오별 동작 표

| 상황 | HasAuthority | IsLocallyControlled | 데미지 틱 | Niagara | GameplayCue |
|------|:-----------:|:-------------------:|:---------:|:-------:|:-----------:|
| 리슨 서버 호스트 | O | O | O | O | 서버 관리 |
| 리모트 클라이언트 (서버 측) | O | X | O | X | 서버 관리 |
| 리모트 클라이언트 (클라이언트 측) | X | O | X | O | ASC 리플리케이트 수신 |
| 데디케이티드 서버 | O | X | O | X | 서버 관리 |

### 6.3 GC_WaterPump가 사운드 전용인 이유

GameplayCue는 **이벤트 기반** (OnActive/OnRemove)이므로, 매 프레임 파라미터를 갱신해야 하는 빔 이펙트에는 부적합하다.

```
GameplayCue 라이프사이클:
  OnActive ─────────────────────────── OnRemove
           (이 사이에 파라미터 갱신 불가)
```

| 이펙트 유형 | 특성 | 관리 방식 |
|------------|------|----------|
| 루프 사운드 | 시작/종료만 필요, 파라미터 갱신 불필요 | GC_WaterPump (이벤트 기반) |
| 빔 Niagara | 매 0.033초 위치/회전/끝점 갱신 필요 | C++ 직접 관리 (틱 기반) |

---

## 7. 동작 흐름 (전체 라이프사이클)

### 7.1 활성화 단계 (Event ActivateAbility)

블루프린트(GA_WaterPump.uasset)에서 처리된다:

```
Event ActivateAbility
  → Cast To BP_GardenRobot (아바타 액터)
  → Sequence (두 가지 병렬 흐름)
```

**Then 0 - 몽타주 재생 & 루프 시작:**
```
1. IsLocallyControlled 확인
   - True: 1인칭 메시에 AM_FP_HoseBlast 몽타주 직접 재생
   - (공통) PlayMontageAndWait로 AM_HoseBlast 3인칭 몽타주 실행

2. Wait Gameplay Event (Event.Montage.WaterPump, Once, Exact Match)
   - 몽타주의 AnimNotify에서 이벤트가 발생하면 →

3. 루프 진입:
   a) SetInWaterLoop(true) → 캐릭터의 물대포 루프 상태 활성화
   b) ApplyGameplayEffectToOwner(GE_WaterPump_SlowSelf) → 자기 이동속도 감소
      → 반환된 핸들을 Slow Gameplay Effect Handle에 저장
   c) StartWaterPumpLoop() → C++ 서버/클라이언트 분기 루프 시작 (아래 7.2 참조)
   d) 3인칭 메시에 AM_InHoseBlast 몽타주 재생 (루프 애니메이션)
   e) Set Timer by Event(Cost, Damage Delta Time, Looping) → 주기적 비용 소모 타이머
```

**Then 1 - 입력 해제 대기 & 종료:**
```
1. Wait Input Release
   - 입력이 놓이면 →

2. 정리 작업:
   a) SetInWaterLoop(false) → 물대포 루프 상태 비활성화
   b) StopWaterPumpLoop() → C++ 서버/클라이언트 분기 정리 (아래 7.5 참조)
   c) Montage Stop(AM_InHoseBlast, BlendOut: 0.2초)
   d) RemoveGameplayEffectFromOwnerWithHandle(Slow 핸들) → 이동속도 복구
   e) Clear and Invalidate Timer by Handle(Cost 타이머)
   f) End Ability → 어빌리티 종료
```

### 7.2 StartWaterPumpLoop() - 서버/클라이언트 분기 초기화

`StartWaterPumpLoop()`이 호출되면 `HasAuthority`와 `IsLocallyControlled`로 분기한다:

```
공통:
  DamageTickCounter = 0, CurrentTarget = nullptr, PreviousTarget = nullptr

SERVER (HasAuthority):
  1. AddGameplayCue(GameplayCue.Skill.WaterPump) → ASC가 클라이언트에 자동 리플리케이트
  2. WaterPumpTimerHandle 시작 → 0.1초 간격 PerformWaterPumpTick() 루핑

CLIENT (IsLocallyControlled):
  1. StartBeamEffect() → 1인칭/3인칭 Niagara 컴포넌트 생성
  2. BeamUpdateTimer 시작 → 0.033초 간격 UpdateBeamEndpoint() 루핑
```

### 7.3 서버 틱 (`PerformWaterPumpTick` - 0.1초마다, 서버 전용)

```
1. 무기 소켓 위치 획득 (CombatSocket.RightHand)

2. CalculateWaterBeamEndPoint() → 빔 끝점 계산
   - 리플리케이트된 ControlRotation 기반 카메라 방향 사용
   - 무기 소켓 → 카메라 목표점으로 LineTrace (ECC_Pawn 채널)

3. FindClosestTargetInBeam() → 빔 범위 내 타겟 탐색
   - 빔 경로를 따라 BoxOverlap 수행 (ECC_Target 채널)
   - ICombatInterface 구현 + 생존 여부 필터링
   - 가장 가까운 타겟 1명 반환

4. 타겟 변경 감지:
   - 새 타겟 != 현재 타겟 → PreviousTarget 저장, DamageTickCounter 리셋
   - OnTargetChanged(이전 타겟, 새 타겟) 블루프린트 이벤트 발화

5. DamageTickCounter 증가
   - 10 이상 도달 시 (= 1초 경과):
     → DamageTickCounter 리셋
     → OnDamageTickReached(CurrentTarget) 블루프린트 이벤트 발화
```

**비주얼 관련 코드 없음**: `CachedBeamEndPoint` 갱신과 `OnBeamEndPointUpdated` 호출은 서버 틱에서 제거됨. 클라이언트의 `UpdateBeamEndpoint`에서 독립적으로 처리.

### 7.4 클라이언트 비주얼 틱 (`UpdateBeamEndpoint` - 0.033초마다, 클라이언트 전용)

```
1. 무기 소켓 위치 획득 (CombatSocket.RightHand)

2. CalculateWaterBeamEndPoint() 직접 호출 → 빔 끝점 계산
   - 로컬 카메라의 정확한 뷰포인트 사용 (지연 없음)
   - CachedBeamEndPoint에 결과 저장

3. Niagara 방향/회전 계산

4. FirstPersonBeam & ThirdPersonBeam 업데이트:
   - SetWorldLocation(소켓 위치)
   - SetWorldRotation(빔 회전)
   - SetVectorParameter("HitEffectPosition", 빔 끝점)

5. OnBeamEndPointUpdated() 블루프린트 이벤트 발화
```

**핵심**: `UpdateBeamEndpoint`는 `PerformWaterPumpTick`의 `CachedBeamEndPoint`에 의존하지 않고, `CalculateWaterBeamEndPoint()`를 직접 호출하여 독립적으로 빔 끝점을 계산한다. 클라이언트는 항상 정확한 로컬 카메라 데이터를 사용하므로 빔 비주얼이 플레이어의 조준점과 정확히 일치한다.

### 7.5 데미지/효과 적용 (`OnDamageTickReached` 블루프린트 이벤트, 서버 전용)

`PerformWaterPumpTick`이 서버에서만 실행되므로, 이 이벤트도 서버에서만 발화된다:

```
OnDamageTickReached(Target Actor)
  → IsNotFriend(자기, 타겟) 확인

  [적(True)인 경우]:
    → Cast To DREnemy
    → HasAuthority 확인 (방어적 체크, 이미 서버에서만 발화됨)
      → True: ApplyDamageEffect(MakeDamageEffectParamsFromClassDefaults(타겟))

  [아군(False)인 경우]:
    → Cast To DRCharacter
    → MakeTargetDataHandleFromActors(타겟)
    → ApplyGameplayEffectToTarget(GE_WaterPump_GrantWater)
```

### 7.6 주기적 비용 소모 (`Event Cost` 블루프린트 이벤트)

블루프린트의 "Set Timer by Event"에 의해 `Damage Delta Time` 간격으로 반복 발생:

```
Event Cost
  → CommitAbilityCost → WaterCost만큼 물 자원 소모
```

이는 C++의 `UDRGameplayAbility::ApplyCost()` 오버라이드를 통해 실행되며, 내부에서 `HasAuthority(&ActivationInfo)` 체크로 서버에서만 비용이 차감된다.

### 7.7 종료 단계 (`StopWaterPumpLoop` - 서버/클라이언트 분기 정리)

```
SERVER (HasAuthority):
  1. WaterPumpTimerHandle 해제
  2. RemoveGameplayCue(GameplayCue.Skill.WaterPump) → 클라이언트에 자동 리플리케이트
  3. DamageTickCounter, CurrentTarget, PreviousTarget 리셋

CLIENT (IsLocallyControlled):
  1. BeamUpdateTimer 해제
  2. StopBeamEffect() → Niagara 컴포넌트 비활성화 + 파괴
  3. CachedBeamEndPoint 리셋
```

---

## 8. 빔 감지 시스템 상세

### 8.1 단계 1: 빔 끝점 계산 (`CalculateWaterBeamEndPoint`)

**목적**: 카메라가 바라보는 방향으로 빔의 최종 도달점을 결정한다.
**호출 위치**: 서버(`PerformWaterPumpTick`) + 클라이언트(`UpdateBeamEndpoint`) 양쪽에서 호출되지만, 각각 독립적인 목적으로 사용.

```
입력: 무기 소켓 위치 (WeaponSocketLocation)
출력: 빔 끝점 (FVector), 장애물 히트 여부, 히트 결과

처리:
1. GetAimDirection() → 카메라 위치/방향 획득
   - PlayerController::GetPlayerViewPoint() 사용
   - 서버: 리플리케이트된 ControlRotation (약간의 지연)
   - 클라이언트: 로컬 카메라 (정확)
2. 카메라 목표점 = 카메라 위치 + (카메라 방향 × WeaponRange)
3. LineTrace: 무기 소켓 → 카메라 목표점
   - 채널: ECC_Pawn (벽/장애물 감지용)
   - 자기 자신 무시
4. 히트 시 → ImpactPoint 반환 / 미히트 시 → 카메라 목표점 반환
```

### 8.2 단계 2: 타겟 감지 (`FindClosestTargetInBeam`, 서버 전용)

**목적**: 빔 경로 내의 타격 가능한 대상을 찾는다.
**호출 위치**: 서버의 `PerformWaterPumpTick`에서만 호출.

```
입력: 무기 소켓 위치, 빔 끝점
출력: 가장 가까운 유효 타겟 (AActor*)

처리:
1. 빔 방향/길이 계산
2. 박스 파라미터 설정:
   - 중심: 무기 소켓과 빔 끝점의 중간점
   - 크기: (빔길이/2, BeamWidth/2, BeamHeight/2) = (가변, 12.5, 12.5)
   - 회전: 빔 방향에 정렬
3. OverlapMultiByChannel: ECC_Target 채널로 박스 오버랩
4. 결과 필터링:
   a) ICombatInterface 구현 여부 확인
   b) IsDead() 확인 (죽은 대상 제외)
   c) 무기 소켓과의 거리(DistSquared) 비교 → 가장 가까운 1명 선택
```

### 8.3 감지 박스 형태

```
     무기 소켓                    빔 끝점
         ●━━━━━━━━━━━━━━━━━━━━━━━━━●
         |          BeamLength          |
         |                              |
         ┌──────────────────────────────┐
         │   BeamWidth: 25 (±12.5)      │  ← 가로
         │   BeamHeight: 25 (±12.5)     │  ← 세로
         └──────────────────────────────┘

         박스는 빔 방향으로 회전 정렬됨 (FQuat)
```

### 8.4 서버 카메라 정확도

서버에서 리모트 클라이언트의 카메라 방향은 리플리케이트된 `ControlRotation`에 의존한다:

- `ControlRotation`은 `CharacterMovementComponent`에 의해 매 서버 틱마다 리플리케이트
- 일반적 레이턴시(20-100ms) 하에서 서버는 초당 30회 이상의 회전 업데이트를 수신
- WaterPump의 데미지 적용 간격은 **1초**(10틱)이므로 충분히 정확한 타겟 감지 가능
- 연속 빔 무기 특성상 플레이어는 타겟에 수 초간 빔을 유지하므로 50-100ms의 회전 지연은 체감 불가

---

## 9. Niagara 빔 이펙트 시스템 (클라이언트 전용)

### 9.1 듀얼 메시 구조

멀티플레이어 게임에서 1인칭/3인칭 메시를 분리하여 관리한다:

| 컴포넌트 | 메시 | 가시성 설정 | 대상 |
|----------|------|------------|------|
| `FirstPersonBeam` | `FirstPersonMesh` | `SetOnlyOwnerSee(true)` | 로컬 플레이어만 봄 |
| `ThirdPersonBeam` | `GetMesh()` (3인칭) | `SetOwnerNoSee(true)` | 다른 플레이어만 봄 |

### 9.2 이펙트 생성 (`StartBeamEffect`, 클라이언트 전용)

`IsLocallyControlled()` 분기 안에서만 호출되므로, 서버 전용 인스턴스에서는 Niagara 컴포넌트가 생성되지 않는다.

```
1. WaterCannonEffect (UNiagaraSystem) 에셋 유효성 확인
2. ADRCharacter로 캐스팅하여 두 메시 접근
3. 소켓 존재 확인 (MuzzleSocketName = "TestRightHand")
4. UNiagaraFunctionLibrary::SpawnSystemAttached()로 소켓에 부착 생성
5. "HitEffectPosition" 벡터 파라미터로 빔 끝점 전달
```

### 9.3 이펙트 업데이트 (`UpdateBeamEndpoint` - 0.033초/≈30fps, 클라이언트 전용)

```
1. 무기 소켓 위치 갱신
2. CalculateWaterBeamEndPoint() 직접 호출 → 빔 끝점 계산 (로컬 카메라 사용)
3. CachedBeamEndPoint 갱신
4. FirstPersonBeam & ThirdPersonBeam:
   - SetWorldLocation(소켓 위치)
   - SetWorldRotation(빔 회전)
   - SetVectorParameter("HitEffectPosition", 빔 끝점)
5. OnBeamEndPointUpdated() 블루프린트 이벤트 발화
```

**설계 의도**: 서버 데미지 틱(0.1초)과 클라이언트 이펙트 업데이트(0.033초)를 분리하여, 빔 비주얼은 부드럽게(≈30fps) 유지하면서 타겟 감지/데미지 로직은 서버에서만 더 낮은 빈도로 실행하여 성능과 네트워크 정확성을 모두 확보한다.

### 9.4 이펙트 정리 (`StopBeamEffect`, 클라이언트 전용)

```
FirstPersonBeam / ThirdPersonBeam 각각:
  → DeactivateImmediate() → 즉시 비활성화
  → DestroyComponent() → 컴포넌트 파괴
  → nullptr로 초기화
```

---

## 10. 블루프린트 이벤트 인터페이스 (C++ → Blueprint)

C++ 코드는 세 가지 `BlueprintImplementableEvent`를 정의하여 블루프린트에 제어권을 넘긴다:

| 이벤트 | 발화 위치 | 발화 시점 | 파라미터 | 블루프린트 활용 |
|--------|----------|----------|----------|----------------|
| `OnTargetChanged` | 서버 | 타겟이 바뀔 때 | OldTarget, NewTarget | 서버 로직용 |
| `OnDamageTickReached` | 서버 | 1초마다 (10틱 누적) | Target | 데미지/물 회복 GE 적용 |
| `OnBeamEndPointUpdated` | 클라이언트 | 매 0.033초 | EndPoint (FVector) | 추가 VFX 위치 동기화 |

블루프린트에서 호출 가능한 함수:

| 함수 | 용도 |
|------|------|
| `StartWaterPumpLoop()` | 서버/클라이언트 분기 루프 시작 |
| `StopWaterPumpLoop()` | 서버/클라이언트 분기 루프 정지 |
| `GetCurrentTarget()` | 현재 타겟 참조 (서버에서만 유의미) |
| `HasValidTarget()` | 타겟 유효성 확인 (서버에서만 유의미) |
| `GetDamageTickCount()` | 현재 틱 카운터 조회 |
| `GetBeamEndPoint()` | 캐시된 빔 끝점 조회 (클라이언트에서만 갱신됨) |
| `MakeTargetDataHandleFromActors()` | GE 적용을 위한 타겟 데이터 생성 |

---

## 11. 게임플레이 이펙트 (GE) 체계

### 11.1 GE_Cost_WaterPump
- **용도**: 어빌리티 유지 비용
- **적용 방식**: 블루프린트의 `CommitAbilityCost`를 통해 `Damage Delta Time` 간격으로 반복 적용
- **권한**: `ApplyCost()` 내부에서 `HasAuthority` 체크 → 서버에서만 차감
- **효과**: 물 자원(Water) 감소

### 11.2 GE_WaterPump_SlowSelf
- **용도**: 어빌리티 사용 중 자기 이동속도 감소
- **적용 시점**: 몽타주 이벤트 수신 후 즉시
- **제거 시점**: 입력 해제 시 핸들로 제거 (`RemoveGameplayEffectFromOwnerWithHandle`)
- **핸들 관리**: `Slow Gameplay Effect Handle` 변수에 저장

### 11.3 GE_WaterPump_GrantWater
- **용도**: 아군 플레이어에게 물 자원 회복
- **적용 대상**: `IsNotFriend() == false`인 DRCharacter (같은 팀 플레이어)
- **적용 방식**: `MakeTargetDataHandleFromActors()`로 타겟 데이터 생성 후 `ApplyGameplayEffectToTarget()`
- **권한**: `OnDamageTickReached`가 서버에서만 발화되므로 서버에서만 적용

### 11.4 DamageEffectClass (상속받은 데미지 GE)
- **용도**: 적에게 데미지 적용
- **적용 대상**: `IsNotFriend() == true`인 DREnemy
- **조건**: 서버 전용 (PerformWaterPumpTick이 서버에서만 실행 + 블루프린트 HasAuthority 방어적 체크)
- **적용 방식**: `MakeDamageEffectParamsFromClassDefaults()`로 파라미터 생성 → `ApplyDamageEffect()` 호출
- **포함 데이터**: BaseDamage, DamageType, Debuff 확률/데미지/지속시간, Knockback, DeathImpulse

---

## 12. 적/아군 판별 시스템

`UDRAbilitySystemLibrary::IsNotFriend()` 정적 함수 사용:

```cpp
bool IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
    const bool bBothArePlayers = First->ActorHasTag("Player") && Second->ActorHasTag("Player");
    const bool bBothAreEnemies = First->ActorHasTag("Enemy") && Second->ActorHasTag("Enemy");
    return !(bBothArePlayers || bBothAreEnemies);
}
```

- **Player 태그**: 플레이어 캐릭터에 부여
- **Enemy 태그**: 적 캐릭터에 부여
- 같은 태그면 아군(Friend), 다르면 적(Not Friend)

---

## 13. 타이머 체계 정리

| 타이머 | 간격 | 실행 위치 | 콜백 | 용도 |
|--------|------|----------|------|------|
| `WaterPumpTimerHandle` | 0.1초 (루핑) | **서버** | `PerformWaterPumpTick()` | 타겟 감지 + 데미지 틱 카운팅 |
| `BeamUpdateTimer` | 0.033초 (루핑) | **클라이언트** | `UpdateBeamEndpoint()` | 빔 끝점 계산 + Niagara 갱신 |
| `Damage and Cost Timer` (BP) | Damage Delta Time (루핑) | 양쪽 | `Event Cost` | 물 자원 비용 주기적 소모 |

---

## 14. CombatInterface 연동

WaterPump는 `ICombatInterface`의 다음 기능에 의존한다:

| 함수 | 용도 | 호출 위치 |
|------|------|----------|
| `GetCombatSocketLocation(CombatSocket.RightHand)` | 무기 소켓 월드 위치 획득 | 서버 + 클라이언트 |
| `IsDead()` | 타겟 생존 여부 필터링 | 서버 전용 |
| `SetInWaterLoop(bool)` | 캐릭터의 물대포 루프 상태 설정 | 블루프린트 (양쪽) |

---

## 15. 디버그 시각화

`bShowDebugVisualization = true` 설정 시 (`ENABLE_DRAW_DEBUG` 빌드에서만):

| 요소 | 색상 | 표시 내용 | 실행 위치 |
|------|------|----------|----------|
| LineTrace (히트) | 빨강 | 장애물까지의 빔 경로 | 서버 + 클라이언트 |
| LineTrace (미히트) | 초록 | 최대 사거리까지의 빔 경로 | 서버 + 클라이언트 |
| 히트 포인트 | 빨강 구체 (r=20) | 장애물 충돌 지점 | 서버 + 클라이언트 |
| 감지 박스 (오버랩 있음) | 노랑 | 타겟이 감지된 빔 박스 | **서버 전용** |
| 감지 박스 (오버랩 없음) | 파랑 | 타겟이 없는 빔 박스 | **서버 전용** |

`CalculateWaterBeamEndPoint()`는 서버와 클라이언트 양쪽에서 호출되므로 LineTrace 디버그는 양쪽에서 보인다. `FindClosestTargetInBeam()`는 서버의 `PerformWaterPumpTick`에서만 호출되므로 BoxOverlap 디버그는 서버에서만 보인다.

---

## 16. 네트워크 수정 이력

### 16.1 수정 전 문제점

수정 전에는 `StartWaterPumpLoop()`이 서버와 클라이언트 양쪽에서 동일하게 실행되어 다음 문제가 발생했다:

| # | 문제 | 심각도 | 영향 |
|---|------|--------|------|
| 1 | 서버 카메라 방향 불일치 | 높음 | 서버/클라이언트 타겟 감지 결과 불일치 |
| 2 | GrantWater HasAuthority 미체크 | 높음 | 아군 물 회복 2배 적용 |
| 3 | 서버에서 Niagara 생성 | 중간 | 불필요한 서버 리소스 소모 |
| 4 | GameplayCue 중복 관리 | 중간 | 사운드/VFX 이중 재생 가능 |
| 5 | 틱 타이머 비동기 진행 | 낮음 | 데미지/피드백 타이밍 불일치 |

### 16.2 수정 내용 (DRWaterPump.cpp)

| 함수 | 수정 내용 | 해결된 문제 |
|------|----------|------------|
| `StartWaterPumpLoop()` | `HasAuthority` → 서버 로직 / `IsLocallyControlled` → 클라이언트 비주얼 분리 | 1, 3, 4, 5 |
| `StopWaterPumpLoop()` | 동일한 분기로 대칭적 정리 | 3, 4 |
| `PerformWaterPumpTick()` | `CachedBeamEndPoint` 갱신 및 `OnBeamEndPointUpdated` 호출 제거 (서버 전용화) | 1 |
| `UpdateBeamEndpoint()` | `CalculateWaterBeamEndPoint()` 직접 호출하여 독립적 빔 계산 (클라이언트 전용화) | 1 |

### 16.3 미완료 블루프린트 수정 (에디터 수동 작업 필요)

`GA_WaterPump.uasset`의 `OnDamageTickReached` 이벤트에서 아군(False) 경로에 `HasAuthority` 체크 추가 (방어적 프로그래밍):

```
수정 전:
  → False (아군)
      → Cast To DRCharacter → ApplyGE(GE_GrantWater)

수정 후:
  → False (아군)
      → HasAuthority
          → True → Cast To DRCharacter → ApplyGE(GE_GrantWater)
```

C++ 수정으로 `OnDamageTickReached`는 이미 서버에서만 발화되므로 실질적 버그는 해결되었으나, 코드 변경에 대한 방어적 안전장치.

---

## 17. 전체 흐름 요약 다이어그램

```
[입력 누름]
    │
    ▼
Event ActivateAbility (서버 + 클라이언트)
    │
    ├─ Then 0: 몽타주 재생 → 이벤트 대기
    │   │
    │   ▼ (Event.Montage.WaterPump)
    │   SetInWaterLoop(true)
    │   GE_WaterPump_SlowSelf 적용
    │   StartWaterPumpLoop() ─────────────────────────┐
    │   AM_InHoseBlast 루프 몽타주                      │
    │   비용 타이머 시작                                │
    │                                                  ▼
    │                               ┌──────── HasAuthority? ────────┐
    │                               │                               │
    │                          [SERVER]                         [CLIENT]
    │                     ┌──────────────────┐          ┌──────────────────┐
    │                     │ AddGameplayCue   │          │ StartBeamEffect  │
    │                     │ (ASC가 리플리케이트)│        │ (Niagara 생성)   │
    │                     │                  │          │                  │
    │                     │ Tick (0.1초)     │          │ Tick (0.033초)   │
    │                     │  빔 끝점 계산    │          │  빔 끝점 계산    │
    │                     │  타겟 감지       │          │  Niagara 갱신    │
    │                     │  틱 카운터 증가  │          │  OnBeamEndPoint  │
    │                     │  10틱 도달 시:   │          │  Updated         │
    │                     │  OnDamageTickReached│       └──────────────────┘
    │                     │    → [적] 데미지 │
    │                     │    → [아군] 물회복│
    │                     └──────────────────┘
    │
    ├─ Then 1: Wait Input Release
    │
    ▼ (입력 놓음)
SetInWaterLoop(false)
StopWaterPumpLoop()
    ├─ [SERVER] 타이머 해제, GameplayCue 제거, 상태 리셋
    └─ [CLIENT] 타이머 해제, Niagara 정리, 캐시 리셋
몽타주 중단
GE_WaterPump_SlowSelf 제거
비용 타이머 해제
End Ability
```
