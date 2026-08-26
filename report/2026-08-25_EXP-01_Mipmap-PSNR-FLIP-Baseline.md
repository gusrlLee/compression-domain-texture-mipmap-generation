# EXP-01: Mipmap PSNR·FLIP Baseline

## 실험 목적과 가설

Compression-domain BC1 mipmap 생성 결과를 정답 이미지와 level별로 비교하여 화질 저하 양상을 정량화한다. 해상도가 낮아지고 mip generation이 반복될수록 누적 오차로 인해 PSNR은 낮아지고 mean FLIP error는 증가할 것으로 예상했다.

## 적용한 기술과 구현 내용

- RGB 8-bit 전체 채널의 Mean Squared Error를 이용해 PSNR을 계산했다.
- `flip-evaluator 1.7`의 공식 Python API로 LDR-FLIP mean error를 계산했다.
- `mipN.png` 파일명을 기준으로 reference와 test level을 대응시켰다.
- 이미지 누락과 해상도 불일치를 평가 전에 검사했다.
- 결과를 재사용하기 쉽도록 CSV와 Markdown으로 저장했다.

구현 스크립트: `scripts/evaluate_mipmaps.py`

## 실험 환경과 설정

| 항목 | 설정 |
|---|---|
| 실행 환경 | Conda `ntc` |
| Python | 3.13.13 |
| NumPy | 2.4.3 |
| Pillow | 12.1.1 |
| FLIP | `flip-evaluator 1.7` |
| FLIP dynamic range | LDR |
| FLIP input color space | sRGB |
| FLIP viewing condition | 기본값 67 PPD |
| Reference | `target/Ceramic_0557_brick_uneven_stones_color` |
| Test | `output/test` |

실행 명령:

```powershell
conda run -n ntc python scripts/evaluate_mipmaps.py
```

## 정량적·정성적 결과

| Mip | 해상도 | PSNR (dB) | Mean FLIP |
|---:|---:|---:|---:|
| 0 | 2048×2048 | 43.514276 | 0.026732 |
| 1 | 1024×1024 | 40.097402 | 0.039066 |
| 2 | 512×512 | 38.630585 | 0.042182 |
| 3 | 256×256 | 38.026096 | 0.044069 |
| 4 | 128×128 | 37.401495 | 0.045766 |
| 5 | 64×64 | 36.785623 | 0.049073 |
| 6 | 32×32 | 36.060017 | 0.051879 |
| 7 | 16×16 | 34.990155 | 0.055934 |
| 8 | 8×8 | 35.100194 | 0.058295 |
| 9 | 4×4 | 39.043720 | 0.046797 |
| 10 | 2×2 | 39.837766 | 0.048245 |
| 11 | 1×1 | 43.359591 | 0.061181 |

Mip 0부터 mip 7까지 PSNR은 대체로 감소하고 mean FLIP은 증가해 반복 mip generation에 따른 오차 누적 경향이 나타났다. 4×4 이하에서는 표본 수가 매우 작아 PSNR과 FLIP이 단조롭게 변하지 않았다.

## 부족했던 점과 실패 원인

- 단일 texture만 평가했으므로 일반적인 BC1 texture 품질을 대표하지 않는다.
- PSNR은 sRGB PNG 값에서 계산했으므로 linear-light 기준 오차와 다르다.
- FLIP은 기본 67 PPD를 사용했다. 실제 표시 크기와 시청 거리를 반영하지 않았다.
- 작은 mip level은 pixel 수가 적어 평균 metric의 변동성이 크다.
- 현재 결과는 BC1 decoding 오차와 mipmap generation 오차를 함께 포함한다.

## 실험을 통해 발견한 점

- mip 1에서 mip 8까지 해상도가 감소할수록 perceptual error가 전반적으로 증가한다.
- mip 7의 PSNR은 34.990155 dB로 평가 level 중 가장 낮다.
- mip 11은 PSNR이 높지만 mean FLIP은 가장 높다. 1×1 이미지에서는 두 metric을 일반 해상도와 동일하게 해석하기 어렵다.
- Reference와 test의 mip 0도 완전히 동일하지 않으므로, base-level encoding 차이가 전체 결과에 포함되어 있다.

## 후속 실험과 개선 방향

- 여러 종류의 texture를 batch 평가하고 level별 평균과 분산을 계산한다.
- reference mip을 만드는 downsampling filter와 color space를 명시하고 동일 조건을 보장한다.
- compression-domain 결과를 conventional decode-downsample-reencode 방식과 비교한다.
- FLIP error map을 함께 저장하여 artifact 위치와 metric 변화를 연결한다.
- 작은 mip은 별도 집계하거나 pixel 수 기반 weighted metric을 추가한다.
