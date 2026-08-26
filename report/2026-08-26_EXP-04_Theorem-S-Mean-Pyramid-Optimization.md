# EXP-04: Theorem S Mean Pyramid 최적화 및 BC1 Encoder 비교

## 실험 목적과 가설

EXP-03에서 Theorem S 기반 direct mipmap 생성은 mip 2 이상에서 PSNR과 FLIP을 모두 개선했지만, 실행시간이 Recursive 대비 +87.10% 증가했다. 해당 보고서는 이를 "각 level이 $M$에서 독립적으로 box mean을 다시 계산하기 때문"으로 진단하고, 후속 과제로 ping-pong linear buffer 방식을 제시했다.

이번 실험의 목적은 그 진단이 맞는지 확인하고 최적화를 적용하는 것이다. 가설은 다음과 같다.

- 실행시간 증가의 대부분은 mip level 전체에 고르게 분산된 것이 아니라, box size가 커지는 소수의 낮은 해상도 level에 집중되어 있다.
- linear domain에서 box filter는 결합적이므로, $M$을 level마다 $s_L \times s_L$로 다시 누산하는 대신 직전 level의 mean image를 2×2로 반씩 줄여도 $T_L$의 값은 정확히 동일하다.
- 따라서 최적화는 품질을 전혀 바꾸지 않으며, 출력 BC1 stream은 byte 단위로 동일해야 한다.
- 최적화 후 Theorem S의 실행시간은 Recursive 수준으로 회복되어, "품질은 얻지만 속도는 손해"라는 EXP-03의 trade-off 결론이 해소된다.

## 원인 분석

`CpuBackend::GenerateChain()`에 level별 timer를 삽입하여 EXP-03 구현의 시간 분포를 측정했다. 대표 실행 1회(총 29.395 ms)의 결과는 다음과 같다.

| Mip | dst block grid | 시간 (ms) |
|---:|---:|---:|
| 1 | 256×256 | 9.7281 |
| 2 | 128×128 | 2.3795 |
| 3 | 64×64 | 0.6233 |
| 4 | 32×32 | 0.2453 |
| 5 | 16×16 | 0.1318 |
| 6 | 8×8 | 0.1225 |
| 7 | 4×4 | 0.1232 |
| 8 | 2×2 | 0.1987 |
| 9 | 1×1 | 0.6479 |
| 10 | 1×1 | 2.4589 |
| 11 | 1×1 | **9.8467** |

블록이 1개뿐인 mip 11이 블록 65,536개인 mip 1과 같은 시간을 소비했다. mip 9·10·11의 합만으로 12.95 ms이며, 이는 EXP-03에서 관측된 증가분과 거의 일치한다.

원인은 세 가지가 곱해진 결과였다.

1. **level당 sample 수가 상수** — level $L$의 dst block 수는 $N/4^{L}$로 줄지만 texel당 sample 수는 $s_L^2 = 4^{L-2}$로 늘어난다. 곱하면 level과 무관하게 항상 $M$ 전체(262,144 sample)를 훑는다.
2. **clamp에 의한 16배 중복** — `ComputeLinearBoxMean()`은 `target_x`를 $\lceil M_w / s_L \rceil - 1$로 clamp한다. mip 11에서는 이 값이 0이므로 block 안 16개 texel이 전부 동일한 전체 이미지 평균을 각각 다시 계산한다.
3. **SIMD lane의 4배 중복** — `ProcessLinearRowBC1()`의 lane 루프는 `dst_x + std::min(lane, valid_lanes - 1)`을 사용한다. `valid_lanes == 1`이면 4개 lane이 모두 같은 block을 계산한다.

따라서 mip 11에서 실제로 필요한 262,144 sample 대신 $4 \times 16 \times 262{,}144 = 16.8$ M sample을 누산했다. 게다가 `dst_block_height == 1`이라 task가 1개뿐이어서 단일 스레드로 직렬 실행되었다.

## 적용한 기술과 구현 내용

### 1. Mean pyramid (ping-pong 2×2 halving)

linear domain의 box mean은 결합적이므로 다음이 성립한다.

$$
T_L(x,y)=\frac{1}{4}\sum_{j=0}^{1}\sum_{i=0}^{1} T_{L-1}(2x+i,\,2y+j),
\qquad T_2 \equiv M
$$

즉 level $L$의 target은 $M$을 $s_L \times s_L$로 누산한 값과, $T_{L-1}$을 2×2로 평균한 값이 동일하다. 이 재귀는 linear float domain에서만 수행되며 BC1 재인코딩을 거치지 않으므로, Theorem S의 핵심인 recursive quantization error 제거 성질은 그대로 유지된다.

`DownsampleLinearMeanRow()`를 추가하고, `GenerateChain()`이 level $\ge 3$마다 mean image를 한 번씩 반으로 줄이도록 ping-pong buffer를 도입했다. scratch buffer는 $M$의 1/4 크기면 이후 모든 level을 커버한다. 축소 작업도 행 단위로 thread pool에 분배했다.

```text
mip 0 BC1
 ├─ quadrant means ──> mip 1
 └─ block means M = T_2 ──> mip 2
                  └─ 1/2 ──> T_3 ──> mip 3
                             └─ 1/2 ──> T_4 ──> mip 4 ...
```

level당 비용은 상수에서 $O(N/4^L)$의 등비 감소로 바뀌고, 전체 축소 비용은 $M$ 크기의 1/3로 수렴한다.

### 2. 유휴 SIMD lane 스킵

`ProcessLinearRowBC1()`의 lane 루프 상한을 `kLaneCount`에서 `valid_lanes`로 바꾸었다. `QuadrantMeans`는 `float4`의 기본 생성자가 0으로 채우므로 미사용 lane은 정의된 0 상태로 남고, 결과 저장 루프가 `lane < valid_lanes`만 기록하므로 출력에 영향이 없다.

### 3. Box 누산 제거

1번으로 $s_L$이 항상 1이 되면서 `ComputeLinearBoxMean()`의 이중 루프, runtime 나눗셈, clamp 재계산이 모두 불필요해졌다. clamp-to-edge fetch 한 줄(`FetchLinearMeanTexel()`)로 대체하고 `box_size` 파라미터를 제거했다.

### 등가성 검증

최적화 전후의 `output/test/test_mipmap.dds`가 byte 단위로 동일함을 MD5로 확인했다(`ac69f2140944140796144a26bf721420`). `output/test/metrics_theorem_s/metrics.csv`를 재생성한 결과도 EXP-03과 모든 level에서 소수점 6자리까지 일치한다. 즉 이번 변경은 순수한 성능 최적화이며 품질 데이터는 EXP-03과 동일한 값을 그대로 사용할 수 있다.

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 날짜 | 2026-08-26 |
| 운영체제 | Windows 11 |
| CPU logical core | 12 |
| 빌드 | MSVC Release (`/O2 /Ob2`) |
| 입력 | `input/bc1/test.dds` |
| 입력 해상도 | 2048×2048, mip0~mip11 |
| Reference | `target/Ceramic_0557_brick_uneven_stones_color` |
| 품질 평가 환경 | Conda `ntc` |
| 성능 측정 범위 | `CpuBackend::GenerateChain()` (mip1~mip11) |
| 성능 반복 횟수 | warm-up 이후 10회 |
| 대표 실행시간 | 10회 중앙값 |
| 품질 원본 (Theorem S) | `output/test/metrics_theorem_s/metrics.csv` |
| 품질 원본 (Recursive) | `output/test/metrics_ours/metrics.csv` |
| Baseline 원본 | `output/baseline/Ceramic_0557_brick_uneven_stones_color/20260826_145808` |

비교 대상은 EXP-02와 동일한 baseline을 재사용했다.

- `Recursive`: 이전 compression-domain 구현, 생성된 compressed mip을 다음 level 입력으로 사용
- `Theorem S (EXP-03)`: 최적화 이전 direct box 구현
- `Theorem S (optimized)`: 이번 mean pyramid 구현
- `bc7enc fast`: BC1 `-L0 -c`, 각 reference mip을 독립 인코딩
- `bc7enc quality`: BC1 `-L18 -c`, 각 reference mip을 독립 인코딩
- `etcpak mipmap`: `-M -m -c bc1 -h dds`, mip0에서 전체 mip chain 생성

우리 구현의 mip0는 생성 결과가 아니라 입력 DDS의 base level이므로, 생성 품질 분석은 mip1부터 시작한다.

## 정량적·정성적 결과

### Mipmap generation time

최적화 후 10회 측정값은 다음과 같다.

```text
14.911, 12.828, 15.507, 13.023, 15.895,
25.521, 15.033, 15.786, 13.780, 15.665 ms
```

| 통계 | Theorem S (EXP-03) | Theorem S (optimized) |
|---|---:|---:|
| 최소 | 27.747 ms | 12.828 ms |
| 최대 | 31.632 ms | 25.521 ms |
| 평균 | 29.0147 ms | 15.7949 ms |
| 표본 표준편차 | 1.4446 ms | 3.5996 ms |
| 중앙값 | **28.4395 ms** | **15.2700 ms** |

6번째 측정값 25.521 ms는 나머지 9개(12.828~15.895 ms) 범위에서 크게 벗어난 outlier이며 표준편차를 3.5996 ms까지 끌어올렸다. 이 값을 제외하면 평균은 14.7142 ms이다. 중앙값은 outlier에 둔감하므로 대표값으로 15.2700 ms를 사용한다.

| 구현 | 10회 중앙값 | EXP-03 대비 | Recursive 대비 |
|---|---:|---:|---:|
| Recursive | 15.2005 ms | -46.55% | 기준 |
| Theorem S (EXP-03) | 28.4395 ms | 기준 | +87.10% |
| Theorem S (optimized) | **15.2700 ms** | **-46.31%** | **+0.46%** |

최적화로 실행시간이 1.862배 단축되었고, Recursive와의 차이는 +0.46%로 줄었다. 이는 두 구현의 측정 산포(Recursive sd 1.2596 ms, optimized sd 3.5996 ms) 안에 완전히 들어가는 값이므로, 성능 차이가 남아있다고 주장할 수 없는 수준이다. mip1~mip11의 texel 1,398,101개 기준 처리율은 약 91.559 MPixel/s로, Recursive의 91.977 MPixel/s와 사실상 같다.

### Level별 시간 분포 변화

level timer를 삽입한 대표 실행 1회씩을 비교한 결과다. 계측 오버헤드 때문에 총합은 위 중앙값보다 약간 크다.

| Mip | dst block grid | EXP-03 (ms) | Optimized (ms) |
|---:|---:|---:|---:|
| 1 | 256×256 | 9.7281 | 11.2466 |
| 2 | 128×128 | 2.3795 | 1.7512 |
| 3 | 64×64 | 0.6233 | 0.8982 |
| 4 | 32×32 | 0.2453 | 0.3332 |
| 5 | 16×16 | 0.1318 | 0.1816 |
| 6 | 8×8 | 0.1225 | 0.0509 |
| 7 | 4×4 | 0.1232 | 0.0842 |
| 8 | 2×2 | 0.1987 | 0.0402 |
| 9 | 1×1 | 0.6479 | **0.0256** |
| 10 | 1×1 | 2.4589 | **0.0211** |
| 11 | 1×1 | 9.8467 | **0.0198** |
| mip 9~11 합 | | **12.9535** | **0.0665** |

꼬리 3개 level이 12.95 ms에서 0.067 ms로 줄었다. mip 3~5가 소폭 증가한 것은 해당 level 직전에 수행되는 mean image 축소 비용이 그 level에 계상되기 때문이며, 축소 비용 전체가 mip 3~5에 나타난 증가분 약 0.4 ms에 해당한다.

### Mip level별 PSNR

PSNR은 높을수록 좋다. Theorem S 열은 최적화 전후가 동일하다.

| Mip | 해상도 | Ours Recursive | Ours Theorem S | bc7enc fast | bc7enc quality | etcpak mipmap |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 43.514276 | 43.514276 | 42.356697 | 43.514447 | 38.697344 |
| 1 | 1024×1024 | 40.335939 | 40.335939 | 41.109214 | 41.917952 | 36.140151 |
| 2 | 512×512 | 38.854912 | 39.739241 | 40.133554 | 40.835686 | 34.061096 |
| 3 | 256×256 | 38.291303 | 39.474477 | 39.736235 | 40.396989 | 32.498188 |
| 4 | 128×128 | 37.817952 | 38.999398 | 39.325089 | 39.981382 | 31.173056 |
| 5 | 64×64 | 37.267976 | 38.463392 | 38.800755 | 39.403533 | 29.688860 |
| 6 | 32×32 | 36.560433 | 37.781095 | 38.127412 | 38.607900 | 27.783156 |
| 7 | 16×16 | 34.843664 | 36.769628 | 37.350316 | 37.669837 | 26.076159 |
| 8 | 8×8 | 34.757251 | 36.998336 | 37.441991 | 37.645495 | 24.100224 |
| 9 | 4×4 | 37.005312 | 38.498830 | 39.891716 | 39.891716 | 23.477285 |
| 10 | 2×2 | 39.237787 | 40.597527 | 40.597527 | 45.120504 | 10.165234 |
| 11 | 1×1 | 42.110204 | 43.359591 | inf | inf | 10.200820 |

### Mip level별 FLIP

FLIP은 낮을수록 좋다.

| Mip | 해상도 | Ours Recursive | Ours Theorem S | bc7enc fast | bc7enc quality | etcpak mipmap |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 0.026732 | 0.026732 | 0.034014 | 0.026731 | 0.055746 |
| 1 | 1024×1024 | 0.038134 | 0.038134 | 0.033811 | 0.028078 | 0.069855 |
| 2 | 512×512 | 0.041404 | 0.037437 | 0.034847 | 0.029326 | 0.083737 |
| 3 | 256×256 | 0.042743 | 0.037013 | 0.035675 | 0.029517 | 0.096833 |
| 4 | 128×128 | 0.045611 | 0.039359 | 0.037028 | 0.030119 | 0.108889 |
| 5 | 64×64 | 0.047684 | 0.039366 | 0.038475 | 0.031716 | 0.121924 |
| 6 | 32×32 | 0.049926 | 0.042191 | 0.040099 | 0.037758 | 0.136703 |
| 7 | 16×16 | 0.053240 | 0.045918 | 0.050135 | 0.042576 | 0.146197 |
| 8 | 8×8 | 0.071882 | 0.055241 | 0.045661 | 0.038815 | 0.149955 |
| 9 | 4×4 | 0.068253 | 0.067284 | 0.054902 | 0.054902 | 0.196747 |
| 10 | 2×2 | 0.059890 | 0.055219 | 0.055219 | 0.031297 | 0.719094 |
| 11 | 1×1 | 0.052784 | 0.061181 | 0.000000 | 0.000000 | 0.698326 |

### Theorem S와 baseline encoder의 level별 차이

`Δ`는 Theorem S에서 상대 encoder를 뺀 값이다. PSNR은 양수가, FLIP은 음수가 Theorem S의 우위를 뜻한다.

| Mip | 해상도 | Δ PSNR vs bc7enc fast | Δ FLIP vs bc7enc fast | Δ PSNR vs etcpak | Δ FLIP vs etcpak |
|---:|---:|---:|---:|---:|---:|
| 1 | 1024×1024 | -0.773275 | +0.004323 | +4.195788 | -0.031721 |
| 2 | 512×512 | -0.394313 | +0.002590 | +5.678145 | -0.046300 |
| 3 | 256×256 | -0.261758 | +0.001338 | +6.976289 | -0.059820 |
| 4 | 128×128 | -0.325691 | +0.002331 | +7.826342 | -0.069530 |
| 5 | 64×64 | -0.337363 | +0.000891 | +8.774532 | -0.082558 |
| 6 | 32×32 | -0.346317 | +0.002092 | +9.997939 | -0.094512 |
| 7 | 16×16 | -0.580688 | -0.004217 | +10.693469 | -0.100279 |
| 8 | 8×8 | -0.443655 | +0.009580 | +12.898112 | -0.094714 |
| 9 | 4×4 | -1.392886 | +0.012382 | +15.021545 | -0.129463 |
| 10 | 2×2 | +0.000000 | +0.000000 | +30.432293 | -0.663875 |

mip 10에서 Theorem S의 PSNR과 FLIP이 bc7enc fast와 소수점 6자리까지 완전히 일치한다. 2×2 이미지에서는 표현 가능한 결과의 수가 매우 적으므로, 두 encoder가 동일한 block에 수렴한 것으로 보인다.

### 집계 비교 (mip 1~10, level별 단순 평균)

mip 11은 bc7enc의 PSNR이 `inf`라 평균에 넣을 수 없으므로 제외했다. 해상도가 다른 level을 texel 수로 가중하면 mip 1이 결과를 지배하므로, level별 단순 평균을 사용한다.

| 구현 | mip1~10 평균 PSNR (dB) | mip1~10 평균 FLIP | mip1~6 평균 PSNR | mip7~10 평균 PSNR |
|---|---:|---:|---:|---:|
| Ours Recursive | 37.4973 | 0.051877 | 38.1881 | 36.4610 |
| **Ours Theorem S** | **38.7658** | **0.045716** | **39.1323** | **38.2161** |
| bc7enc fast | 39.2514 | 0.042585 | 39.5387 | 38.8204 |
| bc7enc quality | 40.1471 | 0.035410 | 40.1906 | 40.0819 |
| etcpak mipmap | 27.5163 | 0.182993 | 31.8908 | 20.9547 |

### 생성·인코딩 시간 비교

| 방법 | 반복 | 대표 시간 | 측정 범위 | 비고 |
|---|---:|---:|---|---|
| Ours Recursive | 10회 median | 15.2005 ms | mip1~11 `GenerateChain()` | 평균 15.0501 ms, sd 1.2596 ms |
| Ours Theorem S (EXP-03) | 10회 median | 28.4395 ms | mip1~11 `GenerateChain()` | 평균 29.0147 ms, sd 1.4446 ms |
| **Ours Theorem S (optimized)** | 10회 median | **15.2700 ms** | mip1~11 `GenerateChain()` | 평균 15.7949 ms, sd 3.5996 ms |
| bc7enc fast encoding only | level별 1회 합 | 26.000 ms | mip0~11 BC1 encode 합 | downsampling 제외, timer 해상도 1 ms |
| bc7enc fast total processing | level별 1회 합 | 131.000 ms | 12개 독립 프로세스 합 | PNG 입출력 포함 |
| bc7enc quality encoding only | level별 1회 합 | 1136.000 ms | mip0~11 BC1 encode 합 | downsampling 제외 |
| bc7enc quality total processing | level별 1회 합 | 1242.000 ms | 12개 독립 프로세스 합 | PNG 입출력 포함 |
| etcpak mipmap MT | 10회 median | 71.5135 ms | mip0 encode + 전체 mip 생성/encode | 범위 68.580~77.194 ms |

중앙값을 단순히 나누면 optimized Theorem S는 etcpak보다 약 4.68배, bc7enc fast의 encoding-only 합보다 약 1.70배, bc7enc quality의 encoding-only 합보다 약 74.4배 짧다. 다만 세 도구의 측정 범위가 서로 다르므로 이 비율은 end-to-end 동등 조건의 speedup이 아니다. 아래 비교 분석의 단서를 함께 읽어야 한다.

### 비교 분석

1. **최적화 전후 (동일 구현)**

   출력이 byte 단위로 동일하므로 품질 변화는 정확히 0이고, 시간만 -46.31% 감소했다. EXP-03이 남긴 "Theorem S의 수학적 독립성과 CPU 최적 실행 방식은 별개"라는 관찰은 여전히 맞지만, 그 결론이었던 "direct scheduling은 CPU 성능에 불리하다"는 구현 방식의 문제였지 Theorem S 자체의 비용이 아니었다. level 간 독립성은 스케줄링의 자유도일 뿐, 반드시 $M$을 매번 다시 훑어야 한다는 뜻이 아니다.

2. **Theorem S 대 Recursive**

   mip 2~11의 모든 level에서 PSNR이 +0.884~+2.241 dB 개선되고, mip 2~10의 FLIP이 모두 개선된다. 이 개선을 15.2005 ms 대비 +0.46%, 즉 측정 산포 안의 비용으로 얻는다. Recursive 경로를 유지할 이유가 사실상 사라졌다.

3. **Theorem S 대 bc7enc**

   mip1~10 평균 PSNR 기준 bc7enc fast와의 격차가 Recursive의 1.754 dB에서 0.486 dB로 좁혀졌다. FLIP 격차도 0.009292에서 0.003131로 줄었다. 다만 두 방식은 여전히 동일 작업이 아니다. bc7enc는 각 level의 정답 reference 이미지를 직접 입력받아 독립 인코딩하는 반면, 우리 구현은 압축된 mip 0 하나만 읽고 나머지 전 level을 만든다. 남은 0.486 dB에는 encoder 품질 차이뿐 아니라 $M$이 mip 0의 BC1 양자화 결과라는 점, 그리고 4×4 block mean이 reference downsampler와 다르다는 근사 오차가 함께 들어있다.

   속도 면에서는 bc7enc fast의 encoding-only 합보다 1.70배 짧고 total processing 합보다 8.58배 짧다. bc7enc는 mip 0 인코딩을 포함하고 12개의 독립 프로세스로 실행되었으며 내부 timer 해상도가 1 ms(작은 level 대부분이 0~1 ms로 기록)라는 점을 감안해야 한다.

4. **Theorem S 대 etcpak**

   모든 level에서 Theorem S가 앞선다. mip1~10 평균 PSNR 격차는 11.25 dB이며, level이 작아질수록 벌어져 mip 10에서는 +30.43 dB에 이른다. etcpak은 mip 9 이하에서 PSNR 10 dB대, FLIP 0.7 수준으로 붕괴한다. EXP-02에서 지적한 대로 이 급락이 실제 encoder/downsampler 품질 문제인지 DDS mip offset 또는 decoder convention 차이인지는 아직 분리 검증하지 않았으므로, 이 격차를 그대로 encoder 품질 차이로 해석하면 안 된다. 시간은 etcpak 중앙값의 1/4.68이지만 etcpak은 mip 0 인코딩까지 포함한다.

5. **품질-속도 위치**

   optimized Theorem S는 Recursive와 동일한 시간대에서 Recursive보다 높은 품질을 제공하고, bc7enc fast와 비교하면 품질은 0.486 dB 낮지만 measured encoding time은 1.70배 짧으며 압축된 mip 0만 입력으로 요구한다. 즉 이번 최적화의 의미는 순위 역전이 아니라, Theorem S를 비용 없이 채택 가능한 기본 경로로 만든 것이다.

## 부족했던 점과 실패 원인

- 최적화 후 10회 측정에 25.521 ms outlier가 포함되어 표준편차가 3.5996 ms로 커졌다. 원인(백그라운드 부하, thread pool 초기화, 페이지 폴트)을 분리하지 않았고 측정 횟수도 늘리지 않았다.
- Recursive 15.2005 ms와 optimized 15.2700 ms의 차이는 산포 안에 있어 통계적으로 구분되지 않는다. 두 구현의 우열을 시간으로 주장하려면 반복 횟수를 늘리고 신뢰구간을 제시해야 한다.
- 2×2 halving과 단일 $s_L \times s_L$ box는 2의 거듭제곱 해상도에서만 정확히 동등하다. 중간 level에 홀수 폭이 생기면 edge clamp 가중치가 달라지므로, non-power-of-two 입력에서의 동등성은 별도 검증이 필요하다. 이번에는 2048×2048만 확인했다.
- byte 단위 동일성은 단일 texture 1개에 대해서만 확인했다. 서로 다른 색 분포에서도 동일한지는 검증하지 않았다.
- 품질 비교 전체가 texture 1개에 기반하므로 일반화할 수 없다.
- bc7enc의 시간은 level별 1회 내부 timer의 합이며 해상도가 1 ms다. 10회 중앙값을 쓰는 우리 구현·etcpak과 반복 조건이 다르다.
- 세 도구의 측정 범위(mip 0 포함 여부, 입력 표현, 프로세스 수)가 달라 speedup 비율은 참고값에 머문다.
- etcpak의 mip 9~11 품질 급락 원인을 여전히 분리하지 않았다.
- level별 시간 분포는 계측 오버헤드가 포함된 대표 실행 1회씩이며 반복 측정하지 않았다.
- 정성적 artifact 비교 이미지는 이번 보고서에도 포함하지 않았다.

## 실험을 통해 발견한 점

- EXP-03의 실행시간 증가는 mip level 전반이 아니라 block이 1개뿐인 mip 9~11에 12.95 ms가 집중된 결과였다. 총 시간만 보면 알 수 없고 level별 계측이 필요했다.
- 비용을 키운 것은 box 누산 자체가 아니라 그 위에 곱해진 clamp 16배 중복과 SIMD lane 4배 중복이었다. 필요한 sample의 64배를, 그것도 단일 스레드로 처리하고 있었다.
- linear domain box filter의 결합성을 이용하면 Theorem S의 $T_L$을 mean pyramid로 계산할 수 있고, 결과는 근사가 아니라 byte 단위로 동일하다. BC1 재인코딩이 개입하지 않으므로 recursive quantization error 제거 성질도 유지된다.
- 그 결과 Theorem S는 Recursive와 같은 시간대에서 mip 2~11 PSNR +0.884~+2.241 dB를 제공한다. EXP-03의 품질-속도 trade-off 결론은 더 이상 유효하지 않다.
- bc7enc fast와의 mip1~10 평균 PSNR 격차가 1.754 dB에서 0.486 dB로 좁혀졌다. 남은 격차의 상당 부분은 encoder 품질이 아니라 입력 조건 차이(reference 이미지 직접 입력 대 압축된 mip 0)에서 온다.
- 2×2 이하의 극소 level에서는 표현 가능한 결과가 적어 서로 다른 encoder가 동일한 block에 수렴할 수 있다(mip 10에서 Theorem S와 bc7enc fast의 PSNR·FLIP 완전 일치).
- 성능 회귀를 총 실행시간만으로 진단하면 원인을 잘못 짚는다. 이번 경우 처음 의심했던 `SourceBlockMeans` 저장 비용은 실측 약 0.9 ms로 전체의 3% 수준이었다.

## 후속 실험과 개선 방향

1. 성능 측정을 30회 이상으로 늘리고 중앙값과 함께 사분위 범위를 기록하여, Recursive와 optimized Theorem S의 차이가 유의한지 판정한다.
2. 25.521 ms outlier를 재현 조건과 함께 분리한다. thread pool 초기화, 첫 touch 페이지 폴트, 백그라운드 부하를 각각 통제한 측정을 수행한다.
3. non-power-of-two 해상도에서 mean pyramid 결과와 단일 $s_L$ box 결과를 `allclose`로 비교하고, 불일치가 생기는 경계 조건을 문서화한다.
4. 최소 4개 이상의 texture corpus에서 byte 단위 동일성과 level별 PSNR·FLIP을 반복 검증한다.
5. mip 1이 여전히 전체 시간의 약 70%를 차지한다. `ProcessRowBC1()`의 lane 추출이 runtime index `switch`에 의존하는 부분을 SoA 또는 명시적 SIMD로 바꿔 다음 병목을 공략한다.
6. bc7enc fast/quality도 각 level을 10회 반복해 encoding-only median을 합산하고, 공통 BC1 decoder를 사용하도록 평가 파이프라인을 통일한다.
7. etcpak의 mip 9~11 DDS offset, block padding, decode 결과를 원본 tool의 출력과 교차 검증하여 품질 급락의 원인을 분리한다.
8. $M$을 mip 0의 BC1 결과가 아니라 reference mip 0에서 직접 계산한 변형을 만들어, bc7enc와의 남은 0.486 dB 중 encoder 품질과 입력 조건이 각각 얼마인지 ablation한다.
9. $M$의 저장 형식(8-bit linear, 16-bit linear, float32)에 따른 품질과 memory bandwidth trade-off를 측정한다.
