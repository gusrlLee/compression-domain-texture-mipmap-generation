# EXP-02: ANOVA Covariance Decomposition

## 실험 목적과 가설

BC1 child block의 covariance를 16개 group mean의 direct second moment로 계산하던 구현을 ANOVA의 between-parent covariance와 within-parent covariance로 명시적으로 분해한다. 두 식은 실수 산술에서 동치이므로 품질은 동일하고, 부동소수 연산 순서에 따른 미세한 차이만 발생할 것으로 예상했다.

## 적용한 기술과 구현 내용

- 각 parent block의 4개 quadrant mean으로 parent mean과 within-parent covariance를 계산했다.
- 네 parent mean과 전체 mean으로 between-parent covariance를 계산했다.
- 최종 covariance를 `Between + Within`으로 구성했다.
- 구현은 `ParentStatistics`, `ComputeParentStatistics()`, `AccumulateBetweenParentCovariance()`로 분리했다.
- C++과 Slang 공용 경로에서 동일한 함수를 사용했다.

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 빌드 | MSVC Release |
| GPU 언어 검증 | Slang SPIR-V compile |
| 입력 | `input/bc1/test.dds`, 2048×2048 BC1 |
| Reference | `target/Ceramic_0557_brick_uneven_stones_color` |
| 품질 환경 | Conda `ntc` |
| PSNR | RGB 8-bit 전체 채널 MSE |
| FLIP | LDR-FLIP, sRGB, 기본 67 PPD |

## 정량적·정성적 결과

| Mip | Baseline PSNR | ANOVA PSNR | Δ PSNR | Baseline FLIP | ANOVA FLIP | Δ FLIP |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 43.514276 | 43.514276 | 0.000000 | 0.026732 | 0.026732 | 0.000000 |
| 1 | 40.097402 | 40.097516 | +0.000114 | 0.039066 | 0.039068 | +0.000002 |
| 2 | 38.630585 | 38.629780 | -0.000805 | 0.042182 | 0.042157 | -0.000025 |
| 3 | 38.026096 | 38.027523 | +0.001427 | 0.044069 | 0.043925 | -0.000144 |
| 4 | 37.401495 | 37.401196 | -0.000299 | 0.045766 | 0.045524 | -0.000242 |
| 5 | 36.785623 | 36.843518 | +0.057895 | 0.049073 | 0.049296 | +0.000223 |
| 6 | 36.060017 | 36.249248 | +0.189231 | 0.051879 | 0.054116 | +0.002237 |
| 7 | 34.990155 | 35.229008 | +0.238853 | 0.055934 | 0.052564 | -0.003370 |
| 8 | 35.100194 | 35.083341 | -0.016853 | 0.058295 | 0.057572 | -0.000723 |
| 9 | 39.043720 | 36.198398 | -2.845322 | 0.046797 | 0.075300 | +0.028503 |
| 10 | 39.837766 | 35.783944 | -4.053822 | 0.048245 | 0.092836 | +0.044591 |
| 11 | 43.359591 | 43.359591 | 0.000000 | 0.061181 | 0.061181 | 0.000000 |

단일 실행에서 mipmap generation은 16.648 ms, 83.982 MPixel/s, 5.249 MBlock/s로 측정되었다. 반복 benchmark가 아니므로 baseline과의 성능 차이는 판단하지 않았다.

## 부족했던 점과 실패 원인

- ANOVA는 기존 total covariance와 수학적으로 동치이므로 근본적인 품질 향상을 제공하지 않는다.
- 부동소수 덧셈 순서가 달라져 RGB565 quantization 경계에서 일부 block 결과가 바뀌었다.
- 현재 구현은 생성한 compressed mip을 다음 level 입력으로 사용한다. 작은 차이가 level을 따라 누적되어 mip 9와 mip 10에서 큰 품질 차이로 증폭되었다.
- 단일 texture와 단일 시간 측정만 사용했다.

## 실험을 통해 발견한 점

- mip 1~4에서는 두 covariance 계산 방식이 사실상 동일한 품질을 보였다.
- 명시적 ANOVA 분해 자체는 품질 개선 기법이 아니라 covariance 구조를 드러내는 표현 방식이다.
- 작은 부동소수 차이도 recursive compressed mip chain에서는 후속 level의 endpoint와 selector를 바꿀 수 있다.
- 문서의 “level 0에서 직접 계산하여 오차가 누적되지 않는다”는 설명과 현재 코드의 recursive generation 방식이 일치하지 않는다.

## 후속 실험과 개선 방향

- direct moment와 ANOVA covariance를 random input에서 수치 오차 범위로 비교하는 단위 테스트를 추가한다.
- 각 mip을 level 0의 통계량에서 직접 생성하는 구조를 구현해 recursive quantization error를 제거한다.
- quality 개선은 ANOVA 분해가 아니라 selector-endpoint 반복 최적화와 RGB565 local search로 진행한다.
- 여러 texture에서 반복 benchmark와 품질 평균·분산을 측정한다.
- 작은 mip level은 별도 평가하여 quantization sensitivity를 분석한다.
