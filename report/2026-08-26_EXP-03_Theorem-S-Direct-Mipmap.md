# EXP-03: Theorem S 기반 Direct Mipmap 생성 실험

## 실험 목적과 가설

기존 구현은 mip 1을 BC1으로 인코딩한 뒤, 해당 BC1 결과를 다시 디코딩하여 mip 2 이상을 재귀적으로 생성했다. 이 방식에서는 각 레벨의 BC1 양자화 오차가 다음 레벨 입력에 누적된다.

이번 실험의 목적은 mip 0 BC1 블록에서 계산한 linear block-mean image $M$을 이용하여 mip 2 이상의 모든 레벨을 직접 생성하고, 다음 가설을 확인하는 것이다.

- mip 1은 기존 quadrant-mean 경로와 동일한 결과를 유지한다.
- mip 2 이상은 이전 BC1 mip을 다시 읽지 않으므로 recursive quantization error가 제거된다.
- 오차 누적이 제거되면서 낮은 해상도의 mip에서 PSNR과 FLIP이 개선된다.
- 현재 CPU 구현은 각 레벨의 box mean을 $M$에서 직접 다시 계산하므로 실행시간이 증가할 수 있다.

## 적용한 기술과 구현 내용

입력은 mip 0의 BC1 block 배열이다. 각 BC1 블록을 linear RGB domain으로 복원한 뒤 다음 통계를 한 번의 처리 경로에서 계산한다.

- 네 개의 2×2 quadrant mean: mip 1 생성에 사용
- 전체 4×4 block mean: block-mean image $M$ 생성에 사용

$M$의 크기는 mip 0의 BC1 block grid와 같다.

$$
M_{width}=\left\lceil\frac{W}{4}\right\rceil,\qquad
M_{height}=\left\lceil\frac{H}{4}\right\rceil
$$

mip level $L\ge2$에서 하나의 target texel은 $M$의 다음 크기 영역을 평균하여 계산한다.

$$
s_L=2^{L-2}
$$

$$
T_L(x,y)=\frac{1}{s_L^2}
\sum_{j=0}^{s_L-1}\sum_{i=0}^{s_L-1}
M(s_Lx+i,s_Ly+j)
$$

따라서 level별 경로는 다음과 같다.

```text
mip 0 BC1
 ├─ quadrant means ──> mip 1
 └─ block means M
      ├─ 1×1 box ──> mip 2
      ├─ 2×2 box ──> mip 3
      ├─ 4×4 box ──> mip 4
      └─ 8×8 box ──> mip 5 ...
```

mip 2 이상에서는 이전에 인코딩된 BC1 mip을 입력으로 사용하지 않는다. 각 레벨의 linear target을 계산한 뒤 기존의 ANOVA covariance, PCA endpoint initialization, Least Squares endpoint optimization, chord-curve correction 및 BC1 selector allocation을 적용한다.

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 날짜 | 2026-08-26 |
| 운영체제 | Windows 11 |
| 빌드 | MSVC Release |
| 입력 | `input/bc1/test.dds` |
| 입력 해상도 | 2048×2048 |
| 평가 레벨 | mip 0~mip 11 |
| Reference | `target/Ceramic_0557_brick_uneven_stones_color` |
| 품질 평가 환경 | Conda `ntc` |
| 성능 측정 범위 | `CpuBackend::GenerateChain()` |
| 성능 반복 횟수 | warm-up 이후 10회 |
| 대표 실행시간 | 10회 중앙값 |

PSNR과 FLIP은 여러 mip을 하나의 평균으로 합치지 않고 각 mip level별로 기록했다. 성능 측정에는 PNG 및 DDS 저장 시간이 포함되지 않는다.

비교 대상 `Recursive`는 직전 구현의 `output/test/metrics_ours/metrics.csv` 및 10회 중앙값 15.2005 ms이다.

## 정량적·정성적 결과

### Mip level별 PSNR

PSNR은 높을수록 좋다. `Δ`는 Theorem S에서 Recursive 결과를 뺀 값이다.

| Mip | 해상도 | Recursive (dB) | Theorem S (dB) | Δ PSNR (dB) |
|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 43.514276 | 43.514276 | +0.000000 |
| 1 | 1024×1024 | 40.335939 | 40.335939 | +0.000000 |
| 2 | 512×512 | 38.854912 | 39.739241 | +0.884329 |
| 3 | 256×256 | 38.291303 | 39.474477 | +1.183174 |
| 4 | 128×128 | 37.817952 | 38.999398 | +1.181446 |
| 5 | 64×64 | 37.267976 | 38.463392 | +1.195416 |
| 6 | 32×32 | 36.560433 | 37.781095 | +1.220662 |
| 7 | 16×16 | 34.843664 | 36.769628 | +1.925964 |
| 8 | 8×8 | 34.757251 | 36.998336 | +2.241085 |
| 9 | 4×4 | 37.005312 | 38.498830 | +1.493518 |
| 10 | 2×2 | 39.237787 | 40.597527 | +1.359740 |
| 11 | 1×1 | 42.110204 | 43.359591 | +1.249387 |

mip 0은 입력 BC1이고 mip 1은 기존과 같은 quadrant-mean 경로이므로 결과가 동일하다. mip 2부터 모든 level에서 PSNR이 개선됐다. 가장 큰 향상은 mip 8의 +2.241085 dB이다.

### Mip level별 FLIP

FLIP은 낮을수록 좋다. 음수 `Δ`는 Theorem S의 개선을 의미한다.

| Mip | 해상도 | Recursive | Theorem S | Δ FLIP |
|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 0.026732 | 0.026732 | +0.000000 |
| 1 | 1024×1024 | 0.038134 | 0.038134 | +0.000000 |
| 2 | 512×512 | 0.041404 | 0.037437 | -0.003967 |
| 3 | 256×256 | 0.042743 | 0.037013 | -0.005730 |
| 4 | 128×128 | 0.045611 | 0.039359 | -0.006252 |
| 5 | 64×64 | 0.047684 | 0.039366 | -0.008318 |
| 6 | 32×32 | 0.049926 | 0.042191 | -0.007735 |
| 7 | 16×16 | 0.053240 | 0.045918 | -0.007322 |
| 8 | 8×8 | 0.071882 | 0.055241 | -0.016641 |
| 9 | 4×4 | 0.068253 | 0.067284 | -0.000969 |
| 10 | 2×2 | 0.059890 | 0.055219 | -0.004671 |
| 11 | 1×1 | 0.052784 | 0.061181 | +0.008397 |

mip 2~10에서는 FLIP이 모두 개선됐다. mip 8에서 개선 폭이 -0.016641로 가장 크다. 반면 mip 11에서는 PSNR이 +1.249387 dB 개선됐지만 FLIP은 +0.008397 증가했다. 1×1처럼 표본 수가 극단적으로 작은 level에서는 PSNR과 perceptual metric의 판단 방향이 일치하지 않을 수 있다.

### Mipmap generation time

10회 측정값은 다음과 같다.

```text
27.925, 31.625, 28.076, 28.340, 29.359,
28.390, 28.564, 31.632, 28.489, 27.747 ms
```

| 통계 | 시간 |
|---|---:|
| 최소 | 27.747 ms |
| 최대 | 31.632 ms |
| 평균 | 29.0147 ms |
| 표본 표준편차 | 1.4446 ms |
| 중앙값 | **28.4395 ms** |

생성되는 mip 1~11의 전체 texel 수는 1,398,101개이며, 중앙값 기준 처리율은 약 49.161 MPixel/s이다.

| 구현 | 10회 중앙값 | 상대 변화 |
|---|---:|---:|
| Recursive | 15.2005 ms | 기준 |
| Theorem S direct | 28.4395 ms | +87.10% |

현재 Theorem S direct 구현은 Recursive 구현보다 약 1.87배의 시간이 필요하다. 이는 각 mip level이 $M$에서 독립적으로 box mean을 다시 계산하여 동일한 $M$ texel을 level마다 반복해서 읽고 누산하기 때문이다. 따라서 이번 결과는 품질 가설은 지지하지만, 현재 CPU 스케줄링이 성능상 최적이라는 가설은 지지하지 않는다.

정성적으로는 mip 2 이상에서 recursive compression error가 제거되며, 특히 낮은 해상도의 mip에서 PSNR 및 대부분의 FLIP 결과가 뚜렷하게 개선되는 형태를 보였다.

## 부족했던 점과 실패 원인

- 하나의 texture만 평가했으므로 색 분포와 공간 주파수가 다른 데이터에 일반화할 수 없다.
- 2048×2048의 2의 거듭제곱 입력만 검증했으며, non-power-of-two 경계 처리의 수학적 동등성은 별도로 검증하지 않았다.
- direct 구현은 level마다 $M$ 전체에 해당하는 sample을 다시 누산한다. 이 방식은 level 간 독립성을 얻지만 CPU에서는 중복 메모리 접근과 연산이 증가한다.
- 현재 `ComputeLinearBoxMean()`의 `box_size`는 runtime 값이므로 작은 고정 크기 box에 대한 compiler unrolling 및 SIMD 효율이 제한될 수 있다.
- 성능 비교는 동일 프로그램의 `GenerateChain()` 범위이므로 전후 비교에는 유효하지만, GPU의 독립 dispatch 장점까지 반영하지는 않는다.
- mip 11의 FLIP은 PSNR과 반대 방향으로 변했다. 1×1 결과를 독립적인 perceptual quality 근거로 과도하게 해석하면 안 된다.

## 실험을 통해 발견한 점

- mip 0에서 계산한 block-mean image $M$만으로 mip 2 이상의 target을 생성할 수 있음을 실제 전체 mip chain에서 확인했다.
- recursive BC1 재압축을 제거하면 mip 2부터 PSNR 향상이 나타나며, 이번 texture에서는 +0.884329~+2.241085 dB의 개선을 기록했다.
- FLIP도 mip 2~10에서 모두 개선되어 PSNR 향상이 단순 수치상의 변화만은 아님을 확인했다.
- mip level별 결과를 유지해야 낮은 해상도에서 발생하는 큰 개선을 확인할 수 있다. texel 수로 가중한 하나의 집계값은 mip 1의 비중 때문에 이 효과를 축소한다.
- Theorem S의 수학적 독립성과 CPU에서의 최적 실행 방식은 별개의 문제다. 현재 direct scheduling은 연구 가설 검증에는 적합하지만 CPU 성능에는 불리하다.

## 후속 실험과 개선 방향

1. 동일한 $M$을 입력으로 사용하는 linear carry 구현을 추가하고 direct 방식과 decoded target이 `allclose`인지 검증한다.
2. CPU에서는 ping-pong linear buffer로 2×2 box filtering하여 전체 누산량을 줄이고, GPU에서는 level별 direct dispatch를 유지하는 backend별 스케줄링을 비교한다.
3. `box_size` 1, 2, 4에 대한 specialized kernel을 작성하여 loop unrolling과 SIMD 적용 효과를 측정한다.
4. 8-bit linear, 16-bit linear 및 float32의 $M$ 저장 형식을 비교하여 품질과 memory bandwidth의 trade-off를 측정한다.
5. 최소 4개 이상의 texture와 non-power-of-two 해상도에서 mip별 PSNR, FLIP 및 10회 중앙값을 반복 측정한다.
6. mip 11의 PSNR/FLIP 불일치가 endpoint ordering, 1×1 padding 또는 FLIP의 작은 영상 처리 특성에서 발생하는지 분리 검증한다.
