# EXP-03: Hardware Palette Linearization

## 실험 목적과 가설

BC1 endpoint를 먼저 linearize한 뒤 중간 palette를 보간하던 구현을, RGB565 endpoint에서 sRGB/code-space BC1 palette를 먼저 복원하고 네 palette entry를 각각 linearize하는 방식으로 변경한다. Reference downsampler의 `decode → per-texel linearize → average` 순서와 일치하므로 주요 mip level의 PSNR과 FLIP이 개선될 것으로 예상했다.

## 적용한 기술과 구현 내용

- `BuildOpaqueLinearPaletteBC1()` 공용 helper를 추가했다.
- RGB565 endpoint를 8-bit sRGB 값으로 확장했다.
- `c2=(2c0+c1)/3`, `c3=(c0+2c1)/3` 정수 보간으로 opaque BC1 palette를 구성했다.
- 완성된 `c0`, `c1`, `c2`, `c3`를 각각 sRGB-to-linear 변환했다.
- 입력 parent block의 quadrant mean 계산과 최종 selector 재할당에 동일한 palette helper를 사용했다.
- C++과 Slang 공용 코드 경로를 유지했다.

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 빌드 | MSVC Release |
| GPU 언어 검증 | Slang SPIR-V compile |
| 입력 | `input/bc1/test.dds`, 2048×2048 BC1 |
| Reference | `target/Ceramic_0557_brick_uneven_stones_color` |
| 품질 환경 | Conda `ntc` |
| 비교 baseline | EXP-02 explicit ANOVA 구현 |
| PSNR | RGB 8-bit 전체 채널 MSE |
| FLIP | LDR-FLIP, sRGB, 기본 67 PPD |

## 정량적·정성적 결과

| Mip | ANOVA PSNR | Palette PSNR | Δ PSNR | ANOVA FLIP | Palette FLIP | Δ FLIP |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 43.514276 | 43.514276 | 0.000000 | 0.026732 | 0.026732 | 0.000000 |
| 1 | 40.097516 | 40.334059 | +0.236543 | 0.039068 | 0.038143 | -0.000925 |
| 2 | 38.629780 | 38.854416 | +0.224636 | 0.042157 | 0.041417 | -0.000740 |
| 3 | 38.027523 | 38.290590 | +0.263067 | 0.043925 | 0.042753 | -0.001172 |
| 4 | 37.401196 | 37.819062 | +0.417866 | 0.045524 | 0.045565 | +0.000041 |
| 5 | 36.843518 | 37.254292 | +0.410774 | 0.049296 | 0.047690 | -0.001606 |
| 6 | 36.249248 | 36.557283 | +0.308035 | 0.054116 | 0.050109 | -0.004007 |
| 7 | 35.229008 | 34.843664 | -0.385344 | 0.052564 | 0.053240 | +0.000676 |
| 8 | 35.083341 | 34.757251 | -0.326090 | 0.057572 | 0.071882 | +0.014310 |
| 9 | 36.198398 | 37.005312 | +0.806914 | 0.075300 | 0.068253 | -0.007047 |
| 10 | 35.783944 | 39.237787 | +3.453843 | 0.092836 | 0.059890 | -0.032946 |
| 11 | 43.359591 | 43.359591 | 0.000000 | 0.061181 | 0.073318 | +0.012137 |

단일 실행 성능은 15.291 ms, 91.433 MPixel/s, 5.715 MBlock/s로 측정되었다. 주요 mip 1~6에서 PSNR은 0.22~0.42 dB 개선되었고, mip 4를 제외한 해당 구간의 FLIP도 개선되었다.

## 부족했던 점과 실패 원인

- mip 7, 8과 mip 11에서는 품질이 악화되었다.
- 현재 chain은 이전 compressed mip을 다음 입력으로 사용하므로 작은 endpoint/selector 변화가 후속 level에서 증폭된다.
- 정수 `/3` 보간의 반올림 규칙이 target BC1 decoder와 bit-exact하게 같은지 별도 검증하지 않았다.
- opaque 4-color mode만 처리하며 `color0 <= color1`인 3-color/transparent mode는 지원하지 않는다.
- 단일 texture와 단일 성능 측정만 사용했다.

## 실험을 통해 발견한 점

- sRGB/code-space에서 BC1 palette를 먼저 만든 뒤 linearize하는 순서가 고해상도 mip 품질에 실질적인 영향을 준다.
- 입력과 최종 selector 양쪽이 같은 palette reconstruction helper를 사용해야 계산 일관성이 유지된다.
- 작은 mip의 품질 변동은 palette 정확성 외에 recursive re-encoding 구조의 민감도가 크다는 점을 보여준다.
- 명시적 ANOVA보다 palette reconstruction 순서 변경이 훨씬 큰 품질 효과를 보였다.

## 후속 실험과 개선 방향

- scalar BC1 reference decoder와 palette entry를 bit 단위로 비교한다.
- 각 mip을 level 0 통계량에서 직접 생성해 recursive error accumulation을 제거한다.
- RGB565 endpoint가 같은 경우에도 opaque 4-color mode를 보장하도록 strict endpoint ordering을 구현한다.
- 3-color/transparent BC1 mode를 별도 처리한다.
- 여러 texture에서 PSNR·FLIP 평균과 반복 성능을 측정한다.
