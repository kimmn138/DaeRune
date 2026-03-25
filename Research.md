# Research: PoisonGas가 쉬는 시간에도 스폰되는 문제 점검

## 1. 결론

**쉬는 시간에 새로운 PoisonGas가 스폰되지는 않지만, 이전 웨이브에서 스폰된 PoisonGas가 쉬는 시간까지 살아남아 있을 수 있다.**

---

## 2. 상세 분석

### 2.1 PoisonGasSpawnTimerHandle 정리 여부

`EndCurrentWave()` (DRPhase3.cpp:255-273)에서:

```cpp
void UDRPhase3::EndCurrentWave()
{
    if (!GameMode) return;

    if (UWorld* World = GameMode->GetWorld())
    {
        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
        World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);  // ★ 반복 스폰 타이머 해제
    }
    // ...
    StartRestTime();
}
```

**`PoisonGasSpawnTimerHandle`은 웨이브 종료 시 정상적으로 Clear된다.**
→ 쉬는 시간에 **새로운 PoisonGas 스폰은 발생하지 않음** (반복 타이머가 해제되었으므로).

### 2.2 이미 스폰된 PoisonGas 액터의 생존 문제

`SpawnPoisonGasActor()` (DRPhase3.cpp:1034-1068)에서:

```cpp
PoisonGas->SetLifeSpan(10.0f);  // 경고 3초 + 활성 7초
```

`SpawnToxicGas()` (DRPhase3.cpp:1011-1031)에서:

```cpp
World->GetTimerManager().SetTimer(
    PoisonGasSpawnTimerHandle,
    this,
    &UDRPhase3::SpawnPoisonGasActor,
    PoisonGasSpawnInterval,  // 기본값 10초
    true,   // 반복 실행
    0.0f    // 첫 스폰 즉시
);
```

**타이밍 분석**:
- PoisonGas는 매 `PoisonGasSpawnInterval`(10초)마다 스폰됨
- 각 PoisonGas 액터의 수명(LifeSpan)은 10초
- 웨이브 PlayDuration은 기본 50초

**최악의 경우 시나리오**:
```
웨이브 시작: 0초
  ├─ 0초: PoisonGas 배치 #1 스폰 (수명 0~10초)
  ├─ 10초: PoisonGas 배치 #2 스폰 (수명 10~20초)
  ├─ 20초: PoisonGas 배치 #3 스폰 (수명 20~30초)
  ├─ 30초: PoisonGas 배치 #4 스폰 (수명 30~40초)
  ├─ 40초: PoisonGas 배치 #5 스폰 (수명 40~50초)  ★
웨이브 종료: 50초
  ├─ PoisonGasSpawnTimerHandle Clear (새 스폰 중단)
  ├─ 하지만 배치 #5는 아직 수명 남음 (40~50초 → 50초에 Destroy)
쉬는 시간 시작: 50초
```

위 경우 배치 #5의 마지막 스폰 타이밍이 웨이브 종료 직전이면, LifeSpan 10초 중 일부가 쉬는 시간에 겹칠 수 있다.

**구체적인 겹침 계산**:
- 마지막 스폰이 웨이브 종료 `X초 전`에 발생했다면
- 쉬는 시간에 `10 - X초` 동안 PoisonGas가 남아있음
- 예: 마지막 스폰이 종료 2초 전 → 쉬는 시간에 8초 동안 PoisonGas 활성

### 2.3 EndCurrentWave()에서 기존 PoisonGas 파괴 여부

`EndCurrentWave()`는 `PoisonGasSpawnTimerHandle`만 Clear하고, **이미 스폰된 PoisonGas 액터들은 파괴하지 않는다.**

반면 `RemoveToxicGas()` (DRPhase3.cpp:1071-1093)는 타이머 Clear + 모든 기존 액터 Destroy를 모두 수행한다:

```cpp
void UDRPhase3::RemoveToxicGas()
{
    // 타이머 해제
    if (PoisonGasSpawnTimerHandle.IsValid())
    {
        World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
    }

    // 모든 기존 가스 액터 즉시 파괴
    for (TWeakObjectPtr<AActor> GasActor : ToxicGasActors)
    {
        if (GasActor.IsValid())
        {
            GasActor->Destroy();
        }
    }

    ToxicGasActors.Empty();
}
```

**하지만 `RemoveToxicGas()`는 `EndCurrentWave()`에서 호출되지 않는다.** `RemoveToxicGas()`가 호출되는 곳은:
- `OnPhaseEnd()` (DRPhase3.cpp:105) — Phase 3 전체 종료 시에만

### 2.4 문제 요약

| 항목 | 상태 |
|------|------|
| 쉬는 시간에 새 PoisonGas 스폰 | X (타이머 Clear됨) |
| 쉬는 시간에 이전 PoisonGas 생존 | **O (LifeSpan이 남아있으면 활성 상태 유지)** |
| EndCurrentWave()에서 기존 액터 파괴 | **X (타이머만 Clear, 액터는 남음)** |

---

## 3. 해결 방법

`EndCurrentWave()`에서 `PoisonGasSpawnTimerHandle` Clear 대신 `RemoveToxicGas()`를 호출하면 된다. `RemoveToxicGas()`는 이미 타이머 Clear + 기존 액터 파괴를 모두 수행하므로 기존 `ClearTimer` 코드를 대체할 수 있다.

### 현재 코드 (DRPhase3.cpp:255-273)

```cpp
void UDRPhase3::EndCurrentWave()
{
    if (!GameMode) return;

    if (UWorld* World = GameMode->GetWorld())
    {
        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
        World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);  // 타이머만 해제
    }

    // ...
}
```

### 수정 방향

```cpp
void UDRPhase3::EndCurrentWave()
{
    if (!GameMode) return;

    if (UWorld* World = GameMode->GetWorld())
    {
        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
    }

    RemoveToxicGas();  // 타이머 해제 + 기존 액터 전부 파괴

    // ...
}
```

이렇게 하면:
1. `PoisonGasSpawnTimerHandle` Clear는 `RemoveToxicGas()` 내부에서 처리
2. `ToxicGasActors` 배열의 모든 기존 PoisonGas 액터가 즉시 Destroy
3. 쉬는 시간에 PoisonGas가 남아있는 문제 해결
