# Plan2 — Phase3 웨이브 레벨 외곽선 머티리얼 상세 구성

## 0. 개요

### 목표
페이즈3의 적이 어떤 웨이브 레벨에서 스폰됐는지 외곽선 색으로 즉시 구분 가능하게 함.

### 레벨별 색 매핑
| Wave Level | Custom Depth Stencil 값 | 외곽선 색 | RGB (Linear, 권장) |
|---|---|---|---|
| 1 | 1 | **없음** | — |
| 2 | 2 | 초록 | (0.0, 1.0, 0.1) |
| 3 | 3 | 노랑 | (1.0, 1.0, 0.0) |
| 4 | 4 | 주황 | (1.0, 0.4, 0.0) |
| 5 | 5 | 빨강 | (1.0, 0.0, 0.0) |

> Wave 1 적도 C++에서 stencil 값 1이 찍히지만, **머티리얼 단에서 마스크로 걸러서 외곽선을 그리지 않음**.

### 작업 산출물
1. `M_PP_WaveOutline` (Material, Post Process Domain)
2. `MI_PP_WaveOutline` (Material Instance, 디자이너용 색 조정)
3. PostProcessVolume 액터에 인스턴스 연결

### 저장 경로 권장
- `Content/Materials/PostProcess/M_PP_WaveOutline.uasset`
- `Content/Materials/PostProcess/MI_PP_WaveOutline.uasset`

---

## 1. 사전 준비 (필수)

### 프로젝트 세팅
**Edit → Project Settings → Rendering → Postprocessing**
- `Custom Depth-Stencil Pass` → **"Enabled with Stencil"**
- 변경 후 에디터 재시작

이 설정이 없으면 `SceneTexture: CustomStencil` 노드의 출력이 항상 0이 됨.

---

## 2. 머티리얼 본체 (M_PP_WaveOutline) 생성

Content Browser → 우클릭 → **Material** → 이름 `M_PP_WaveOutline`.

### 머티리얼 노드 (Main Output) Details
| 속성 | 값 |
|---|---|
| Material Domain | **Post Process** |
| Blendable Location | **Before Tonemapping** |
| Blendable Priority | 0 (기본) |
| Output Alpha | 체크 해제 |

> Before Tonemapping을 쓰는 이유: Emissive로 색을 강하게 줄 때 톤매퍼가 적용되어 자연스러운 글로우/블룸으로 이어짐. After Tonemapping은 색이 정확하지만 LDR 클램프 때문에 빛나는 느낌이 약해짐.

---

## 3. 파라미터 정의

머티리얼 그래프 빈 공간에 우클릭 → 검색 `Vector Parameter` / `Scalar Parameter`로 추가.
**이름 정확히 일치 필수** (인스턴스 매칭됨).

### Vector Parameters (4개)
| 이름 | Group | Default Value (R, G, B, A) |
|---|---|---|
| `Color_Lv2` | OutlineColors | (0.0, 1.0, 0.1, 1.0) — 초록 |
| `Color_Lv3` | OutlineColors | (1.0, 1.0, 0.0, 1.0) — 노랑 |
| `Color_Lv4` | OutlineColors | (1.0, 0.4, 0.0, 1.0) — 주황 |
| `Color_Lv5` | OutlineColors | (1.0, 0.0, 0.0, 1.0) — 빨강 |

> Wave1용 `Color_Lv1`은 필요 없음 (마스크에서 제외됨).

### Scalar Parameters (2개)
| 이름 | Group | Default | 설명 |
|---|---|---|---|
| `OutlineThickness` | OutlineSettings | 1.0 | 외곽선 두께 (픽셀 단위 배수) |
| `OutlineIntensity` | OutlineSettings | 3.0 | Emissive 강도 (1=원색, 5+=빛남) |

---

## 4. 노드 그래프 (단계별 상세)

### 전체 데이터 플로우
```
[① 픽셀 오프셋 계산]
        ↓ TexelOffset (float2)
[② 5방향 스텐실 샘플링 (Center, Up, Down, Left, Right)]
        ↓ CenterStencil, NeighborMax (각 float, 0~5)
[③ 외곽선 마스크 계산 (Wave1 제외)]
        ↓ EdgeMask (float, 0 or 1)
[④ NeighborMax → 색상 매핑]
        ↓ SelectedColor (float3)
[⑤ 원본 씬과 Lerp]
        ↓ FinalColor → Emissive Color (Main Output)
```

---

### ① 픽셀 오프셋 계산

**목적**: 1픽셀 만큼의 UV 오프셋을 만들고, `OutlineThickness`로 스케일.

#### 노드 A1: SceneTexture (참조용 — InvSize 얻기)
- 노드 추가: 우클릭 → 검색 `SceneTexture`
- Details 패널:
  - **Scene Texture Id**: `CustomStencil`
- 사용할 출력 핀: `InvSize` (float2) — Color/Size 핀은 미연결

#### 노드 A2: Scalar Parameter `OutlineThickness`
- §3에서 만든 파라미터를 그래프로 드래그 (또는 우클릭 → 검색 `Scalar Parameter`로 새로 만들고 이름 변경)

#### 노드 A3: Multiply
- 노드: `Multiply`
- **A 입력** ← `SceneTexture (A1)`의 `InvSize`
- **B 입력** ← `OutlineThickness (A2)` (float이 자동 broadcast됨)
- **출력 (TexelOffset, float2)** — 이후 ②에서 재사용

> 팁: 결과 핀 우클릭 → "Promote to Local" 대신, 선을 끌어 빈 공간에 놓고 `Reroute` 노드를 만들어 정리하면 그래프가 깔끔해짐.

---

### ② 5방향 스텐실 샘플링

**목적**: 현재 픽셀(Center) 과 4방향 이웃(Up/Down/Left/Right)의 CustomStencil 값을 각각 읽어옴.

#### 공통 노드 B0: TexCoord
- 노드: `TextureCoordinate`
- Details: `Coordinate Index = 0`
- 출력 1개를 5군데에서 분기해서 사용

#### B-Center: 중심 픽셀 (오프셋 없음)
1. **SceneTexture** (Scene Texture Id = `CustomStencil`)
   - UVs ← `TexCoord (B0)`
2. **ComponentMask**
   - Details: **R 체크**, GBA 해제
   - 입력 ← SceneTexture의 `Color` 출력
   - 출력 = **CenterStencil** (float)

#### B-Right: 우측 1픽셀
1. **Constant2Vector**: (X=1.0, Y=0.0)
2. **Multiply**: A=Constant2Vector, B=TexelOffset (①의 A3) → DirOffset_R
3. **Add**: A=TexCoord (B0), B=DirOffset_R → SampleUV_R
4. **SceneTexture** (CustomStencil), UVs ← SampleUV_R
5. **ComponentMask** (R) → **RightStencil**

#### B-Left: 좌측 1픽셀
1. **Constant2Vector**: (X=-1.0, Y=0.0)
2~5. 동일 패턴 → **LeftStencil**

#### B-Up: 위 1픽셀
1. **Constant2Vector**: (X=0.0, Y=-1.0)  ※ UV는 Y 아래 방향이 +
2~5. 동일 패턴 → **UpStencil**

#### B-Down: 아래 1픽셀
1. **Constant2Vector**: (X=0.0, Y=1.0)
2~5. 동일 패턴 → **DownStencil**

> **실작업 팁**: B-Right 한 세트를 만든 뒤 노드 5개를 통째로 복사(Ctrl+C / Ctrl+V) × 3, Constant2Vector 값만 변경하는 식으로 진행. 모든 SceneTexture는 `CustomStencil`, 모든 ComponentMask는 `R`만.

#### 산출 변수 (다음 단계에서 사용)
- `CenterStencil`, `UpStencil`, `DownStencil`, `LeftStencil`, `RightStencil` (각 float)

---

### ③ 외곽선 마스크 계산 (Wave1 제외 로직)

**목적**: "외곽선을 그려야 할 픽셀"인지 0/1 마스크 산출.

**판정 식**:
```
EdgeMask = (CenterStencil < 2) AND (NeighborMax >= 2)
```
- Wave 1 (stencil=1)은 "외곽선 대상 아님" 으로 취급 → 배경(0)과 같이 분류
- Wave 2 이상 (stencil >= 2) 이 이웃에 있을 때만 외곽선

#### 노드 C1: NeighborMax 계산
- **Max** (#1): A=UpStencil, B=DownStencil → Max_UD
- **Max** (#2): A=LeftStencil, B=RightStencil → Max_LR
- **Max** (#3): A=Max_UD, B=Max_LR → **NeighborMax** (float, 0~5)

#### 노드 C2: NeighborMask (NeighborMax >= 2)
부동소수 비교를 안전하게 하려고 임계값 `1.5`로 잡음.
- **Subtract**: A=NeighborMax, B=Constant `1.5` → diff (NeighborMax가 2 이상이면 양수)
- **Saturate**: 입력=diff → satDiff (0~1로 클램프, 양수는 1로 수렴)
- **Ceil**: 입력=satDiff → **NeighborMask** (0 또는 1)

#### 노드 C3: CenterMask (CenterStencil < 2)
- **Subtract**: A=Constant `1.5`, B=CenterStencil → diff2 (Center가 1 이하면 양수)
- **Saturate**: 입력=diff2 → satDiff2
- **Ceil**: 입력=satDiff2 → **CenterMask** (Center<=1이면 1, 아니면 0)

#### 노드 C4: EdgeMask 결합
- **Multiply**: A=CenterMask, B=NeighborMask → **EdgeMask** (0 or 1)

> 비교 노드 대안: Material Editor의 **If** 노드를 써도 됨.
> - `If`: A=NeighborMax, B=Constant(1.5), `A>B`=1, `A==B`=1, `A<B`=0 → NeighborMask
> - 위 Subtract→Saturate→Ceil 트릭이 분기 노드 없이 더 단순함.

---

### ④ NeighborMax → 색상 매핑

**목적**: NeighborMax 값(2, 3, 4, 5)에 따라 4개 색 중 하나 선택.

#### 권장 방법: Custom HLSL 노드 1개

##### 노드 D1: Custom
- 노드 추가: 우클릭 → 검색 `Custom`
- Details:
  - **Output Type**: `CMOT Float 3`
  - **Description**: `Pick outline color by wave level`
- Inputs (좌측 + 버튼으로 추가. 이름·타입 정확히):
  | Name | Type |
  |---|---|
  | `Level` | Float 1 |
  | `C2` | Float 3 |
  | `C3` | Float 3 |
  | `C4` | Float 3 |
  | `C5` | Float 3 |
- Code (Details의 Code 필드에 입력):
  ```hlsl
  if (Level >= 4.5) return C5;
  if (Level >= 3.5) return C4;
  if (Level >= 2.5) return C3;
  return C2;
  ```
- 연결:
  - `Level` ← NeighborMax (③의 C1 결과)
  - `C2` ← `Color_Lv2` VectorParameter (Vector4 → Float3 자동 변환)
  - `C3` ← `Color_Lv3`
  - `C4` ← `Color_Lv4`
  - `C5` ← `Color_Lv5`
- 출력 = **SelectedColor** (float3)

> VectorParameter는 float4(RGBA)를 출력하지만 Custom 노드의 float3 입력에 바로 꽂으면 Alpha가 무시되며 자동 변환됨. 명시적으로 하고 싶으면 `ComponentMask(RGB)`로 감싸도 됨.

#### 대체 방법: If 노드 3중첩 (Custom 안 쓰고 싶을 때)
1. **If (#1)**: A=NeighborMax, B=Constant `4.5`
   - `A>B` → Color_Lv5
   - `A==B` → Color_Lv5
   - `A<B` → If(#2) 결과
2. **If (#2)**: A=NeighborMax, B=Constant `3.5`
   - `A>B` / `A==B` → Color_Lv4
   - `A<B` → If(#3) 결과
3. **If (#3)**: A=NeighborMax, B=Constant `2.5`
   - `A>B` / `A==B` → Color_Lv3
   - `A<B` → Color_Lv2
4. 최종 출력 = If(#1) 의 결과 → **SelectedColor**

> Material Editor의 `If` 노드 슬롯: `A`, `B`, `A>B`, `A==B`, `A<B`. `A>B`와 `A==B`에 같은 값을 연결하면 `>=` 효과.

---

### ⑤ 최종 합성

**목적**: 원본 씬 컬러 위에 SelectedColor를 EdgeMask에 따라 덧입힘.

#### 노드 E1: 원본 씬 컬러 샘플
- **SceneTexture** 노드
- Details: **Scene Texture Id** = `PostProcessInput0`
- 출력 `Color` (float4) → **ComponentMask (RGB)** 로 감싸 → **SceneColor** (float3)

#### 노드 E2: 외곽선 색 × 강도
- **Multiply**:
  - A ← SelectedColor (④의 D1 결과, float3)
  - B ← `OutlineIntensity` (Scalar Parameter, float이 broadcast됨)
- 출력 = **OutlineColorBoosted** (float3)

#### 노드 E3: Lerp (조건부 합성)
- **LinearInterpolate** (Lerp) 노드:
  - **A** ← SceneColor (E1)
  - **B** ← OutlineColorBoosted (E2)
  - **Alpha** ← EdgeMask (③의 C4 결과)
- 출력 = **FinalColor** (float3)

#### 노드 E4: Main Output 연결
- `FinalColor` → **Emissive Color** 핀
- 다른 출력 핀 (Base Color, Metallic, Roughness, …) 은 미연결 (Post Process Domain에서는 무시됨)

---

## 4.5. 가림(Occlusion) 처리 — 벽/다른 적 뒤에 있을 때 외곽선 숨기기

### 4.5.0 개요

**현재 상태**: §4까지의 그래프는 CustomStencil만으로 외곽선을 그리므로, 적이 벽이나 다른 오브젝트 뒤에 있어도 외곽선이 그대로 보인다 ("X-Ray" 효과).

**목표**: 적이 다른 불투명 오브젝트(벽, 지형, 다른 적)에 가려져 있는 부분의 외곽선은 그리지 않게 한다.

### 4.5.1 원리

| 텍스처 | 의미 |
|---|---|
| `SceneTexture: CustomDepth` | 외곽선 대상(`RenderCustomDepth=true` 인 메쉬) 의 깊이만 저장. 가려짐 무시. |
| `SceneTexture: SceneDepth` | 화면에 실제 보이는 픽셀의 깊이. 가장 앞 불투명 오브젝트의 깊이. |

같은 UV에서 두 값을 비교:
- `SceneDepth ≥ CustomDepth - ε` → 적이 장면에서 가장 앞에 있음 (또는 비슷한 위치) → **보임** → 외곽선 그림
- `SceneDepth < CustomDepth - ε` → 적보다 앞에 무언가가 있음 → **가려짐** → 외곽선 숨김

> ε(epsilon)은 깊이 정밀도 오차 보정용 작은 양수. UE 단위(cm) 기준 보통 1~10. 너무 크면 얇은 벽을 통과해서 보임, 너무 작으면 자기 자신을 가린 것으로 판정되어 깜빡임.

### 4.5.2 추가할 파라미터 (§3에 추가)

#### Scalar Parameter (1개 추가)
| 이름 | Group | Default | 설명 |
|---|---|---|---|
| `OcclusionEpsilon` | OutlineSettings | 5.0 | 깊이 비교 허용 오차 (cm). 깜빡임 발생 시 ↑, 얇은 벽 통과 시 ↓ |

### 4.5.3 수정해야 할 기존 노드 — §4-② 5방향 스텐실 샘플링

§4-②의 **각 방향 샘플(B-Right, B-Left, B-Up, B-Down) 4세트**에 동일한 추가 작업을 한다. **B-Center는 수정하지 않음** (이유: §4.5.5).

기존 B-Right 노드 체인 (수정 전):
```
Constant2Vector(1,0) → Multiply(×TexelOffset) → Add(+TexCoord) = SampleUV_R
SampleUV_R → SceneTexture(CustomStencil) → ComponentMask(R) = RightStencil
```

수정 후 (B-Right 기준, 다른 방향도 동일 패턴):
```
                              ┌→ SceneTexture(CustomStencil) → ComponentMask(R) = Stencil_R
SampleUV_R(기존) ─────────────┼→ SceneTexture(CustomDepth)   → ComponentMask(R) = CustomDepth_R
                              └→ SceneTexture(SceneDepth)    → ComponentMask(R) = SceneDepth_R

[가림 판정]
Subtract: A=SceneDepth_R + OcclusionEpsilon, B=CustomDepth_R → DepthDiff_R
Saturate(DepthDiff_R) → satDD_R
Ceil(satDD_R) → VisibilityMask_R   (보이면 1, 가려지면 0)

[기존 Stencil_R에 가림 마스크 곱하기]
Multiply: A=Stencil_R, B=VisibilityMask_R → RightStencil   (이름은 그대로 유지)
```

#### 단계별 노드 추가 (B-Right 예시)

기존 노드는 그대로 두고, **SampleUV_R 핀에서 분기**해서 다음을 추가:

1. **SceneTexture #2** (방향마다 1개씩 추가)
   - Details: **Scene Texture Id** = `CustomDepth`
   - UVs ← SampleUV_R (기존 Add 노드의 출력)
2. **ComponentMask** (R 체크)
   - 입력 ← 위 SceneTexture #2 의 `Color`
   - 출력 = **CustomDepth_R** (float)

3. **SceneTexture #3** (방향마다 1개씩 추가)
   - Details: **Scene Texture Id** = `SceneDepth`
   - UVs ← SampleUV_R
4. **ComponentMask** (R 체크)
   - 입력 ← 위 SceneTexture #3 의 `Color`
   - 출력 = **SceneDepth_R** (float)

5. **Add**
   - A ← SceneDepth_R
   - B ← `OcclusionEpsilon` (Scalar Parameter)
   - 출력 = SceneDepthBiased_R

6. **Subtract**
   - A ← SceneDepthBiased_R
   - B ← CustomDepth_R
   - 출력 = DepthDiff_R  (보이면 양수, 가려지면 음수)

7. **Saturate**
   - 입력 ← DepthDiff_R
   - 출력 = satDD_R  (0~1로 클램프)

8. **Ceil**
   - 입력 ← satDD_R
   - 출력 = **VisibilityMask_R**  (0 또는 1)

9. **Multiply** (기존 ComponentMask(R) → RightStencil 사이에 끼워넣기)
   - 기존: `Stencil_R(ComponentMask R 출력) → (이후 ③의 Max 체인으로)`
   - 수정: `Stencil_R → Multiply(A=Stencil_R, B=VisibilityMask_R) → RightStencil → (이후 ③의 Max 체인으로)`
   - **즉, 기존의 RightStencil 이름이 가리키는 핀이 Multiply의 출력으로 바뀜.** ③ 이후 그래프는 수정 불필요.

#### 4방향 전체 작업

위 B-Right 작업을 **B-Left / B-Up / B-Down 에도 동일하게 반복**. 각 방향에 추가되는 노드:
- SceneTexture × 2 (CustomDepth, SceneDepth)
- ComponentMask × 2
- Add × 1 (epsilon bias)
- Subtract × 1
- Saturate × 1
- Ceil × 1
- Multiply × 1

= 방향당 **9개**, 4방향 = **36개** 추가.

> 실작업 팁: B-Right에서 9개 노드 세트를 만들고 통째로 복사해서 3방향에 붙여넣기. 연결만 각 방향의 SampleUV / Stencil 핀으로 다시 잡아주면 됨.

### 4.5.4 ③·④·⑤ 단계는 수정 불필요

기존 ③ 마스크 계산은 `UpStencil/DownStencil/LeftStencil/RightStencil` 이름의 핀을 그대로 받음. 위 수정에서 각 방향의 최종 출력 이름은 그대로 `XxxStencil` 이므로 **③ 이후의 그래프는 한 줄도 손대지 않아도 됨**.

NeighborMax 계산 시 가려진 방향의 stencil이 0이 되므로, 가려진 적의 외곽선은 그 가려진 부분에서만 자연스럽게 사라진다.

### 4.5.5 왜 Center는 수정하지 않는가

**EdgeMask = (CenterStencil < 2) AND (NeighborMax >= 2)** 에서 Center는 "배경(stencil=0 또는 1)이어야 한다"는 조건이다.

- 외곽선이 그려질 픽셀의 Center는 항상 배경(벽/지형) 위에 있다. 즉 그 픽셀의 CustomDepth는 무효(이 픽셀에는 CustomDepth가 기록되지 않음, 기본값은 매우 큼).
- Center에 visibility를 적용하면 의미 있는 정보가 안 나옴.
- 가림 판정은 "이웃 픽셀(적 실루엣) 이 실제 보이는가" 로 판단하면 충분하다.

### 4.5.6 엣지 케이스 동작 정리

| 상황 | 동작 | 이유 |
|---|---|---|
| 적이 벽 뒤 완전히 가려짐 | 모든 방향 VisibilityMask=0 → NeighborMax=0 → EdgeMask=0 → 외곽선 없음 | 의도 |
| 적의 절반만 벽 뒤 | 가려진 쪽 픽셀에서만 외곽선 사라짐, 노출된 쪽은 정상 | 의도 |
| 적 A가 적 B 앞에 있음 (둘 다 stencil) | B의 가려진 부분은 외곽선 없음. A는 정상. | 정상 — A의 CustomDepth가 SceneDepth와 같음 |
| 반투명 오브젝트(유리, 파티클) 뒤 | 가림 처리 **안 됨** (외곽선 보임) | 반투명은 SceneDepth에 기록 안 됨. 의도된 한계. |
| 같은 적의 같은 픽셀에 자기 자신 | OcclusionEpsilon 덕에 ≈동일 깊이로 판정, VisibilityMask=1 | epsilon이 너무 작으면 자기 자신을 가린 것으로 오판 → 깜빡임 |

### 4.5.7 튜닝 가이드

- **외곽선이 깜빡임 (자기 자신을 가린 것으로 판정)**: `OcclusionEpsilon` 을 5 → 10~20 으로 올림
- **얇은 벽 너머가 그대로 보임**: `OcclusionEpsilon` 을 5 → 2 로 낮춤
- **카메라가 멀리 있을 때 깜빡임 심함**: depth precision이 떨어지는 거리 — epsilon을 거리 기반으로 만들거나 `PixelDepth` 사용 검토 (지금은 과한 작업)
- **VisibilityMask 디버깅**: ⑤의 Emissive 핀에 임시로 `VisibilityMask_R` 만 흑백으로 출력해서 가림 판정이 맞는지 시각 확인

### 4.5.8 노드 추가 개수 요약

| 추가/수정 | 개수 |
|---|---|
| ScalarParameter `OcclusionEpsilon` | +1 |
| 4방향 가림 판정 노드 세트 (방향당 9개) | +36 |
| **합계** | **+37** |

§9의 총 노드 수가 약 48 → **약 85개**가 된다. 복사-붙여넣기로 작업하면 실제 손은 방향당 9노드 1세트만 만들면 됨.

---

## 5. 머티리얼 컴파일 & 인스턴스 생성

### 컴파일
- 상단 툴바 **Apply** → **Save**
- 좌하단 Stats 패널에서 에러 0 확인
- 워닝 "Material is missing a Scene Texture node" 같은 건 정상 동작

### 인스턴스 생성
- Content Browser에서 `M_PP_WaveOutline` 우클릭 → **Create Material Instance**
- 이름: `MI_PP_WaveOutline`
- 더블클릭으로 열면 파라미터들이 그룹별로 정렬되어 보임:
  - **OutlineColors** 그룹: 4개 색
  - **OutlineSettings** 그룹: Thickness, Intensity
- 각 파라미터 좌측 체크박스를 켜면 오버라이드 활성화 → 값 변경 가능

### 권장 초기값 (인스턴스에서 오버라이드, HDR 포함)
| 파라미터 | 값 |
|---|---|
| Color_Lv2 | (0.0, 1.5, 0.2) — 초록, HDR로 살짝 빛남 |
| Color_Lv3 | (1.5, 1.5, 0.0) — 노랑, HDR |
| Color_Lv4 | (2.0, 0.7, 0.0) — 주황, HDR |
| Color_Lv5 | (3.0, 0.0, 0.0) — 빨강, 가장 강조 |
| OutlineThickness | 1.5 |
| OutlineIntensity | 3.0 |

> HDR 색(>1)을 쓰면 Bloom과 어우러져 "빛나는 외곽선" 느낌이 강해짐. 톤은 디자이너 취향대로 조정.

---

## 6. PostProcessVolume 설정

### 적용할 맵
- `TestMap1.umap` (개발 테스트용)
- 페이즈3가 실행되는 모든 정식 맵

### 작업 순서
1. World Outliner에서 기존 PostProcessVolume 확인 (있으면 재사용)
2. 없으면 **Place Actors → Visual Effects → Post Process Volume** 드래그
3. Details 패널:
   - **Post Process Volume Settings → Infinite Extent (Unbound)** ✅ 체크
   - **Priority**: 0 (또는 기존 볼륨보다 높게)
4. **Rendering Features → Post Process Materials**:
   - Array 옆 `+` 클릭 → 새 항목 추가
   - 드롭다운에서 **Asset reference** 선택
   - 우측 슬롯에 `MI_PP_WaveOutline` 할당
   - Weight = 1.0 (기본)

### 주의
- **반드시 인스턴스(MI_)를 할당.** 본체 머티리얼을 넣으면 파라미터 기본값으로 그려지고 디자이너가 인스턴스에서 한 조정이 반영되지 않음.
- 여러 PostProcessVolume이 겹치면 우선순위가 높은 것의 Material Stack이 합쳐짐. 외곽선은 거의 항상 켜져 있어야 하므로 Unbound 글로벌 볼륨에 넣는 게 안전.

---

## 7. 검증 체크리스트

### 단일 PIE 테스트
- [ ] 페이즈3 진입 → Wave 1 적 스폰됨 → **외곽선 없음** ✅ (의도)
- [ ] CleanserSite 체력 50% 이하 → Wave 2 적 스폰 → **초록 외곽선**
- [ ] 계속 진행 → Wave 3 적 스폰 → **노란 외곽선**
- [ ] Wave 4 → **주황 외곽선**
- [ ] Wave 5 → **빨간 외곽선**
- [ ] Elite Boss도 현재 웨이브와 같은 색 외곽선
- [ ] Wave 2 적이 살아있는 상태에서 Wave 3 진입 → 살아있던 적은 계속 **초록**, 새로 스폰되는 적만 **노랑** ✅

### 멀티플레이 테스트 (Listen Server 2인)
- [ ] 호스트 화면과 클라이언트 화면에서 **같은 색**으로 보임
- [ ] 클라이언트 후입장 시에도 이미 스폰된 적의 외곽선 색 정상 (COND_InitialOnly 동작 확인)

### 비페이즈3 적
- [ ] 페이즈1/2의 적은 외곽선 없음 (`WaveOutlineLevel = 0` 기본값)
- [ ] 튜토리얼 더미도 외곽선 없음

### 시각 품질
- [ ] 적이 벽에 완전히 가려지면 외곽선 사라짐 (§4.5 가림 처리 동작 확인)
- [ ] 적의 절반만 벽 뒤에 있으면 그 부분만 외곽선이 끊어짐
- [ ] 적 A가 적 B 앞에 있을 때, B의 가려진 부분만 외곽선 없음
- [ ] 외곽선이 깜빡이지 않는지 (깜빡이면 `OcclusionEpsilon` 상향)
- [ ] 두 적이 겹쳐 있을 때 서로 다른 레벨이면 경계가 어느 색으로 그려지는지 (NeighborMax가 더 큰 값 우선)
- [ ] 카메라 거리가 멀어졌을 때 외곽선이 너무 굵게 보이지 않는지 (필요시 거리 기반 Thickness 감쇠 추가)

---

## 8. 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| 외곽선이 전혀 안 보임 | Project Settings의 Custom Depth-Stencil Pass가 "Enabled with Stencil"인지 확인 |
| 색은 보이는데 항상 같은 색 | NeighborMax 연결 누락 또는 Custom 노드의 If 분기 임계값 오타 |
| Wave1 적에도 외곽선 보임 | ③의 CenterMask 또는 NeighborMask 비교 임계값이 `1.5` 아닌 `0.5`로 들어감 |
| 클라이언트에서만 안 보임 | C++의 `OnRep_WaveOutlineLevel`이 호출되는지 로그 추가 / `ApplyWaveOutline`에서 GetMesh() null 체크 |
| 외곽선이 깜빡임 | TAA 때문일 수 있음 — OutlineThickness를 1.5+로 올리거나 콘솔에서 `r.PostProcessAAQuality` 조정 |
| 너무 굵음 | OutlineThickness를 0.5~1.0으로 낮춤 |
| 너무 안 빛남 | OutlineIntensity 5~10으로 / 색 자체를 HDR(>1)로 |
| 빨강만 보임 | Custom 노드 if 순서 오류 — `>= 4.5` 가 가장 위에 와야 함 |
| 가림 처리 후 외곽선 깜빡임 | `OcclusionEpsilon`을 5 → 10~20으로 상향 (자기 자신 가림 오판 방지) |
| 벽 뒤 적의 외곽선이 그대로 보임 | §4.5.3 Multiply 연결 누락 — VisibilityMask가 Stencil에 곱해지지 않음. 4방향 전부 확인 |
| 얇은 벽 뒤 적이 보임 | `OcclusionEpsilon`을 5 → 1~2로 낮춤 |
| 가림은 잘 되는데 적 자기 자신이 외곽선 안 그림 | epsilon이 음수가 됐거나 Add/Subtract A,B가 뒤바뀜 — `SceneDepth + ε ≥ CustomDepth` 식 확인 |

---

## 9. 노드 총 개수 (대략)

| 섹션 | 노드 수 |
|---|---|
| ① 픽셀 오프셋 | 3 (SceneTexture, ScalarParam, Multiply) |
| ② 5방향 샘플링 | TexCoord 1 + (Constant2Vector + Multiply + Add + SceneTexture + ComponentMask) × 4 + Center 2 ≈ 23 |
| ③ 마스크 계산 | Max 3 + (Subtract+Saturate+Ceil) × 2 + Constant 2 + Multiply 1 ≈ 12 |
| ④ 색상 매핑 | Custom 1 + VectorParameter 4 = 5 |
| ⑤ 최종 합성 | SceneTexture 1 + ComponentMask 1 + Multiply 1 + Lerp 1 + ScalarParam 1 = 5 |
| **합계** | **약 48개** |

If 노드 방식으로 ④를 구현하면 +3개. Reroute 노드로 정리하면 시각적으로는 훨씬 깔끔.
