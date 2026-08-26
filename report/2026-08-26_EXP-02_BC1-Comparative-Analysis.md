# EXP-02: BC1 Mipmap 품질 및 생성 성능 비교

## 실험 목적과 가설

현재 compression-domain BC1 mipmap encoder를 strict opaque endpoint 보정 이전 구현, bc7enc_rdo, etcpak과 비교한다. 품질은 서로 다른 해상도의 mip을 하나의 평균으로 합치지 않고 level별 PSNR과 FLIP을 그대로 비교한다. 성능은 각 도구가 실제로 측정한 범위를 구분해 기록한다.

비교 대상은 다음과 같다.

- `Previous ours`: strict `color0 > color1` 보정 직전의 Hardware Palette Linearization 구현
- `Current ours`: strict `color0 > color1` 보정이 적용된 현재 구현
- `bc7enc fast`: BC1 `-L0 -c`, 각 reference mip을 독립적으로 인코딩
- `bc7enc quality`: BC1 `-L18 -c`, 각 reference mip을 독립적으로 인코딩
- `etcpak mipmap`: `-M -m -c bc1 -h dds`, mip0에서 전체 mip chain 생성

strict opaque 보정은 동일 endpoint가 발생하는 block에만 영향을 주므로 대부분의 큰 mip에서는 품질 변화가 작고, 평탄 block 비율이 높은 작은 mip에서 차이가 나타날 것으로 예상했다.

## 적용한 기술과 구현 내용

- Reference는 `target/Ceramic_0557_brick_uneven_stones_color/mip0.png`부터 `mip11.png`까지 사용했다.
- PSNR은 RGB 8-bit 전체 채널 MSE로 level별 계산했다.
- FLIP은 NVIDIA LDR-FLIP mean error, sRGB 입력, 기본 67 PPD로 level별 계산했다.
- bc7enc는 mipmap 생성 기능이 없으므로 각 reference level을 별도 입력으로 사용했다.
- etcpak은 mip0 하나에서 전체 mip chain을 생성했다.
- 현재 구현은 compressed mip0에서 mip1을 만들고, 생성된 compressed mip을 다음 level 입력으로 사용하는 recursive compression-domain 경로다.
- 현재 구현 성능은 `CpuBackend::GenerateChain()`만 10회 측정해 중앙값을 사용했다.
- etcpak 성능은 10회 측정값의 중앙값을 사용했다.

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 날짜 | 2026-08-26 |
| 운영체제 | Windows 11 |
| CPU logical core | 12 |
| 입력 크기 | 2048×2048, mip0~mip11 |
| 현재 구현 빌드 | MSVC Release |
| 품질 환경 | Conda `ntc` |
| Previous ours 품질 원본 | `output/test/metrics_hardware_palette/metrics.csv` |
| Current ours 품질 원본 | `output/test/metrics_ours/metrics.csv` |
| Baseline 원본 | `output/baseline/Ceramic_0557_brick_uneven_stones_color/20260826_145808` |

주의: 우리 구현의 mip0는 생성 결과가 아니라 입력 DDS의 base level이다. 따라서 mipmap 생성 품질에 대한 직접 분석은 mip1부터 시작한다.

## 정량적·정성적 결과

### Mip level별 PSNR

PSNR은 높을수록 좋다.

| Mip | 해상도 | Previous ours | Current ours | bc7enc fast | bc7enc quality | etcpak mipmap |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 43.514276 | 43.514276 | 42.356697 | 43.514447 | 38.697344 |
| 1 | 1024×1024 | 40.334059 | 40.335939 | 41.109214 | 41.917952 | 36.140151 |
| 2 | 512×512 | 38.854416 | 38.854912 | 40.133554 | 40.835686 | 34.061096 |
| 3 | 256×256 | 38.290590 | 38.291303 | 39.736235 | 40.396989 | 32.498188 |
| 4 | 128×128 | 37.819062 | 37.817952 | 39.325089 | 39.981382 | 31.173056 |
| 5 | 64×64 | 37.254292 | 37.267976 | 38.800755 | 39.403533 | 29.688860 |
| 6 | 32×32 | 36.557283 | 36.560433 | 38.127412 | 38.607900 | 27.783156 |
| 7 | 16×16 | 34.843664 | 34.843664 | 37.350316 | 37.669837 | 26.076159 |
| 8 | 8×8 | 34.757251 | 34.757251 | 37.441991 | 37.645495 | 24.100224 |
| 9 | 4×4 | 37.005312 | 37.005312 | 39.891716 | 39.891716 | 23.477285 |
| 10 | 2×2 | 39.237787 | 39.237787 | 40.597527 | 45.120504 | 10.165234 |
| 11 | 1×1 | 43.359591 | 42.110204 | inf | inf | 10.200820 |

### Mip level별 FLIP

FLIP은 낮을수록 좋다.

| Mip | 해상도 | Previous ours | Current ours | bc7enc fast | bc7enc quality | etcpak mipmap |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 0.026732 | 0.026732 | 0.034014 | 0.026731 | 0.055746 |
| 1 | 1024×1024 | 0.038143 | 0.038134 | 0.033811 | 0.028078 | 0.069855 |
| 2 | 512×512 | 0.041417 | 0.041404 | 0.034847 | 0.029326 | 0.083737 |
| 3 | 256×256 | 0.042753 | 0.042743 | 0.035675 | 0.029517 | 0.096833 |
| 4 | 128×128 | 0.045565 | 0.045611 | 0.037028 | 0.030119 | 0.108889 |
| 5 | 64×64 | 0.047690 | 0.047684 | 0.038475 | 0.031716 | 0.121924 |
| 6 | 32×32 | 0.050109 | 0.049926 | 0.040099 | 0.037758 | 0.136703 |
| 7 | 16×16 | 0.053240 | 0.053240 | 0.050135 | 0.042576 | 0.146197 |
| 8 | 8×8 | 0.071882 | 0.071882 | 0.045661 | 0.038815 | 0.149955 |
| 9 | 4×4 | 0.068253 | 0.068253 | 0.054902 | 0.054902 | 0.196747 |
| 10 | 2×2 | 0.059890 | 0.059890 | 0.055219 | 0.031297 | 0.719094 |
| 11 | 1×1 | 0.073318 | 0.052784 | 0.000000 | 0.000000 | 0.698326 |

### Strict opaque 보정 전후 변화

| Mip | Δ PSNR: Current − Previous | Δ FLIP: Current − Previous | 해석 |
|---:|---:|---:|---|
| 0 | +0.000000 | +0.000000 | 입력 base level이므로 동일 |
| 1 | +0.001880 | -0.000009 | 사실상 동일, 미세 개선 |
| 2 | +0.000496 | -0.000013 | 사실상 동일, 미세 개선 |
| 3 | +0.000713 | -0.000010 | 사실상 동일, 미세 개선 |
| 4 | -0.001110 | +0.000046 | 사실상 동일, 미세 악화 |
| 5 | +0.013684 | -0.000006 | 작은 PSNR 개선 |
| 6 | +0.003150 | -0.000183 | 미세 개선 |
| 7 | +0.000000 | +0.000000 | 동일 |
| 8 | +0.000000 | +0.000000 | 동일 |
| 9 | +0.000000 | +0.000000 | 동일 |
| 10 | +0.000000 | +0.000000 | 동일 |
| 11 | -1.249387 | -0.020534 | PSNR은 하락했지만 perceptual FLIP은 개선 |

strict endpoint 보정은 mip1~10의 품질을 거의 바꾸지 않았다. 이는 해당 level에서 endpoint equality가 드물거나, 1 blue-LSB 조정 후 selector 재할당 결과가 기존 reconstruction과 거의 같았음을 의미한다. 반면 1×1인 mip11에서는 PSNR과 FLIP의 방향이 엇갈렸다. 수치상 squared error는 커졌지만 FLIP 기준 perceptual error는 감소했다.

### Mipmap generation 및 encoding time

| 방법 | 반복 | 대표 시간 | 측정 범위 | 비고 |
|---|---:|---:|---|---|
| Previous ours | 1회 | 15.291 ms | mip1~mip11 `GenerateChain()` | 이전 단일 측정 |
| Current ours | 10회 median | 15.2005 ms | mip1~mip11 `GenerateChain()` | 평균 15.0501 ms, 최소 13.283 ms, 최대 16.962 ms |
| bc7enc fast encoding only | level별 1회 합 | 26.000 ms | mip0~mip11 BC1 encode 합 | downsampling 제외 |
| bc7enc fast total processing | level별 1회 합 | 131.000 ms | 12개 독립 프로세스의 processing time 합 | PNG 입력 및 부가 처리 포함 |
| bc7enc quality encoding only | level별 1회 합 | 1136.000 ms | mip0~mip11 BC1 encode 합 | downsampling 제외 |
| bc7enc quality total processing | level별 1회 합 | 1242.000 ms | 12개 독립 프로세스의 processing time 합 | PNG 입력 및 부가 처리 포함 |
| etcpak mipmap MT | 10회 median | 71.5135 ms | mip0 encode와 전체 mip 생성/encode | `-M -m`, 10회 범위 68.580~77.194 ms |

현재 구현의 10회 원시 측정값은 다음과 같다.

```text
13.966, 13.648, 13.283, 14.183, 15.997,
14.597, 15.939, 16.962, 15.804, 16.122 ms
```

현재 구현이 생성하는 mip1~mip11의 texel 수는 1,398,101개이며, 중앙값 기준 처리량은 약 91.977 MPixel/s이다. 이전 단일 측정 15.291 ms와 비교하면 현재 중앙값은 약 0.592% 짧지만, 이전 값은 한 번만 측정했으므로 성능 개선으로 단정할 수 없다.

숫자만 나누면 현재 구현은 etcpak 중앙값보다 약 4.70배 짧고, bc7enc fast의 encoding-only 합보다 약 1.71배 짧다. 그러나 현재 구현은 이미 압축된 mip0에서 하위 level만 생성하는 반면, etcpak은 mip0 인코딩까지 포함하고 bc7enc는 이미 만들어진 각 reference mip을 독립 인코딩한다. 따라서 이 비율은 end-to-end 동등 조건의 speedup이 아니라 현재 측정 구성에서의 참고값이다.

### 비교 분석

1. **현재 구현 대 이전 구현**

   큰 mip에서 strict opaque 보정의 영향은 거의 없다. mip1~6의 PSNR 변화는 최대 +0.013684 dB이고 FLIP 변화도 매우 작다. 보정의 주된 가치는 평균 품질 상승보다는 BC1 opaque mode의 형식적 안전성 보장에 있다.

2. **현재 구현 대 bc7enc**

   mip1~11에서 bc7enc fast와 quality가 모두 현재 구현보다 높은 PSNR과 대체로 낮은 FLIP을 기록했다. 특히 bc7enc quality가 가장 높은 품질을 보였다. 다만 bc7enc는 각 level의 정답 이미지를 직접 입력받아 독립 인코딩하고, 현재 구현은 이전 compressed mip에서 다음 mip을 재귀적으로 생성한다. 따라서 이 차이에는 BC1 encoder 품질뿐 아니라 downsampling 근사와 recursive error accumulation이 함께 포함된다.

3. **현재 구현 대 etcpak**

   현재 구현은 모든 level에서 etcpak보다 높은 PSNR과 낮은 FLIP을 기록했다. etcpak은 mip level이 작아질수록 오차가 빠르게 증가했고, mip10과 mip11에서 PSNR 약 10 dB 및 FLIP 약 0.7까지 악화되었다. 작은 mip의 padding, downsampling 또는 BC1 decode 결과를 별도로 시각 검증할 필요가 있다.

4. **PSNR과 FLIP의 불일치**

   mip11의 strict opaque 보정은 PSNR을 1.249387 dB 낮추면서 FLIP을 0.020534 개선했다. 단일 지표만으로 endpoint 보정을 평가하면 결론이 달라질 수 있으므로 두 지표를 함께 유지해야 한다.

## 부족했던 점과 실패 원인

- bc7enc의 각 mip 직접 인코딩과 현재 구현의 recursive compression-domain 생성은 동일한 작업이 아니다.
- 현재 구현과 etcpak은 전체 chain을 생성하지만 mip0 포함 여부와 입력 표현이 다르다.
- bc7enc 성능은 level별 한 번의 내부 timer를 합산한 값이며, 현재 구현 및 etcpak의 10회 중앙값과 반복 조건이 다르다.
- Previous ours 성능은 단일 실행값이어서 현재 median과 통계적으로 비교하기 어렵다.
- 단일 texture만 사용했으므로 다른 색 분포와 해상도에 결과를 일반화할 수 없다.
- etcpak의 작은 mip 품질 급락은 실제 encoder/downsampler 문제인지 DDS mip 추출 또는 decoder convention 차이인지 아직 분리 검증하지 않았다.
- 정성적 artifact 비교 이미지는 이번 보고서에 포함하지 않았다.

## 실험을 통해 발견한 점

- strict opaque endpoint 보정은 일반 level의 품질을 사실상 유지하면서 `color0 > color1` 조건을 보장한다.
- 현재 compression-domain 방식은 etcpak mip chain보다 높은 품질과 짧은 측정 시간을 기록했다.
- 직접 reference mip을 인코딩하는 bc7enc에는 품질이 미치지 못하며, 특히 recursive error accumulation 제거가 핵심 개선 방향이다.
- 작은 mip에서는 PSNR과 FLIP이 서로 다른 결론을 낼 수 있다.
- 성능 비교에는 `encoding only`, `total processing`, mip0 포함 여부를 반드시 함께 표기해야 한다.

## 후속 실험과 개선 방향

- 현재 구현의 이전 버전도 10회 반복해 median을 다시 측정한다.
- bc7enc fast/quality도 각 level을 10회 반복해 encoding-only median을 합산한다.
- 공통 입력과 공통 BC1 decoder를 사용하도록 평가 파이프라인을 통일한다.
- etcpak의 mip10·mip11 DDS offset, block padding 및 decoder 결과를 원본 tool의 decode 출력과 교차 검증한다.
- 여러 texture corpus에서 level별 PSNR·FLIP과 median runtime을 반복 측정한다.
- level0에서 각 mip을 직접 계산하는 경로를 추가해 recursive error accumulation의 영향을 ablation한다.
