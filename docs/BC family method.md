# 현재 BC1 Compression-Domain Mipmap 생성 방법

이 문서는 `src/backend_cpu.cpp`, `src/codec/bc1.cpp`, `src/codec/bc1.h`에 현재 구현된 방법을 설명한다. 이론적으로 가능한 전체 BC-family 알고리즘이 아니라, **현재 코드가 실제로 수행하는 BC1 처리 순서**를 기준으로 한다.

## 1. 입력과 출력

현재 구현에는 자식 BC1 블록을 만드는 경로가 두 개 있고, 두 경로는 뒷부분의 encoder core를 공유한다.

### 경로 A: symbolic 경로 (`ProcessRowBC1()`, mip 1 전용)

입력은 이전 mip level의 2×2 부모 BC1 블록이다.

```text
p00  p10
p01  p11
```

- 부모 블록 하나는 4×4 texel을 표현한다.
- 부모 블록 네 개는 복호화했을 때 8×8 texel 영역을 표현한다.
- 각 부모 블록 내부를 2×2 texel quadrant 네 개로 나눈다.
- 총 16개의 quadrant 평균을 구하면, 이것들이 자식 블록의 4×4 texel 16개가 된다.
- 출력은 이 16개 값을 근사하여 인코딩한 BC1 블록 하나이다.
- 부수적으로 각 부모 블록의 4×4 linear RGB 평균을 block-mean image $M$에 기록한다.

### 경로 B: linear 경로 (`ProcessLinearRowBC1()`, mip 2 이상)

입력은 압축 블록이 아니라 linear RGB mean image이다. 이 image의 texel 격자는 생성하려는 mip level의 texel 격자와 1:1로 일치한다.

- 자식 블록 하나의 4×4 texel을 mean image에서 그대로 읽어온다.
- 범위를 벗어나는 좌표는 clamp-to-edge로 읽는다.
- 출력은 경로 A와 동일한 BC1 블록 하나이다.

두 경로 모두 CPU에서는 서로 독립적인 자식 블록 네 개를 `uint4`/`float4`의 `x`, `y`, `z`, `w` lane에 넣고 같은 연산을 수행한다. 현재 CPU의 `uint4`와 `float4`는 실제 SIMD intrinsic이 아니라 4-lane 형태의 scalar 연산이다.

## 2. 사용 데이터 타입

### 2.1 CPU와 Slang 공용 기본 타입

`bc1.h`는 같은 핵심 encode 코드를 C++과 Slang에서 공유하기 위해 다음 타입을 사용한다.

| 타입 | CPU C++ 정의 | Slang에서의 의미 | 용도 |
|---|---|---|---|
| `uint8_t` | 8-bit unsigned integer | CPU wrapper에서만 사용 | 압축 block buffer의 byte pointer |
| `uint16_t` | 16-bit unsigned integer | BC1 scalar 구조체의 endpoint | RGB565 endpoint 한 개 |
| `ushort` | `uint16_t` alias | Slang의 `ushort` | 16-bit 값 변환 및 저장 |
| `uint` | `uint32_t` alias | Slang의 32-bit `uint` | selector field와 일반 정수 연산 |
| `float` | 32-bit floating point | Slang의 32-bit `float` | linear RGB와 통계 계산 |
| `uint4` | 네 개의 `uint`를 가진 자체 구조체 | Slang built-in `uint4` | 독립적인 BC1 작업 네 개의 정수 값 |
| `float4` | 네 개의 `float`를 가진 자체 구조체 | Slang built-in `float4` | 독립적인 BC1 작업 네 개의 실수 값 |

여기서 `uint4`와 `float4`의 lane은 RGBA 채널이 아니다. `x`, `y`, `z`, `w` 각각이 **서로 다른 자식 BC1 블록 하나**를 담당한다. RGB 채널은 `q0_r`, `q0_g`, `q0_b`처럼 별도의 `float4` 변수로 분리되어 있다. 따라서 현재 자료 배치는 네 작업을 병렬화하기 위한 Structure of Arrays 형태이다.

CPU에서 `uint4`와 `float4`는 연산자 overload를 가진 scalar 구조체다. Slang에서는 같은 이름의 native vector type을 사용한다. `INOUT`, `OUT`, `SHARED_INLINE` macro는 함수 본문을 공유하기 위한 함수 인자 및 inline 표기 차이만 흡수한다.

CPU 경로에는 sRGB 8-bit code value를 linear로 바꾸는 256개짜리 lookup table `kSrgb8ToLinear`가 있다. Slang 경로에서는 같은 변환을 해석적으로 계산한다.

### 2.2 BC1 block 타입

```cpp
struct SymbolicDataBC1
{
    uint16_t color_0;
    uint16_t color_1;
    uint selectors;
};
```

`SymbolicDataBC1`은 압축된 BC1 block 하나를 나타내며 총 8 byte이다.

| member | 크기 | 의미 |
|---|---:|---|
| `color_0` | 16 bit | 첫 번째 RGB565 endpoint |
| `color_1` | 16 bit | 두 번째 RGB565 endpoint |
| `selectors` | 32 bit | 4×4 texel의 2-bit selector 16개 |

`src_blocks`와 `dst_blocks`는 외부에서는 `uint8_t*` byte buffer로 전달되고, `ProcessRowBC1()`과 `ProcessLinearRowBC1()` 안에서 `SymbolicDataBC1*`로 해석된다.

### 2.3 4-lane BC1 block 타입

```cpp
struct SymbolicDataBC1x4
{
    uint4 color_0;
    uint4 color_1;
    uint4 selectors;
};
```

`SymbolicDataBC1x4`는 서로 독립적인 BC1 block 네 개를 lane별로 묶은 표현이다. 각 `color_0`과 `color_1` lane에는 유효한 하위 16 bit RGB565 값이 들어가고, 각 `selectors` lane에는 해당 block의 32-bit selector field가 들어간다.

```text
lane x → child block 0
lane y → child block 1
lane z → child block 2
lane w → child block 3
```

### 2.4 Linear palette 타입

```cpp
struct LinearPaletteBC1
{
    float4 c0_r, c0_g, c0_b;
    float4 c1_r, c1_g, c1_b;
    float4 c2_r, c2_g, c2_b;
    float4 c3_r, c3_g, c3_b;
};
```

`LinearPaletteBC1`은 네 BC1 작업의 palette 네 색을 linear RGB로 보관한다. 예를 들어 `c2_g.z`는 lane `z`에 들어 있는 block의 palette index 2에 대한 linear green 값이다. 모든 채널 값의 범위는 기본적으로 `[0, 1]`이다.

### 2.5 Quadrant 평균 타입

```cpp
struct QuadrantMeans
{
    float4 q0_r, q0_g, q0_b;
    float4 q1_r, q1_g, q1_b;
    float4 q2_r, q2_g, q2_b;
    float4 q3_r, q3_g, q3_b;
};
```

`QuadrantMeans`는 자식 블록의 2×2 texel 그룹 하나에 들어가는 네 값을 저장한다. `p00`, `p10`, `p01`, `p11` 네 개가 모여 자식 블록의 4×4 texel 16개를 이룬다.

두 경로에서 내용의 의미가 다르다.

- 경로 A에서는 각 값이 부모 BC1 블록 한 개의 2×2 quadrant를 linear RGB로 평균한 값이다.
- 경로 B에서는 각 값이 mean image에서 읽어온 texel 값 그 자체다.

구조체 하나가 lane별로 네 작업을 표현하므로 두 경로 모두 네 개의 `QuadrantMeans`를 사용한다.

### 2.6 Covariance 타입

```cpp
struct CovarianceMatrix
{
    float4 rr, gg, bb;
    float4 rg, rb, gb;
};
```

covariance matrix는 대칭이므로 중복되는 성분을 제외한 여섯 성분만 저장한다.

```text
| rr  rg  rb |
| rg  gg  gb |
| rb  gb  bb |
```

각 member가 `float4`이므로 lane마다 독립적인 3×3 covariance matrix 하나를 나타낸다.

### 2.7 부모 통계 타입

```cpp
struct ParentStatistics
{
    float4 mean_r, mean_g, mean_b;
    CovarianceMatrix within_covariance;
};
```

`ParentStatistics`는 한 그룹에 묶인 네 lane 각각에 대해 값 네 개의 평균과 within-group covariance를 저장한다. 이 값 네 세트가 이후 ANOVA의 between/within 결합에 사용된다.

### 2.8 Block mean 타입

```cpp
struct BlockMeans
{
    float4 r, g, b;
};

struct LinearBlockMean
{
    float r, g, b;
};

struct SourceBlockMeans
{
    BlockMeans p00, p10, p01, p11;
};
```

- `BlockMeans`는 4-lane 형태의 block 평균이다. 네 lane 각각이 서로 다른 source block 하나의 linear RGB 평균을 담는다.
- `LinearBlockMean`은 같은 값을 lane 없이 scalar 세 개로 저장한 형태다. block-mean image $M$의 texel 타입이며 12 byte이다.
- `SourceBlockMeans`는 자식 블록 그룹 하나를 만드는 데 사용된 source block 네 개(`p00`, `p10`, `p01`, `p11`)의 평균을 한꺼번에 담는다. `ComputeChildBlockMoments()`가 이미 계산하는 중간값을 밖으로 내보내기 위한 출력 전용 구조체다.

### 2.9 중간 histogram과 endpoint 값

selector histogram 전용 구조체는 사용하지 않는다. `Extract2x2SelectorHistograms()`의 출력 네 개를 각각 `uint4`로 유지한다.

```text
hist_00 → selector 0의 quadrant별 개수
hist_01 → selector 1의 quadrant별 개수
hist_10 → selector 2의 quadrant별 개수
hist_11 → selector 3의 quadrant별 개수
```

각 lane의 한 `uint` 안에는 quadrant count가 다음 bit 위치에 pack되어 있다.

```text
bits  0..3  → quadrant 0 count
bits  4..7  → quadrant 1 count
bits 16..19 → quadrant 2 count
bits 20..23 → quadrant 3 count
```

PCA와 least-squares 단계의 평균, 축, endpoint도 별도 RGB vector 구조체 없이 채널별 `float4`로 유지한다. 예를 들어 `p0_r`, `p0_g`, `p0_b` 세 값이 네 작업의 첫 번째 linear RGB endpoint를 나타낸다.

## 3. 전체 처리 순서

두 경로는 자식 texel 16개를 만드는 앞부분만 다르고, 그 이후 encoder core는 완전히 같다.

```text
[경로 A: mip 1]                       [경로 B: mip 2 이상]
2×2 부모 BC1 블록 수집                  linear mean image 준비
    ↓                                      ↓
부모 selector를 quadrant histogram으로     자식 texel 좌표로 clamp fetch
    ↓                                      ↓
RGB565 endpoint로 hardware palette 생성    ─
    ↓                                      ↓
palette를 sRGB에서 linear RGB로 변환       ─
    ↓                                      ↓
histogram과 linear palette로 16개          16개 자식 texel 확보
자식 texel 평균 계산                        ↓
    ↓                                      │
block-mean image M에 부모 평균 기록          │
    └──────────────┬───────────────────────┘
                   ↓
        [공통 encoder core]
        ANOVA 방식으로 전체 평균과 covariance 계산
                   ↓
        PCA로 초기 endpoint 추정
                   ↓
        discrete selector를 사용한 least-squares endpoint 보정
                   ↓
        고정 4/9 chord-curve 보정
                   ↓
        linear endpoint를 sRGB로 변환하고 RGB565로 양자화
                   ↓
        strict opaque 조건(color0 > color1) 강제
                   ↓
        양자화된 endpoint로 hardware palette 재생성
                   ↓
        16개 자식 texel에 가장 가까운 selector 재할당
                   ↓
        BC1 block으로 pack하고 출력
```

## 4. 경로 A: 부모 블록 수집

`ProcessRowBC1()`은 자식 블록 좌표 `(x, y)`에 대해 이전 mip level에서 다음 네 부모 블록을 읽는다.

```text
p00 = source(2x,     2y)
p10 = source(2x + 1, 2y)
p01 = source(2x,     2y + 1)
p11 = source(2x + 1, 2y + 1)
```

source block 범위를 넘어가는 좌표는 마지막 유효 block 좌표로 clamp한다. 한 번의 encoder 호출에는 최대 네 개의 자식 블록 작업을 모아 각 lane에 배치한다. 마지막 묶음에서 실제 작업 수가 네 개보다 적으면 남은 lane은 마지막 유효 작업을 복제해 채우고, 출력은 유효한 lane만 저장한다.

## 5. 부모 selector의 quadrant histogram 계산

부모 BC1 블록의 32-bit selector field에는 texel당 2-bit selector가 저장되어 있다. selector 값은 BC1 hardware index 순서인 `0`, `1`, `2`, `3`을 그대로 사용한다.

`Extract2x2SelectorHistograms()`는 SWAR 연산으로 selector의 low bit와 high bit를 분리하고, 각 selector 값에 해당하는 bit flag를 만든다.

```text
selector 0: low = 0, high = 0
selector 1: low = 1, high = 0
selector 2: low = 0, high = 1
selector 3: low = 1, high = 1
```

그다음 `Count2x2Regions()`가 부모 블록 안의 각 2×2 quadrant에서 selector별 출현 횟수를 센다. 각 quadrant의 histogram은 다음 정보를 가진다.

```text
n0 + n1 + n2 + n3 = 4
```

즉, 부모 texel 16개를 직접 복원해 저장하지 않고 네 개의 quadrant histogram으로 요약한다.

## 6. 부모 BC1 hardware palette 생성

`BuildOpaqueLinearPaletteBC1()`는 부모 블록의 두 RGB565 endpoint를 먼저 8-bit sRGB code value로 확장한다.

현재 구현은 opaque BC1 4-color mode를 가정하며, 중간 palette 색은 hardware와 같은 code-space 정수 연산으로 계산한다.

```text
c0 = expand_RGB565(color0)
c1 = expand_RGB565(color1)
c2 = (2 × c0 + c1) / 3
c3 = (c0 + 2 × c1) / 3
```

여기서 나눗셈은 정수 나눗셈이다. 중요한 점은 endpoint만 linear로 변환한 뒤 linear 공간에서 `1/3`, `2/3` 보간하지 않는다는 것이다. 실제 BC1 decoder가 만드는 `c0`, `c1`, `c2`, `c3`을 code space에서 먼저 만든다.

완성된 palette 색 네 개의 각 채널에 `Srgb8ToLinear()`를 적용한다.

```text
uk = sRGB_to_linear(ck),  k ∈ {0, 1, 2, 3}
```

이 결과가 이후 평균, covariance, endpoint 최적화에 사용되는 linear RGB palette이다.

## 7. 자식 texel 16개의 linear 평균 계산

`ComputeParentQuadrantMeans()`는 selector histogram과 linear palette를 결합한다. 한 quadrant의 평균은 다음과 같다.

```text
q = (n0 × u0 + n1 × u1 + n2 × u2 + n3 × u3) / 4
```

이 계산은 부모 블록의 해당 2×2 texel을 linear RGB로 복호화한 뒤 box filter로 평균한 것과 같다.

각 부모 블록에서 `q0`, `q1`, `q2`, `q3` 네 개의 평균이 나오며, 부모 네 개에서 얻은 총 16개 평균은 자식 블록의 다음 4×4 위치에 대응한다.

```text
p00.q0 p00.q1 p10.q0 p10.q1
p00.q2 p00.q3 p10.q2 p10.q3
p01.q0 p01.q1 p11.q0 p11.q1
p01.q2 p01.q3 p11.q2 p11.q3
```

따라서 mip downsampling 자체는 sRGB code value의 평균이 아니라 **linear RGB 평균**으로 수행된다.

## 8. Block-mean image $M$ 생성

`ComputeBlockMeans()`는 한 부모 블록의 quadrant 평균 네 개를 다시 평균한다.

```text
block_mean = (q0 + q1 + q2 + q3) / 4
```

quadrant 평균이 각각 2×2 texel의 평균이므로, 이 값은 부모 블록 16개 texel 전체의 linear RGB 평균과 정확히 같다. 이 값은 ANOVA 단계에서 어차피 계산되는 중간값이므로 추가 연산 비용이 거의 없다.

`ComputeChildBlockMoments()`가 이 네 부모의 평균을 `SourceBlockMeans`로 밖에 내보내고, `ProcessRowBC1()`이 `StoreBlockMean()`으로 lane을 풀어 block-mean image $M$의 해당 source block 좌표에 기록한다. clamp 때문에 같은 좌표가 중복 지정되는 경우에는 한 번만 기록한다.

$M$의 크기는 mip 0의 BC1 block grid와 같다.

$$
M_{width}=\left\lceil\frac{W}{4}\right\rceil,\qquad
M_{height}=\left\lceil\frac{H}{4}\right\rceil
$$

$M$의 texel 하나가 원본 4×4 pixel 영역에 대응하므로, $M$의 격자는 mip 2의 texel 격자와 정확히 일치한다.

## 9. 경로 B: linear mean image에서 자식 texel 구성

`ProcessLinearRowBC1()`은 압축 블록을 전혀 읽지 않는다. 자식 블록 `(block_x, dst_row_y)`의 texel `(local_x, local_y)`에 대해 mean image 좌표를 직접 계산한다.

```text
target_x = block_x * 4 + local_x
target_y = dst_row_y * 4 + local_y
```

`FetchLinearMeanTexel()`이 이 좌표를 image 범위로 clamp해서 읽는다. 읽은 값은 다음 규칙으로 `QuadrantMeans` 네 개에 배치된다.

```text
region = ((local_y >> 1) << 1) + (local_x >> 1)   → p00, p10, p01, p11 중 하나
sample = ((local_y & 1) << 1) + (local_x & 1)     → q0, q1, q2, q3 중 하나
```

즉 자식 블록의 4×4 texel을 2×2 그룹 네 개로 나누고, 각 그룹을 경로 A의 부모 슬롯과 같은 자리에 넣는다. 덕분에 이후 encoder core는 두 경로를 구분할 필요가 없다.

이 경로에서 `QuadrantMeans`의 값은 평균이 아니라 texel 값 자체이므로, 뒤따르는 ANOVA 분해는 16개 texel을 2×2 그룹 네 개로 나누어 계산하는 것과 같다. 결과 covariance는 16개 texel을 직접 사용해 계산한 값과 동일하다.

lane 배치는 경로 A와 같지만, 유효하지 않은 lane은 채우지 않고 건너뛴다. `QuadrantMeans`는 `float4`의 기본 생성자가 0으로 채우므로 미사용 lane은 정의된 0 상태로 남고, 결과 저장 루프가 유효 lane만 기록하므로 출력에는 영향이 없다.

## 10. ANOVA 방식의 평균과 covariance 계산

`ComputeChildBlockMoments()`는 16개 자식 texel의 전체 평균과 covariance를 구한다. 현재 구현은 이를 그룹별 within 성분과 그룹 사이의 between 성분으로 나누어 계산한다.

### 10.1 그룹별 통계

`ComputeParentStatistics()`가 한 그룹에서 나온 값 네 개에 대해 다음을 계산한다.

```text
group_mean = (q0 + q1 + q2 + q3) / 4
within_covariance = 평균((qi - group_mean)(qi - group_mean)^T)
```

### 10.2 전체 평균

그룹 네 개의 평균으로 자식 블록 전체 평균을 계산한다.

```text
mean = (mean00 + mean10 + mean01 + mean11) / 4
```

### 10.3 Between covariance

`AccumulateBetweenParentCovariance()`가 각 그룹 평균과 전체 평균의 차이를 누적한다.

```text
between = 평균((group_mean - mean)(group_mean - mean)^T)
```

### 10.4 전체 covariance

최종 covariance는 다음과 같다.

```text
covariance = between + 평균(within_covariance)
```

이 ANOVA 분해는 16개 자식 texel을 직접 사용해 계산한 전체 covariance와 수학적으로 같은 값을 다른 구조로 계산하는 것이다.

## 11. PCA 기반 초기 endpoint 계산

`ComputeInitialEndpointsPCA()`는 covariance의 주성분 방향을 구한다.

1. 초기 방향을 `(1/√3, 1/√3, 1/√3)`으로 설정한다.
2. covariance에 대한 power iteration을 **한 번** 수행한다. (루프 상한이 `i < 1`이다. 함수 안의 주석은 두 번이라고 되어 있으나 실제 반복 횟수는 한 번이다.)
3. 길이가 너무 작은 경우를 방지하기 위해 `1e-20` epsilon을 더한다.
4. 16개 자식 texel을 주성분 축에 투영한다.
5. 최소 투영값과 최대 투영값으로 초기 linear endpoint를 만든다.

```text
p0 = mean + t_min × axis
p1 = mean + t_max × axis
```

이 단계는 최종 BC1 endpoint를 확정하는 단계가 아니라, least-squares 최적화를 시작하기 위한 초기값을 정하는 단계이다.

## 12. Least-squares endpoint 보정

`OptimizeEndpointsLeastSquares()`는 현재 endpoint 선분에 각 자식 texel을 투영하고, 투영 위치를 BC1의 네 interpolation weight 중 가장 가까운 값으로 양자화한다.

```text
w ∈ {0, 1/3, 2/3, 1}
```

선분을 다음과 같이 표현한다.

```text
predicted_color = a + w × d
```

16개 자식 texel에 대해 `w`, `w²`, texel 색, `w × texel 색`을 누적한 뒤 2×2 normal equation을 풀어 `a`와 `d`를 갱신한다.

```text
det = 16 × S2 - S1²
p0  = a
p1  = a + d
```

행렬식이 `1e-6`보다 작아 안정적으로 풀 수 없으면 두 endpoint를 모두 전체 평균으로 설정한다.

## 13. Chord-curve gap 보정

`CorrectChordCurveGap()`는 linear 공간에서 구한 직선 endpoint와 sRGB code-space에서 보간된 실제 BC1 palette 사이의 차이를 줄이기 위한 보정을 수행한다.

1. linear endpoint `p0`, `p1`을 sRGB로 변환한다.
2. 두 sRGB endpoint의 중점을 다시 linear로 변환한다.
3. linear endpoint 중점과의 차이 `sigma`를 계산한다.
4. 두 linear endpoint에 동일하게 `4/9 × sigma`를 더한다.
5. 보정된 endpoint를 sRGB로 변환한다.

```text
sigma = (p0 + p1) / 2
        - sRGB_to_linear((linear_to_sRGB(p0) + linear_to_sRGB(p1)) / 2)

q0 = linear_to_sRGB(p0 + (4/9) × sigma)
q1 = linear_to_sRGB(p1 + (4/9) × sigma)
```

현재 `4/9` 계수는 BC1 4-color interpolation 위치가 균등하게 사용된다는 가정의 고정값이다. 실제 selector 분포에 따라 계수를 다시 계산하지는 않는다.

## 14. RGB565 endpoint 양자화와 strict opaque 순서 강제

`PackAndReallocateSelectors()`는 보정된 sRGB endpoint의 각 채널을 `[0, 1]`로 clamp한 뒤 RGB565로 반올림하여 pack한다.

```text
R: 5 bit
G: 6 bit
B: 5 bit
```

그다음 opaque 4-color mode를 보장하기 위해 `color0 > color1`을 강제한다.

```text
if color0 == color1:
    if (color1 & 0x1F) > 0:  color1 -= 1     # blue LSB 하나만 낮춤
    else:                    color0 += 1     # blue LSB 하나만 높임
elif color0 < color1:
    swap(color0, color1)
```

두 endpoint가 같은 경우에도 blue 채널 LSB 하나만 조정하므로 색 이동이 최소이며, 결과는 항상 strict한 `color0 > color1`이 된다. CPU 경로는 lane별 분기로, Slang 경로는 `select()` 기반 branchless 연산으로 같은 규칙을 구현한다.

## 15. 양자화된 palette 재생성과 selector 재할당

endpoint 양자화가 끝나면 양자화 전의 endpoint나 이상적인 linear interpolation 결과를 selector 선택에 사용하지 않는다.

1. 최종 RGB565 endpoint로 `BuildOpaqueLinearPaletteBC1()`를 다시 호출한다.
2. code-space 정수 보간으로 실제 hardware palette를 생성한다.
3. palette 네 색을 각각 linear RGB로 변환한다.
4. 16개 자식 texel 각각에 대해 네 palette 색과의 squared Euclidean distance를 계산한다.
5. 거리가 가장 작은 hardware selector index를 선택한다.

```text
selector = argmin_k ||child_texel - linear_palette[k]||²
```

선택된 2-bit selector를 자식 블록의 texel 순서에 맞춰 32-bit selector field에 pack한다. 최종 출력 블록은 다음 8 byte 구조를 가진다.

```text
16-bit color0
16-bit color1
32-bit selectors
```

## 16. 네 lane 결과 출력

`EncodeLinearBlocksBC1x4()`가 공통 encoder core이고, `EncodeMipBlocksBC1x4()`는 그 앞에 경로 A의 histogram 및 palette 복원 단계를 붙인 wrapper이다. 두 함수 모두 `SymbolicDataBC1x4`를 반환한다. `ProcessRowBC1()`과 `ProcessLinearRowBC1()`은 유효한 각 lane의 endpoint와 selector를 일반 BC1 block으로 꺼내 destination mip level에 저장한다.

## 17. 전체 mip chain에서의 사용 방식

`CpuBackend::GenerateChain()`은 mip 1만 압축 블록에서 만들고, mip 2 이상은 linear mean image에서 만든다.

```text
mip 0 BC1 blocks ─┬─ quadrant means ──> mip 1 BC1 blocks
                  └─ block means M = T_2 ──> mip 2 BC1 blocks
                                    └─ 1/2 ──> T_3 ──> mip 3 BC1 blocks
                                               └─ 1/2 ──> T_4 ──> mip 4 ...
```

mip 2 이상은 이전에 인코딩된 BC1 mip을 입력으로 사용하지 않으므로 재인코딩 오차가 level을 따라 누적되지 않는다.

### 17.1 Mean pyramid

level $L \ge 2$의 target texel은 원래 $M$을 $s_L = 2^{L-2}$ 크기의 box로 평균한 값으로 정의된다.

$$
T_L(x,y)=\frac{1}{s_L^2}
\sum_{j=0}^{s_L-1}\sum_{i=0}^{s_L-1}
M(s_Lx+i,\,s_Ly+j)
$$

linear domain의 box filter는 결합적이므로 이 값은 직전 level의 mean image를 2×2로 평균한 것과 같다.

$$
T_L(x,y)=\frac{1}{4}\sum_{j=0}^{1}\sum_{i=0}^{1} T_{L-1}(2x+i,\,2y+j),
\qquad T_2 \equiv M
$$

현재 구현은 이 재귀식을 사용한다. `DownsampleLinearMeanRow()`가 mean image를 반으로 줄이고, `GenerateChain()`이 ping-pong buffer 두 개를 번갈아 쓴다. scratch buffer는 $M$의 1/4 크기면 이후 모든 level을 커버한다.

이 방식으로 level당 비용이 상수에서 등비 감소로 바뀐다. 축소는 linear float domain에서만 일어나고 BC1 재인코딩을 거치지 않으므로, 오차 누적이 없다는 성질은 그대로 유지된다.

### 17.2 작업 분배

각 level에서는 destination block row를 작업 단위로 thread pool에 분배하고, 해당 level의 모든 row 작업이 끝난 뒤 다음 level로 넘어간다. mean image 축소도 같은 방식으로 row 단위 분배 후 동기화한다.

## 18. 현재 방법에서 정확한 부분과 근사인 부분

### 현재 연산 모델 안에서 직접 계산되는 부분

- 부모 selector의 2×2 quadrant histogram
- RGB565 확장과 BC1 code-space 정수 palette 생성
- palette별 sRGB-to-linear 변환
- histogram을 이용한 linear 2×2 box-filter 평균
- quadrant 평균 네 개에서 얻는 block 평균
- ANOVA로 계산한 평균과 covariance
- 2의 거듭제곱 해상도에서 mean pyramid와 단일 $s_L$ box mean의 동등성
- 최종 양자화 endpoint에 대한 hardware palette 재생성
- 고정된 endpoint에 대한 nearest-selector 선택

### 근사 또는 heuristic인 부분

- 한 번의 power iteration으로 구하는 PCA 방향
- discrete weight를 먼저 배정한 뒤 한 번 푸는 least-squares endpoint
- 실제 selector 분포와 무관한 고정 `4/9` chord-curve 보정
- continuous endpoint를 RGB565 격자로 반올림하는 양자화
- endpoint가 같을 때 blue LSB 하나를 움직이는 strict opaque 보정
- $M$이 원본 texel이 아니라 mip 0 BC1 양자화 결과에서 계산된 평균이라는 점
- 경계에서 유효 texel 수에 따른 가중 평균이 아니라 clamp-to-edge로 처리하는 것

## 19. 현재 구현의 범위와 남은 제한

- opaque BC1 4-color block만 가정한다.
- BC1 3-color/transparent mode는 처리하지 않는다.
- BC1 UNORM과 BC1 UNORM SRGB의 format metadata를 끝까지 구분하지 않는다.
- mean pyramid와 단일 $s_L$ box mean의 동등성은 2의 거듭제곱 해상도에서만 성립한다. 중간 level에 홀수 폭이 생기면 edge clamp 가중치가 달라지므로 두 결과가 미세하게 갈릴 수 있다.
- 홀수 크기나 매우 작은 mip의 clamp는 실제 유효 texel 수에 따른 가중 평균이 아니라 경계 texel 반복 방식이다.
- `ProcessLinearRowBC1()`은 `SourceBlockMeans` 출력 인자를 채우지만 그 값을 사용하지 않는다. 경로 B에서는 필요 없는 출력이다.
- 경로 A의 lane 채우기는 유효하지 않은 lane도 마지막 유효 작업을 복제해 계산한다. 경로 B와 달리 아직 skip하지 않는다.
- `Lane()`과 `SetLane()`이 runtime index `switch`로 구현되어 있어 lane 추출/삽입이 벡터화되지 않는다.
- CPU의 4-lane 자료형은 SIMD에 적합한 구조지만, 현재 구현 자체가 SSE/AVX intrinsic을 사용하는 것은 아니다.
- BC3, BC4, BC5, BC7 encoder는 이 경로에 구현되어 있지 않다.

## 20. 코드의 주요 함수 대응표

### 경로별 진입점

| 처리 단계 | 함수 |
|---|---|
| mip chain 전체 스케줄링, mean pyramid 관리 | `CpuBackend::GenerateChain()` |
| 경로 A: 부모 블록 수집, 4-lane 구성, 결과 및 $M$ 저장 | `ProcessRowBC1()` |
| 경로 B: mean image에서 자식 texel 구성, 결과 저장 | `ProcessLinearRowBC1()` |
| mean image 2×2 축소 (한 row) | `DownsampleLinearMeanRow()` |
| mean image clamp-to-edge fetch | `FetchLinearMeanTexel()` |

### 경로 A 전용 단계

| 처리 단계 | 함수 |
|---|---|
| histogram과 palette 복원을 포함한 4-lane encode | `EncodeMipBlocksBC1x4()` |
| selector quadrant histogram | `Extract2x2SelectorHistograms()` |
| 2×2 영역 SWAR 카운터 | `Count2x2Regions()` |
| BC1 hardware palette와 linear 변환 | `BuildOpaqueLinearPaletteBC1()` |
| 부모별 quadrant 평균 | `ComputeParentQuadrantMeans()` |
| quadrant 평균 네 개에서 block 평균 | `ComputeBlockMeans()` |
| lane을 풀어 $M$에 기록 | `StoreBlockMean()` |

### 공통 encoder core

| 처리 단계 | 함수 |
|---|---|
| 자식 texel 16개에서 BC1 block으로 | `EncodeLinearBlocksBC1x4()` |
| 그룹별 평균과 within covariance | `ComputeParentStatistics()` |
| between covariance 누적 | `AccumulateBetweenParentCovariance()` |
| 전체 평균, ANOVA covariance, source block 평균 출력 | `ComputeChildBlockMoments()` |
| PCA 초기 endpoint | `ComputeInitialEndpointsPCA()` |
| least-squares endpoint 보정 | `OptimizeEndpointsLeastSquares()` |
| chord-curve gap 보정 | `CorrectChordCurveGap()` |
| RGB565 양자화, strict opaque 강제, selector 재할당, pack | `PackAndReallocateSelectors()` |
| texel 하나의 nearest selector | `AssignNearestSelector()`, `FindBestSelector()` |

### CPU 보조

| 처리 단계 | 함수 |
|---|---|
| scalar BC1 block 네 개를 4-lane으로 pack | `PackBlocks()` |
| lane 값 읽기/쓰기 | `Lane()`, `SetLane()` |
| 자식 texel을 `QuadrantMeans` 슬롯에 배치 | `SetQuadrantSample()` |
