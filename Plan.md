# WaterPump 1P VFX 크기 및 빔 끝점 초과 문제 수정 계획

---

## 1. 현상

1P 빔이 보이게 된 이후 발견된 두 가지 문제:

| # | 문제 | 설명 |
|---|------|------|
| 1 | **1P VFX 반지름이 너무 큼** | 현재 1P와 3P가 동일한 Niagara 시스템(`WaterCannonEffect`)을 사용. 이 VFX는 3P 메시 크기에 맞춰 제작되어서, 1P 시점에서 카메라에 매우 가깝게 렌더링될 때 반지름이 과도하게 크게 보임. |
| 2 | **빔이 끝점(HitEffectPosition)을 넘어감** | 빔 VFX가 `HitEffectPosition`(장애물 또는 최대 사거리 지점)에서 멈추지 않고 그 너머까지 렌더링됨. |

---

## 2. 원인 분석

### 2.1 문제 1: 1P VFX 반지름 과대

```
현재 구조:
  WaterCannonEffect (하나의 Niagara 에셋)
    ├── 1P 빔 (StartBeamEffect)       → 카메라 바로 앞에서 렌더링 → 크게 보임
    └── 3P 빔 (OnRep_WaterPumpActive) → 먼 거리에서 렌더링 → 적절한 크기
```

3P 메시는 다른 플레이어에게 수 미터 떨어져 보이므로 원래 크기가 적절하지만, 1P 메시는 카메라에 50cm 거리에 있으므로 같은 반지름의 물줄기가 화면의 상당 부분을 차지한다.

**해결**: 1P 전용 Niagara 시스템을 별도로 만들어 사용.

### 2.2 문제 2: 빔이 끝점을 넘어감

현재 C++ 코드가 Niagara에 전달하는 파라미터:

```
SetWorldLocation(WeaponSocketLocation)    → 빔 시작점 (위치)
SetWorldRotation(BeamRotation)            → 빔 방향 (회전)
SetVectorParameter("HitEffectPosition")   → 빔 끝점 (월드 좌표)
```

Niagara 시스템이 파티클의 이동 거리를 `HitEffectPosition`까지로 제한하려면, **시작점에서 끝점까지의 거리**(= 빔 길이)를 알아야 한다. 현재는 `HitEffectPosition`만 월드 좌표로 전달되므로, Niagara 내부에서 빔 길이를 계산하기 어렵거나 제대로 활용되지 않고 있다.

**해결**: C++에서 `BeamLength`(float)를 계산하여 Niagara에 추가 파라미터로 전달. Niagara 시스템에서 이 값으로 파티클 수명/이동 거리를 제한.

---

## 3. 수정 계획

### 3.1 C++ 수정 (DRWaterPump.h)

`WaterCannonEffect1P` 프로퍼티 추가:

```cpp
protected:
    // 3P Niagara System Asset (기존 - 3P 빔 + DRCharacter의 WaterPumpEffectAsset에서도 사용)
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> WaterCannonEffect;

    // [추가] 1P 전용 Niagara System Asset (반지름이 작은 1P용 VFX)
    // 설정되지 않으면 WaterCannonEffect를 폴백으로 사용
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> WaterCannonEffect1P;
```

**설계 의도:**
- `WaterCannonEffect1P`가 설정되면 → 1P 빔에 사용 (작은 반지름)
- `WaterCannonEffect1P`가 null이면 → `WaterCannonEffect`를 폴백 (하위 호환)
- `WaterCannonEffect`는 3P 빔 전용으로 유지 (DRCharacter의 `WaterPumpEffectAsset`과 동일)

### 3.2 C++ 수정 (DRWaterPump.cpp) - StartBeamEffect()

1P 전용 에셋 사용 + `BeamLength` 파라미터 전달:

```cpp
void UDRWaterPump::StartBeamEffect()
{
    // 1P 전용 에셋이 있으면 사용, 없으면 기존 에셋 폴백
    UNiagaraSystem* EffectToUse = WaterCannonEffect1P ? WaterCannonEffect1P : WaterCannonEffect;

    if (!EffectToUse)
    {
        UE_LOG(LogTemp, Error, TEXT("StartBeamEffect: No Niagara effect available!"));
        return;
    }

    ADRCharacter* Character = Cast<ADRCharacter>(GetAvatarActorFromActorInfo());
    if (!Character) { ... return; }

    USkeletalMeshComponent* FPMesh = Character->FirstPersonMesh;
    if (!FPMesh) { ... return; }
    if (!FPMesh->DoesSocketExist(MuzzleSocketName)) { ... return; }

    FirstPersonBeam = UNiagaraFunctionLibrary::SpawnSystemAttached(
        EffectToUse,       // ← WaterCannonEffect 대신 EffectToUse 사용
        FPMesh,
        MuzzleSocketName,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget,
        false
    );

    if (FirstPersonBeam)
    {
        FirstPersonBeam->SetOnlyOwnerSee(true);
        FirstPersonBeam->SetVectorParameter(FName("HitEffectPosition"), CachedBeamEndPoint);
        FirstPersonBeam->SetFloatParameter(FName("BeamLength"), 0.f);  // ← 초기값
    }
}
```

### 3.3 C++ 수정 (DRWaterPump.cpp) - UpdateBeamEndpoint()

`BeamLength` 파라미터를 매 프레임 갱신:

```cpp
void UDRWaterPump::UpdateBeamEndpoint()
{
    ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetAvatarActorFromActorInfo());
    if (!DRCharacter) return;

    USkeletalMeshComponent* FPMesh = DRCharacter->FirstPersonMesh;
    if (!FPMesh) return;

    const FVector WeaponSocketLocation = FPMesh->GetSocketLocation(MuzzleSocketName);

    bool bHitObstacle = false;
    FHitResult HitResult;
    CachedBeamEndPoint = CalculateWaterBeamEndPoint(WeaponSocketLocation, bHitObstacle, HitResult);

    // ★ 빔 길이 계산
    const float BeamLength = FVector::Distance(WeaponSocketLocation, CachedBeamEndPoint);

    FVector BeamDir = CachedBeamEndPoint - WeaponSocketLocation;
    if (!BeamDir.IsNearlyZero()) BeamDir.Normalize();
    const FRotator BeamRotation = BeamDir.Rotation();

    if (FirstPersonBeam)
    {
        FirstPersonBeam->SetWorldLocation(WeaponSocketLocation);
        FirstPersonBeam->SetWorldRotation(BeamRotation);
        FirstPersonBeam->SetVectorParameter(FName("HitEffectPosition"), CachedBeamEndPoint);
        FirstPersonBeam->SetFloatParameter(FName("BeamLength"), BeamLength);  // ← 추가
    }

    OnBeamEndPointUpdated(CachedBeamEndPoint);
}
```

### 3.4 C++ 수정 (DRCharacter.cpp) - 3P 빔에도 BeamLength 전달

3P 빔에서도 같은 끝점 초과 문제가 발생할 수 있으므로 동일하게 적용:

**OnRep_WaterPumpActive() 초기 생성 시:**

```cpp
if (WaterPumpThirdPersonBeam)
{
    WaterPumpThirdPersonBeam->SetOwnerNoSee(true);
    WaterPumpThirdPersonBeam->SetVectorParameter(
        FName("HitEffectPosition"), WaterPumpBeamEndPoint);

    // ★ 추가: 빔 길이 전달
    const FVector SocketLoc = ThirdPersonMesh->GetSocketLocation(WaterPumpMuzzleSocket);
    const float BeamLen = FVector::Distance(SocketLoc, WaterPumpBeamEndPoint);
    WaterPumpThirdPersonBeam->SetFloatParameter(FName("BeamLength"), BeamLen);
}
```

**UpdateWaterPumpThirdPersonBeam() 매 프레임 갱신:**

```cpp
// 기존 코드 끝에 추가
WaterPumpThirdPersonBeam->SetVectorParameter(
    FName("HitEffectPosition"), WaterPumpBeamDisplayEndPoint);

// ★ 추가: 빔 길이 전달
const float BeamLen = FVector::Distance(SocketLocation, WaterPumpBeamDisplayEndPoint);
WaterPumpThirdPersonBeam->SetFloatParameter(FName("BeamLength"), BeamLen);
```

### 3.5 Niagara 에디터 작업 (블루프린트)

#### 3.5.1 1P 전용 Niagara 시스템 생성

1. **에디터에서 기존 WaterCannon Niagara 시스템 복제**:
   - `Content/Blueprints/VFX/WaterCannon/WaterCannon.uasset` (또는 현재 사용 중인 에셋) 우클릭 → Duplicate
   - 이름: `WaterCannon_1P` (또는 적절한 이름)
   - 저장 위치: `Content/Blueprints/VFX/WaterCannon/`

2. **1P 에셋에서 반지름/크기 축소**:
   - Niagara 에디터에서 `WaterCannon_1P` 열기
   - 파티클의 **Sprite Size**, **Scale**, **Radius** 등 크기 관련 파라미터를 축소
   - 1P 시점에서 자연스러운 크기가 될 때까지 조절 (일반적으로 3P 대비 30~50% 크기)
   - 리본/스프라이트의 Width도 확인하여 축소

3. **GA_WaterPump 블루프린트에서 1P 에셋 연결**:
   - GA_WaterPump 블루프린트 열기
   - Class Defaults → Effects 카테고리
   - `WaterCannonEffect1P`에 `WaterCannon_1P` 에셋 설정
   - `WaterCannonEffect`는 기존 3P용 에셋 유지

#### 3.5.2 Niagara 시스템에 BeamLength 파라미터 추가

**1P(`WaterCannon_1P`)와 3P(`WaterCannon`) Niagara 시스템 모두에 적용:**

1. **User Parameter 추가**:
   - Niagara 에디터 → Parameters 패널
   - User Exposed 섹션에 `BeamLength` (float) 파라미터 추가
   - 기본값: `1000.0` (최대 사거리와 동일)

2. **파티클 수명/이동 거리를 BeamLength로 제한**:

   방법은 Niagara 시스템의 구현에 따라 다름. 일반적인 접근:

   **방법 A - Particle Lifetime 제한** (속도 기반 파티클):
   ```
   Particle Spawn → Set Lifetime:
     Lifetime = BeamLength / ParticleSpeed
   ```
   파티클이 BeamLength 거리를 이동하면 수명이 끝나 사라짐.

   **방법 B - Kill Particles Beyond Distance** (위치 기반):
   ```
   Particle Update → Kill Particles:
     조건: Distance(Particle.Position, Emitter.Position) > BeamLength
   ```
   시작점에서 BeamLength 이상 멀어진 파티클을 즉시 제거.

   **방법 C - Scale/Fade by Distance** (부드러운 종료):
   ```
   Particle Update → Scale Sprite Size:
     Factor = 1.0 - Saturate((Distance - BeamLength * 0.9) / (BeamLength * 0.1))
   ```
   끝점 근처(90~100%)에서 서서히 크기가 줄어들며 사라짐.

   **권장**: 방법 B가 가장 간단하고 확실함. 방법 C를 추가하면 더 자연스러움.

3. **HitEffectPosition 활용 확인**:
   - 기존 Niagara 시스템에서 `HitEffectPosition`이 어떻게 사용되는지 확인
   - 히트 이펙트(스플래시 등)의 위치로만 사용되는지, 빔 길이 제한에도 사용되는지 확인
   - 빔 길이 제한에 사용되고 있지 않다면, `BeamLength` 파라미터로 제한 추가

---

## 4. 수정 파일 목록

| # | 파일 | 변경 유형 | 변경 내용 |
|---|------|----------|----------|
| 1 | `DRWaterPump.h` | UPROPERTY 추가 | `WaterCannonEffect1P` (1P 전용 Niagara 에셋) |
| 2 | `DRWaterPump.cpp` | 로직 수정 | `StartBeamEffect()`: 1P 에셋 우선 사용 + BeamLength 초기값 |
| 3 | `DRWaterPump.cpp` | 로직 수정 | `UpdateBeamEndpoint()`: BeamLength 매 프레임 갱신 |
| 4 | `DRCharacter.cpp` | 로직 수정 | `OnRep_WaterPumpActive()`: BeamLength 초기값 전달 |
| 5 | `DRCharacter.cpp` | 로직 수정 | `UpdateWaterPumpThirdPersonBeam()`: BeamLength 매 프레임 갱신 |
| 6 | Niagara 에셋 (에디터) | 에셋 생성 | `WaterCannon_1P` 1P 전용 에셋 생성 (3P 에셋 복제 후 크기 축소) |
| 7 | Niagara 에셋 (에디터) | 파라미터 추가 | `BeamLength` User Parameter 추가 + 파티클 거리 제한 로직 |
| 8 | GA_WaterPump (에디터) | 설정 | `WaterCannonEffect1P`에 1P 에셋 연결 |

**헤더 변경 있음** (`DRWaterPump.h`) → 에디터 재시작 또는 전체 컴파일 필요

---

## 5. 구현 순서

```
1. DRWaterPump.h     → WaterCannonEffect1P 프로퍼티 추가
2. DRWaterPump.cpp   → StartBeamEffect() 수정 (1P 에셋 사용 + BeamLength)
3. DRWaterPump.cpp   → UpdateBeamEndpoint() 수정 (BeamLength 갱신)
4. DRCharacter.cpp   → OnRep_WaterPumpActive() 수정 (BeamLength)
5. DRCharacter.cpp   → UpdateWaterPumpThirdPersonBeam() 수정 (BeamLength)
6. 컴파일 (헤더 변경 → 전체 컴파일)
7. Niagara 에디터    → WaterCannon 복제 → WaterCannon_1P 생성 + 크기 축소
8. Niagara 에디터    → 양쪽 에셋에 BeamLength 파라미터 추가 + 거리 제한 로직
9. GA_WaterPump BP   → WaterCannonEffect1P에 WaterCannon_1P 설정
10. 테스트
```

---

## 6. 테스트 계획

### 6.1 1P VFX 크기 확인
- 호스트가 WaterPump 사용 → 1P 시점에서 물줄기 반지름이 자연스러운지 확인
- 화면을 과도하게 가리지 않는지 확인

### 6.2 빔 끝점 제한 확인
- 벽을 향해 WaterPump 발사 → 빔이 벽(HitEffectPosition)에서 멈추는지 확인
- 빔이 벽 너머로 뚫고 나가지 않는지 확인
- 최대 사거리(1000 유닛)에서 빔이 정확히 끝나는지 확인
- 가까운 거리/먼 거리 모두 테스트

### 6.3 3P 빔 확인
- 다른 플레이어가 본 3P 빔도 벽에서 멈추는지 확인
- 3P 빔 크기가 기존과 동일한지 확인 (3P 에셋 변경 없으므로)

### 6.4 폴백 확인
- `WaterCannonEffect1P`를 비워두면 기존 `WaterCannonEffect`로 동작하는지 확인

---

## 7. 요약

| 문제 | 원인 | 해결 |
|------|------|------|
| 1P VFX 반지름 과대 | 1P/3P가 동일한 Niagara 시스템 사용, 3P 크기로 제작됨 | `WaterCannonEffect1P` 프로퍼티 추가, 1P 전용 Niagara 에셋 생성 (작은 반지름) |
| 빔이 끝점 초과 | Niagara에 빔 길이 제한 파라미터 없음 | C++에서 `BeamLength` float 파라미터 전달, Niagara에서 파티클 이동 거리 제한 |
