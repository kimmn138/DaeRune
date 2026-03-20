# WaterPump VFX 90도 꺾임 현상 원인 분석 (v2 - 수정됨)

## 현상 요약

물대포(WaterPump) 스킬 발동 시:
- **1인칭(1P)과 3인칭(3P) 모두**에서 문제 발생
- 호스를 드는 모션 **전체 과정**에서 VFX가 아래로 90도 꺾여 있음 (33ms 순간이 아님)
- 호스를 완전히 들고 물대포를 쏘기 시작하는 시점부터 올바른 방향으로 전환
- **핵심 추정 원인**: 빔의 끝지점(HitEffectPosition)이 유효하지 않은 상태에서 VFX가 생성됨

---

## 전체 실행 흐름

### Blueprint (GA_WaterPump)

```
Event ActivateAbility
→ Sequence
    → Then 0:
        [로컬] Play Montage: AM_FP_HoseBlast (1P 메시 - 호스 들기)
        [전체] PlayMontageAndWait: AM_HoseBlast (3P 메시 - 호스 들기)
        WaitGameplayEvent: Event.Montage.WaterPump
            → Event Received:
                ① SetInWaterLoop(true)
                ② ApplyGE: GE_WaterPump_SlowSelf
                ③ StartWaterPumpLoop()          ← VFX 생성 시점
                ④ Play Montage: AM_InHoseBlast  (3P 루프 애니메이션)
                ⑤ SetTimer: Cost
    → Then 1:
        WaitInputRelease → StopWaterPumpLoop() → EndAbility
```

### C++ (StartWaterPumpLoop → StartBeamEffect)

#### StartWaterPumpLoop() [DRWaterPump.cpp:182]
```cpp
CachedBeamEndPoint = FVector::ZeroVector;  // ★ (0,0,0)으로 초기화

// SERVER 분기
if (HasAuthority())
{
    // GameplayCue 추가 (3P 비소유 클라이언트용)
    ASC->AddGameplayCue(GameplayCue_Skill_WaterPump, FGameplayCueParameters());

    // PerformWaterPumpTick 타이머 (0.1초 간격, 최초 0.0초 딜레이)
    // → 이 안에서 WaterPumpBeamEndPoint를 갱신함
    SetTimer(PerformWaterPumpTick, 0.1초, looping, InitialDelay=0.0);

    // ★★ 문제: bWaterPumpActive = true를 설정하기 전에
    // ★★ WaterPumpBeamEndPoint가 아직 갱신되지 않았음
    DRChar->bWaterPumpActive = true;        // → OnRep 트리거
    DRChar->OnRep_WaterPumpActive();        // 리슨서버 수동 호출
}

// CLIENT 분기 (소유 클라이언트)
if (IsLocallyControlled())
{
    StartBeamEffect();                       // ★ CachedBeamEndPoint = (0,0,0) 상태
    SetTimer(UpdateBeamEndpoint, 0.033초);   // 보정은 33ms 후
}
```

#### StartBeamEffect() [DRWaterPump.cpp:370]
```cpp
FirstPersonBeam = SpawnSystemAttached(
    WaterCannonEffect, FPMesh, MuzzleSocketName,
    FVector::ZeroVector,            // 위치 오프셋 없음
    FRotator::ZeroRotator,          // ★ 회전 = 소켓 기본 회전 상속
    EAttachLocation::SnapToTarget,  // 소켓 트랜스폼 스냅
    false
);

// ★★ 핵심 문제: HitEffectPosition = (0,0,0) → 유효하지 않은 끝지점
FirstPersonBeam->SetVectorParameter(
    FName("HitEffectPosition"), CachedBeamEndPoint);  // = FVector::ZeroVector
```

#### OnRep_WaterPumpActive() [DRCharacter.cpp:316] (3P 빔)
```cpp
WaterPumpThirdPersonBeam = SpawnSystemAttached(
    WaterPumpEffectAsset, ThirdPersonMesh, WaterPumpMuzzleSocket,
    FVector::ZeroVector, FRotator::ZeroRotator,
    EAttachLocation::SnapToTarget, false
);

// ★★ 핵심 문제: WaterPumpBeamEndPoint = (0,0,0) → 유효하지 않은 끝지점
// (서버의 PerformWaterPumpTick이 아직 실행되지 않았거나, 리플리케이션 미도착)
WaterPumpThirdPersonBeam->SetVectorParameter(
    FName("HitEffectPosition"), WaterPumpBeamEndPoint);
```

---

## 근본 원인 분석

### 핵심 원인: 유효한 끝지점 없이 VFX가 생성됨

Niagara 시스템은 `HitEffectPosition` 파라미터를 빔의 **끝지점(목표 위치)**으로 사용한다.
VFX가 생성되는 시점에서 이 값이 `(0,0,0)` (월드 원점)이면:

- **빔이 소켓 위치 → 월드 원점(0,0,0) 방향**으로 향함
- 캐릭터가 월드 원점보다 위에 있으면 → **아래로 90도 꺾여 보임**
- Niagara 시스템이 내부적으로 빔 방향을 `(소켓위치 → HitEffectPosition)` 벡터로 결정하기 때문

### 왜 "33ms 후 보정"이 아니라 "호스 들기 전체 과정"에서 보이는가?

#### 1P 빔의 경우:
- `StartBeamEffect()` 직후 `UpdateBeamEndpoint()` 타이머가 0.033초 간격으로 시작됨
- `UpdateBeamEndpoint()`가 `SetWorldRotation()`과 `SetVectorParameter("HitEffectPosition", 올바른값)`으로 보정
- **하지만**: Niagara 컴포넌트는 `SnapToTarget`으로 소켓에 부착된 상태. 매 프레임 소켓 트랜스폼에 스냅되면서 `SetWorldRotation()`으로 설정한 회전이 다음 프레임에 소켓 회전으로 덮어써질 수 있음
- 또는: Niagara 시스템 내부에서 `HitEffectPosition`이 파티클 스폰 시점의 값으로 캐시되어, 이미 방출된 파티클은 계속 잘못된 방향으로 이동

#### 3P 빔의 경우:
- `bWaterPumpActive = true` 설정 시점에서 `WaterPumpBeamEndPoint`는 아직 `(0,0,0)`
- `PerformWaterPumpTick()`의 InitialDelay가 0.0이지만, 타이머 콜백은 같은 프레임 또는 다음 틱에서 실행
- `bWaterPumpActive` RepNotify가 먼저 도달하고, `WaterPumpBeamEndPoint` 리플리케이션은 아직 안 옴
- 결과: 3P 빔도 `HitEffectPosition = (0,0,0)`으로 생성됨

### 시간축 다이어그램 (수정본)

```
[AM_FP_HoseBlast / AM_HoseBlast 시작] ─── 호스 들기 애니메이션 시작
    │
    │  (애니메이션 재생 중...)
    │
    ▼
[AnimNotify: Event.Montage.WaterPump 발화] ─── 호스가 아직 중간 자세
    │
    ├─ CachedBeamEndPoint = (0,0,0)
    │
    ├─ [SERVER]
    │   ├─ PerformWaterPumpTick 타이머 시작 (InitialDelay=0.0)
    │   │   └─ 하지만 타이머 콜백은 아직 실행 안 됨 (같은 프레임 내 or 다음 틱)
    │   │
    │   ├─ bWaterPumpActive = true                    ← WaterPumpBeamEndPoint는 아직 (0,0,0)
    │   └─ OnRep_WaterPumpActive() 수동 호출 (리슨서버)
    │       └─ 3P 빔 생성: HitEffectPosition = (0,0,0)  ★★ 3P 문제 발생
    │
    └─ [CLIENT - 로컬]
        ├─ StartBeamEffect()
        │   └─ 1P 빔 생성: HitEffectPosition = (0,0,0)  ★★ 1P 문제 발생
        │
        │   ★★ 이 시점: 1P/3P 모두 VFX가 월드 원점(아래)을 향함 ★★
        │
        └─ UpdateBeamEndpoint 타이머 시작 (33ms 간격)
            │
            │ ← 33ms 후 첫 보정
            │   하지만 Niagara가 SnapToTarget 부착 상태이고
            │   이미 방출된 파티클은 보정 불가
            │
            ▼
    (호스 들기 애니메이션 완료 → AM_InHoseBlast 루프 시작)
            │
            ▼
    [여러 차례 UpdateBeamEndpoint 실행 후]
        └─ 비로소 VFX가 올바른 방향으로 안정화

    ★ 호스 들기 전체 과정(수백ms)에서 VFX가 아래로 꺾여 보임
```

### 3P 빔 리플리케이션 타이밍 상세

```
서버 StartWaterPumpLoop():
    ├─ PerformWaterPumpTick 타이머 등록 (InitialDelay=0.0)
    │   └─ 이 시점에서는 아직 콜백 미실행
    │       WaterPumpBeamEndPoint = (0,0,0) 그대로
    │
    ├─ bWaterPumpActive = true
    │   └─ RepNotify 큐에 등록됨
    │
    └─ 이후 프레임:
        ├─ PerformWaterPumpTick() 첫 실행 → WaterPumpBeamEndPoint 갱신
        └─ bWaterPumpActive RepNotify가 클라이언트에 도착
            └─ 이때 WaterPumpBeamEndPoint도 리플리케이트되었을 수 있지만
               ★ 같은 리플리케이션 번들에 포함되지 않을 수 있음
               ★ 특히 bWaterPumpActive가 먼저 도착하면 3P 빔은 (0,0,0)으로 생성
```

---

## 결론 (수정본)

| 항목 | 설명 |
|------|------|
| **핵심 원인** | VFX 생성 시점에 `HitEffectPosition`이 `(0,0,0)` (유효한 끝지점 없음) |
| **1P 빔** | `CachedBeamEndPoint = FVector::ZeroVector` 상태에서 `StartBeamEffect()` 호출 |
| **3P 빔** | `WaterPumpBeamEndPoint`가 아직 서버에서 계산되기 전에 `bWaterPumpActive = true`가 설정됨 |
| **지속 시간** | 33ms가 아닌 호스 들기 애니메이션 전체 시간 (Niagara 파티클 캐시 + SnapToTarget 부착) |
| **방향** | 소켓 위치에서 월드 원점(0,0,0)을 향하므로 대부분의 경우 아래로 꺾여 보임 |
| **해결 방향** | VFX 생성 전에 유효한 끝지점을 먼저 계산하여 설정해야 함 |
