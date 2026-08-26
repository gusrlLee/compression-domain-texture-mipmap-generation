# BC1 Encoder Baseline 실험

## 실험 목적과 가설

동일한 reference mip chain을 기준으로 bc7enc_rdo의 최고 속도 및 최고 품질 설정과 etcpak의 mipmap 생성 경로를 비교한다. bc7enc 최고 품질 설정은 더 높은 PSNR과 더 낮은 FLIP을, etcpak은 더 높은 처리량을 보일 것으로 예상한다.

품질은 mip level별로 독립적으로 기록하며, 서로 다른 해상도의 level을 하나의 평균으로 합치지 않는다.

## 적용한 기술과 구현 내용

- `bc7enc BC1 fast`: `-1 -L0 -c`, 각 reference mip를 독립적으로 encode/decode
- `bc7enc BC1 quality`: `-1 -L18 -c`, 각 reference mip를 독립적으로 encode/decode
- `etcpak BC1 mipmap`: `-m -c bc1 -h dds`, mip0에서 전체 mip chain 생성
- PSNR: level별 RGB 8-bit MSE 기준
- FLIP: level별 NVIDIA LDR-FLIP mean error, 기본 67 PPD

## 실험 환경과 설정

- 실행 시각: `2026-08-26T14:41:26+09:00`
- 운영체제: `Windows-11-10.0.26200-SP0`
- CPU logical core 수: `12`
- Python: `3.13.13`
- Reference: `C:\Users\hyeon\Desktop\gen_mipmap\902_source\compression-domain-texture-mipmap-generation\target\Ceramic_0557_brick_uneven_stones_color`
- 결과 디렉터리: `C:\Users\hyeon\Desktop\gen_mipmap\902_source\compression-domain-texture-mipmap-generation\output\baseline\Ceramic_0557_brick_uneven_stones_color\20260826_144113`
- bc7enc 시간: 각 프로세스가 출력한 `Total encoding time`의 level별 합
- etcpak 시간: 전체 mip chain에 대해 출력한 `Encoding Time`

## 정량적·정성적 결과

### Performance

| Encoder | 전체 texel 수 | Encoding time (ms) | Throughput (Mpix/s) |
|---|---:|---:|---:|
| bc7enc_bc1_fast_L0 | 5592405 | 27.000000 | 207.126111 |
| bc7enc_bc1_quality_L18 | 5592405 | 1120.000000 | 4.993219 |
| etcpak_bc1_mipmap | 5592405 | 70.440000 | 79.392462 |

### bc7enc_bc1_fast_L0: level별 품질

| Mip | 해상도 | PSNR (dB) | Mean FLIP | Encoding time (ms) |
|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 42.356697 | 0.034014 | 15.000000 |
| 1 | 1024×1024 | 41.109214 | 0.033811 | 3.000000 |
| 2 | 512×512 | 40.133554 | 0.034847 | 1.000000 |
| 3 | 256×256 | 39.736235 | 0.035675 | 1.000000 |
| 4 | 128×128 | 39.325089 | 0.037028 | 1.000000 |
| 5 | 64×64 | 38.800755 | 0.038475 | 0.000000 |
| 6 | 32×32 | 38.127412 | 0.040099 | 1.000000 |
| 7 | 16×16 | 37.350316 | 0.050135 | 1.000000 |
| 8 | 8×8 | 37.441991 | 0.045661 | 1.000000 |
| 9 | 4×4 | 39.891716 | 0.054902 | 1.000000 |
| 10 | 2×2 | 40.597527 | 0.055219 | 1.000000 |
| 11 | 1×1 | inf | 0.000000 | 1.000000 |

### bc7enc_bc1_quality_L18: level별 품질

| Mip | 해상도 | PSNR (dB) | Mean FLIP | Encoding time (ms) |
|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 43.514447 | 0.026731 | 816.000000 |
| 1 | 1024×1024 | 41.917952 | 0.028078 | 205.000000 |
| 2 | 512×512 | 40.835686 | 0.029326 | 76.000000 |
| 3 | 256×256 | 40.396989 | 0.029517 | 13.000000 |
| 4 | 128×128 | 39.981382 | 0.030119 | 4.000000 |
| 5 | 64×64 | 39.403533 | 0.031716 | 1.000000 |
| 6 | 32×32 | 38.607900 | 0.037758 | 1.000000 |
| 7 | 16×16 | 37.669837 | 0.042576 | 1.000000 |
| 8 | 8×8 | 37.645495 | 0.038815 | 0.000000 |
| 9 | 4×4 | 39.891716 | 0.054902 | 1.000000 |
| 10 | 2×2 | 45.120504 | 0.031297 | 1.000000 |
| 11 | 1×1 | inf | 0.000000 | 1.000000 |

### etcpak_bc1_mipmap: level별 품질

| Mip | 해상도 | PSNR (dB) | Mean FLIP | Encoding time (ms) |
|---:|---:|---:|---:|---:|
| 0 | 2048×2048 | 38.697344 | 0.055746 | chain 전체 측정 |
| 1 | 1024×1024 | 36.140151 | 0.069855 | chain 전체 측정 |
| 2 | 512×512 | 34.061096 | 0.083737 | chain 전체 측정 |
| 3 | 256×256 | 32.498188 | 0.096833 | chain 전체 측정 |
| 4 | 128×128 | 31.173056 | 0.108889 | chain 전체 측정 |
| 5 | 64×64 | 29.688860 | 0.121924 | chain 전체 측정 |
| 6 | 32×32 | 27.783156 | 0.136703 | chain 전체 측정 |
| 7 | 16×16 | 26.076159 | 0.146197 | chain 전체 측정 |
| 8 | 8×8 | 24.100224 | 0.149955 | chain 전체 측정 |
| 9 | 4×4 | 23.477285 | 0.196747 | chain 전체 측정 |
| 10 | 2×2 | 10.165234 | 0.719094 | chain 전체 측정 |
| 11 | 1×1 | 10.200820 | 0.698326 | chain 전체 측정 |

정성적 artifact 판정은 자동화하지 않았으며 생성된 `mipN.png`를 별도로 육안 확인해야 한다.

## 부족했던 점과 실패 원인

- bc7enc는 mipmap 생성 기능이 없어 각 mip를 별도 프로세스로 실행했다. 기록된 encoding time은 프로세스 시작 비용을 제외한 encoder 내부 시간이지만, etcpak의 단일 mip-chain 실행과 작업 구성은 동일하지 않다.
- etcpak은 chain 전체 시간만 출력하므로 level별 encoding time은 제공하지 않는다.
- 한 장의 texture와 한 번의 실행만으로는 시스템 노이즈나 texture 특성에 대한 일반화를 할 수 없다.
- bc7enc와 etcpak의 BC1 decoder rounding 규칙 차이가 품질 수치에 영향을 줄 수 있다.

## 실험을 통해 발견한 점

위 표의 level별 PSNR·FLIP 및 전체 성능 수치를 기준선으로 사용한다. 서로 다른 크기의 mip level을 단순 평균한 품질 수치는 사용하지 않는다.

## 후속 실험과 개선 방향

- 각 encoder를 여러 번 반복 실행하여 median encoding time을 기록한다.
- 여러 texture corpus에서 동일 실험을 반복한다.
- 모든 encoder 결과를 하나의 hardware-compatible BC1 decoder로 통일해 decoder 차이를 제거한다.
- 현재 compression-domain mipmap encoder 결과를 같은 표에 추가해 baseline과 비교한다.

## 원시 결과

- Level metrics: `C:\Users\hyeon\Desktop\gen_mipmap\902_source\compression-domain-texture-mipmap-generation\output\baseline\Ceramic_0557_brick_uneven_stones_color\20260826_144113\level_metrics.csv`
- Performance: `C:\Users\hyeon\Desktop\gen_mipmap\902_source\compression-domain-texture-mipmap-generation\output\baseline\Ceramic_0557_brick_uneven_stones_color\20260826_144113\performance.csv`
