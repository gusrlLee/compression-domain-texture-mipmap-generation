# BC-Family Proof: Exactness and Approximation in Linear Domain Mipmap Generation

## Hypothesis
우리가 사용하는 Input은 BC-family codec block이며, 본 증명의 목적은 압축 도메인(Compression Domain)에서의 Mipmap 생성이 Linear Domain에서도 정확한 수학적 근거를 바탕으로 작동함을 증명하는 것이다.

본 파이프라인은 다음 5단계로 구성된다.

1. **셀렉터 히스토그램 추출** (SWAR, Theorem 2)
2. **리니어 팔레트 구성** $\mathbf{U}_b = L(\mathbf{c}_{b,\cdot})$ (LUT)
3. **그룹 평균 및 모멘트 집계** (Theorem 1, 3, 4)
4. **엔드포인트 결정** (PCA + Least Squares + 현-곡선 갭 보정, Theorem 5, 6)
5. **셀렉터 재할당** (Theorem 7)

이 중 **근사가 발생하는 단계는 4단계(엔드포인트 결정)뿐이며**, 1·2·3·5단계는 실수 산술 모델에서 완전한 Exact 연산이다. 레벨 $\ge 2$의 경우에도 **다운샘플 값(모멘트)의 계산은 Exact**이며(Theorem S), 각 레벨의 엔드포인트 결정에서만 동일한 형태의 닫힌 형태(Closed-form) 근사가 재발생한다. 중요한 것은 이 근사가 항상 **레벨 0의 원본 압축 데이터로부터 직접** 계산되므로 **레벨을 따라 누적되지 않는다**는 점이다.

> 본 문서에서 "Exact"는 실수 산술 이상화 모델 기준이다. RGB565 양자화, 하드웨어 정수 반올림 등 실제 구현상의 제약은 §6에서 별도로 다룬다.

## 1. Notations & Definitions

*   **블록 및 계층:** 부모 블록 $b \in \{0,1,2,3\}$은 각각 4×4 텍셀을 포함한다. 2×2 그룹을 $g$라 하며(블록당 4개, 자식 블록의 1픽셀에 해당), 전체 16개의 $g$가 자식 블록 하나를 구성한다.
*   **엔드포인트 및 팔레트:** sRGB 코드 공간의 엔드포인트를 $\mathbf{e}_{b,0}, \mathbf{e}_{b,1}$라 할 때, 4개의 팔레트는 다음과 같이 정의된다.
    $$\mathbf{c}_{b,k} = \mathbf{e}_{b,0} + t_k\boldsymbol{\Delta}_b, \qquad \boldsymbol{\Delta}_b = \mathbf{e}_{b,1} - \mathbf{e}_{b,0}$$
*   **셀렉터 인덱스와 보간 파라미터의 매핑(중요):** $s_{b,i} \in \{0,1,2,3\}$은 비트스트림에 저장된 **하드웨어 인덱스**이며, BC1 4-color mode에서 인덱스 $k$와 보간 파라미터 $t_k$의 대응은 **균등 오름차순이 아니다**.
    $$t_0 = 0, \quad t_1 = 1, \quad t_2 = \tfrac{1}{3}, \quad t_3 = \tfrac{2}{3}$$
    본 문서의 모든 $k$ 첨자(팔레트 열 순서, 히스토그램 $n_{g,k}$, $\mathbf{U}_b$의 열 순서)는 **이 하드웨어 인덱스 순서로 통일**한다. Theorem 2의 SWAR 히스토그램은 비트 값에서 직접 $n_{g,k}$를 산출하므로 하드웨어 순서를 따르며, $\mathbf{U}_b$의 열을 $t$ 오름차순 $(0,\tfrac13,\tfrac23,1)$으로 배열하면 열 순열 불일치로 인해 Theorem 1의 항등식이 깨진다. 순열 행렬 $\mathbf{P}$를 명시적으로 도입하지 않는 한 두 순서를 혼용해서는 안 된다.
*   **색 공간 변환:** sRGB에서 Linear 공간으로의 변환을 $L$, 그 역변환을 $S = L^{-1}$이라 한다. $L$은 sRGB EOTF로서 $[0,1]$에서 **piecewise 정의**(저휘도 선형 구간 + 지수 2.4 구간)이며 볼록(convex)하다(§6 참조).
*   **그룹 내 셀렉터 분포:**
    $$n_{g,k} = \#\{i \in g : s_{b,i} = k\}, \quad \sum_{k=0}^{3} n_{g,k} = 4, \qquad \boldsymbol{\gamma}_g = \frac{1}{4} \mathbf{n}_g \in \Delta^3$$
*   **리니어 팔레트 행렬:**
    $$\mathbf{u}_{b,k} = L(\mathbf{c}_{b,k}), \qquad \mathbf{U}_b = [\mathbf{u}_{b,0}\ \mathbf{u}_{b,1}\ \mathbf{u}_{b,2}\ \mathbf{u}_{b,3}] \in \mathbb{R}^{3 \times 4}$$

---

## 2. Core Theorems: Exactness in Linear Domain

### Theorem 1 (압축 도메인 다운샘플의 정확성)
압축 도메인에서의 2×2 그룹 $\mathbf{y}_g$ 연산은 디코드된 리니어 텍셀의 박스 평균과 정확히 일치한다.
$$\boxed{\ \mathbf{y}_g = \mathbf{U}_b\boldsymbol{\gamma}_g\ }$$

**Proof.** 
하드웨어 디코딩을 거친 리니어 텍셀 값은 $\mathbf{x}_i = L(\mathbf{c}_{b,s_{b,i}}) = \mathbf{u}_{b,s_{b,i}}$ 이다. 따라서 그룹 $g$의 텍셀 평균은 다음과 같다.
$$\frac{1}{4}\sum_{i \in g}\mathbf{x}_i = \frac{1}{4}\sum_{i \in g}\mathbf{u}_{b,s_{b,i}} = \frac{1}{4}\sum_{k=0}^{3}n_{g,k}\mathbf{u}_{b,k} = \mathbf{U}_b\boldsymbol{\gamma}_g \qquad\blacksquare$$

> **Key Insight:** 비선형 변환 $L$은 16개의 텍셀에 각각 적용될 필요 없이, 유한한 4개의 팔레트에만 선행 적용된다. 텍셀 복원 없이 압축 인덱스 정보만으로 정확한 선형 평균을 도출하는 핵심 항등식이다. 단, $\mathbf{U}_b$는 반드시 4열을 유지해야 한다. $L$의 비선형성으로 인해 리니어 공간에서는 $\mathbf{u}_{b,2} \neq \mathbf{u}_{b,0} + \tfrac{1}{3}(\mathbf{u}_{b,1} - \mathbf{u}_{b,0})$ 이기 때문이다(§1의 하드웨어 인덱스 순서 기준: $k=0 \mapsto \mathbf{e}_0$, $k=1 \mapsto \mathbf{e}_1$, $k=2 \mapsto t=\tfrac13$).

### Theorem 2 (SWAR 기반 셀렉터 추출)
셀렉터 히스토그램은 도메인에 무관하게 SWAR(SIMD within a register) 연산을 통해 순수 비트 레벨에서 추출된다.
32비트 워드 $W$에 대하여:
*   $A = W \ \& \ \texttt{0x55555555}$ (하위 비트)
*   $B = (W \gg 1) \ \& \ \texttt{0x55555555}$ (상위 비트)

셀렉터 값은 $k = 2\cdot(\text{high}) + (\text{low})$ 이므로, 각 셀렉터의 개수 $n_{g,k}$는 그룹 마스크를 적용한 popcount로 도출된다.
$$n_{g,0} = |\overline{A} \wedge \overline{B}|_g, \quad n_{g,1} = |A \wedge \overline{B}|_g, \quad n_{g,2} = |\overline{A} \wedge B|_g, \quad n_{g,3} = |A \wedge B|_g$$

> **구현 주의:** 보수 연산 $\overline{A}, \overline{B}$는 홀수 비트 위치를 1로 채우므로, 그룹 마스크 $M_g$는 반드시 $\texttt{0x55555555}$ 격자 위에서 정의되어야 한다($M_g \subseteq \texttt{0x55555555}$). 그렇지 않으면 텍셀당 2비트가 중복 계수되어 $n_{g,k}$가 2배로 산출된다.

### Theorem 3 (모멘트 폐쇄성, Moment Closure)
$\mathbf{N}_b = \sum_{g \in b}\boldsymbol{\gamma}_g$ 이고, 공분산 구조를 위한 대칭 PSD(Positive Semi-Definite) 행렬을 $\boldsymbol{\Gamma}_b = \sum_{g \in b}\boldsymbol{\gamma}_g\boldsymbol{\gamma}_g^\top \in \mathbb{R}^{4 \times 4}$ 라 정의하면:
$$\boldsymbol{\mu} = \frac{1}{16}\sum_b \mathbf{U}_b\mathbf{N}_b, \qquad \mathbf{M}_2 = \frac{1}{16}\sum_b \mathbf{U}_b\boldsymbol{\Gamma}_b\mathbf{U}_b^\top, \qquad \boldsymbol{\Sigma} = \mathbf{M}_2 - \boldsymbol{\mu}\boldsymbol{\mu}^\top$$

**Proof.** 
$$\sum_g\mathbf{y}_g\mathbf{y}_g^\top = \sum_g\mathbf{U}_b\boldsymbol{\gamma}_g\boldsymbol{\gamma}_g^\top\mathbf{U}_b^\top = \mathbf{U}_b\Big(\sum_g\boldsymbol{\gamma}_g\boldsymbol{\gamma}_g^\top\Big)\mathbf{U}_b^\top \qquad\blacksquare$$

### Theorem 4 (ANOVA 분해 및 공선성 조건)
$\boldsymbol{\mu}_b = \mathbf{U}_b\bar{\boldsymbol{\gamma}}_b$ 이고 $\bar{\boldsymbol{\gamma}}_b = \tfrac{1}{4}\mathbf{N}_b$ 일 때, 총 공분산은 다음과 같이 분해된다.
$$\boldsymbol{\Sigma} = \underbrace{\frac{1}{4}\sum_b(\boldsymbol{\mu}_b - \boldsymbol{\mu})(\boldsymbol{\mu}_b - \boldsymbol{\mu})^\top}_{\text{Between-block}} + \underbrace{\frac{1}{4}\sum_b\mathbf{U}_b\Big(\frac{1}{4}\boldsymbol{\Gamma}_b - \bar{\boldsymbol{\gamma}}_b\bar{\boldsymbol{\gamma}}_b^\top\Big)\mathbf{U}_b^\top}_{\text{Within-block}}$$

> 이 분해에서 계수 $\tfrac14$가 성립하는 것은 **블록당 그룹 수가 4로 균일**하기 때문이다. 그룹 수가 블록마다 다른 일반화(예: 비정방 타일링)에서는 가중 ANOVA 형태로 바뀐다.

**Lemma (공선성 조건).** 채널별 거듭제곱 변환 $L(u) = u^\gamma \ (\gamma \neq 1)$ 을 가정하고, $\boldsymbol{\Delta}$의 **모든 성분이 0이 아니라고** 하자. 이때
$$L(\mathbf{c}(t)) \text{ 가 직선을 유지할 필요충분조건} \iff \mathbf{e}_0 \times \boldsymbol{\Delta} = \mathbf{0}$$

*Proof.* 접선 벡터는 $\frac{d}{dt}L(c_j(t)) = \gamma\Delta_j(e_{0,j} + t\Delta_j)^{\gamma-1}$ 이다. 직선이 되려면 접선 방향이 $t$에 무관해야 하므로, 임의의 두 채널 $j,l$에 대해
$$\frac{\Delta_j}{\Delta_l}\left(\frac{e_{0,j}+t\Delta_j}{e_{0,l}+t\Delta_l}\right)^{\gamma-1} = \text{const} \iff e_{0,j}\Delta_l = e_{0,l}\Delta_j$$
이 되어야 하며, 이는 곧 $\mathbf{e}_0 \times \boldsymbol{\Delta} = \mathbf{0}$ 이다. $\blacksquare$

> **⚠ 퇴화 사례(전제의 필요성):** $\boldsymbol{\Delta}$에 0인 성분이 있으면 위 필요조건은 성립하지 않는다. 예를 들어 $\mathbf{e}_0 = (0.3, 0.5, 0.7)$, $\boldsymbol{\Delta} = (0.4, 0, 0)$ 이면 $\mathbf{e}_0 \times \boldsymbol{\Delta} = (0, 0.28, -0.2) \neq \mathbf{0}$ 이지만, 변하는 채널이 하나뿐이므로 궤적은 여전히 직선(rank 1)이다. 따라서 엄밀한 진술은 **"$\Delta_j \neq 0$ 인 채널들끼리만 $e_{0,j}\Delta_l = e_{0,l}\Delta_j$"** 이며, 위 명제는 $\boldsymbol{\Delta}$의 성분이 모두 0이 아닌 일반적 경우에 한한다.
>
> **⚠ $L$의 형태:** 실제 sRGB EOTF는 순수 거듭제곱이 아니라 저휘도 선형 구간을 갖는 piecewise 함수이므로, 팔레트가 그 선형 구간 안에 놓이는 어두운 블록에서는 $\mathbf{e}_0 \times \boldsymbol{\Delta}$와 무관하게 궤적이 직선이 된다. 본 Lemma는 지수 구간에 대한 근사적 지침으로 해석해야 한다.

**해석.** 무채색이나 원점을 지나는 밝기 램프 블록에서 within-block 공분산은 Rank-1로 유지되고, 그 외의 경우 일반적으로 Rank-3이 된다. 다만 rank는 이산량이므로 "채도가 높을수록 rank가 상승한다"는 표현은 부정확하며, **채도가 높을수록 부수 주성분의 에너지 비중(수치적 rank)이 커진다**고 읽어야 한다. 이탈도
$$\kappa_b = \frac{|\mathbf{e}_0 \times \boldsymbol{\Delta}|}{|\mathbf{e}_0||\boldsymbol{\Delta}|}$$
가 그 자연스러운 측도 지표가 된다.

---

## 3. The PCA & Least Squares Solution

### Theorem 5 (PCA 기반 초기 엔드포인트 도출)
$\boldsymbol{\Sigma}$의 최대 고유벡터를 $\mathbf{v}$라 하자. 각 그룹의 투영값을 $\tau_g = \mathbf{v}^\top(\mathbf{y}_g - \boldsymbol{\mu})$라 하면, Linear 공간에서의 초기 엔드포인트는 다음과 같다.
$$\mathbf{p}_{0,1} = \boldsymbol{\mu} + \tau_{\min/\max}\mathbf{v}$$
이후 $w_g \in \{0, \tfrac{1}{3}, \tfrac{2}{3}, 1\}$가 정해지면, $\mathbf{a} = \mathbf{p}_0, \mathbf{d} = \mathbf{p}_1 - \mathbf{p}_0$에 대한 최소제곱법(Least Squares) 정규 방정식은 다음과 같다.
$$
\begin{pmatrix} 16 & S_1 \\ S_1 & S_2 \end{pmatrix}
\begin{pmatrix} \mathbf{a}^\top \\ \mathbf{d}^\top \end{pmatrix}
=
\begin{pmatrix} \mathbf{T}_0^\top \\ \mathbf{T}_1^\top \end{pmatrix}
$$
*(단, $S_1 = \sum_g w_g, \ S_2 = \sum_g w_g^2, \ \mathbf{T}_j = \sum_g w_g^j\mathbf{y}_g$)*

**비퇴화 조건.** 이 계의 행렬식은
$$\det = 16S_2 - S_1^2 = 16\sum_g\Big(w_g - \bar{w}\Big)^2 \ \ge 0$$
이며(Cauchy–Schwarz), **유일해를 갖는 필요충분조건은 $w_g$가 상수가 아닌 것**이다. 단색 블록이나 모든 그룹이 동일 셀렉터로 매핑되는 평탄 블록에서는 $\det = 0$ 으로 특이(singular)해지므로, 구현에서는 이 경우를 분기하여 $\mathbf{p}_0 = \mathbf{p}_1 = \boldsymbol{\mu}$ 로 처리해야 한다. 마찬가지로 $\boldsymbol{\Sigma} = \mathbf{0}$ 인 블록에서는 주성분 $\mathbf{v}$가 정의되지 않으므로 동일한 분기가 필요하다.

> **Critique:** 이 방정식은 Linear 공간 내에서의 투영 오차(Variance)를 최소화한다. 그러나 최종 목적이 sRGB 역투영을 포함한 오차 최소화라면, 이 $\mathbf{v}$ 방향은 훌륭한 휴리스틱일 뿐 수학적인 Global Optimum을 보장하지는 않는다.

---

## 4. The Chord-Curve Gap (Core Contribution)

### Theorem 6 (현-곡선 갭 보정과 $\frac{4}{9}\boldsymbol{\sigma}$ 최적화)
이 단계는 본 파이프라인에서 **유일하게 근사가 발생하는 지점**이다. Linear Least Squares는 직선 $\ell(t)$를 산출하지만, 하드웨어 디코더(`DXGI_FORMAT_BC1_UNORM_SRGB`)는 sRGB 공간에서 보간 후 $L$을 통과시킨다. 따라서 실제 복원되는 궤적은 다음과 같다.
$$\Phi(t) = L\big(\mathbf{q}_0 + t(\mathbf{q}_1 - \mathbf{q}_0)\big)$$
$L$이 Convex 함수이므로 Jensen의 부등식에 의해 $\Phi(t)$ 곡선은 항상 직선인 현(Chord)의 아래로 처지게 된다. (sRGB EOTF의 볼록성은 §6에서 확인한다.)

처짐 오차 모델을 2차 곡선 $\epsilon(t) = -4\boldsymbol{\sigma}t(1-t)$ 으로 근사한다. 
$$\boldsymbol{\sigma} = \frac{\mathbf{p}_0 + \mathbf{p}_1}{2} - L\Big(\frac{S(\mathbf{p}_0) + S(\mathbf{p}_1)}{2}\Big) \ge \mathbf{0}$$
($\boldsymbol{\sigma} \ge \mathbf{0}$ 은 $L$의 볼록성에서 직접 따라온다. 이 2차 모델은 $t \in \{0, \tfrac12, 1\}$ 에서만 실제 처짐과 일치하는 근사이다.)

엔드포인트를 $\mathbf{p}_0 \mapsto \mathbf{p}_0 + \alpha$, $\mathbf{p}_1 \mapsto \mathbf{p}_1 + (\alpha+\beta)$ 로 이동시키면 잔차는 $\alpha + \beta t - 4\boldsymbol{\sigma}t(1-t)$ 가 되므로, 최적 보정은 $4\boldsymbol{\sigma}t(1-t)$ 를 $\text{span}\{1, t\}$ 에 셀렉터 측도로 사영(Projection)하는 문제로 환원된다. BC1 4-color mode의 **균등 이산 측도**($t \in \{0,\tfrac13,\tfrac23,1\}$, 각 확률 $\tfrac14$; $m_1 = \tfrac{1}{2}, m_2 = \tfrac{7}{18}, m_3 = \tfrac{1}{3}$)를 정규방정식에 대입하면 다음과 같다.
$$\alpha + \beta m_1 = 4\boldsymbol{\sigma}(m_1 - m_2), \qquad \alpha m_1 + \beta m_2 = 4\boldsymbol{\sigma}(m_2 - m_3)$$
측도의 대칭성($t \leftrightarrow 1-t$)에 의해 $\beta = 0$, $\alpha = \tfrac{4}{9}\boldsymbol{\sigma}$ 가 도출된다.
$$\boxed{\ \mathbf{q}_j = S\big(\mathbf{p}_j + \tfrac{4}{9}\boldsymbol{\sigma}\big)\ }$$
(※ 만약 연속 균등 측도를 가정했다면 $\int_0^1 4t(1-t)dt = \tfrac{2}{3}$ 이 도출되나, BC1의 셀렉터는 4레벨 이산이므로 $\tfrac{4}{9}$ 가 도출된다.)

#### 적용 범위 (계수 $\tfrac49$의 성립 조건)
$\tfrac49$ 는 **"BC1 4-color mode + 셀렉터 균등분포"** 라는 두 전제 아래에서의 최적값이다. 전제가 바뀌면 계수도 바뀐다.

(아래 표의 확률은 모두 **$t$ 오름차순** 기준이며, 하드웨어 인덱스 순서와 다르다는 점에 유의한다.)

| 측도 | $\alpha/\boldsymbol{\sigma}$ | $\beta/\boldsymbol{\sigma}$ |
|---|---|---|
| BC1 4-color, 균등 $\{0,\tfrac13,\tfrac23,1\}$ | $\tfrac49 \approx 0.4444$ | $0$ |
| BC1 3-color mode $\{0,\tfrac12,1\}$ | $\tfrac13 \approx 0.3333$ | $0$ |
| BC7 2-bit index $\{0,21,43,64\}/64$ | $\approx 0.4409$ | $0$ |
| BC7 3-bit index $\{0,9,\dots,64\}/64$ | $\approx 0.5669$ | $0$ |
| 비균등 대칭 분포 $(\tfrac25,\tfrac1{10},\tfrac1{10},\tfrac25)$ | $\tfrac{8}{45} \approx 0.1778$ | $0$ |
| 비균등 **비대칭** 분포 $(\tfrac12,\tfrac3{10},\tfrac1{10},\tfrac1{10})$ | $\tfrac29 \approx 0.2222$ | $\tfrac12$ |

즉 (i) BC1 3-color + transparent mode(패킹된 RGB565 워드가 $\texttt{color}_0 \le \texttt{color}_1$ 인 경우), (ii) BC7 등 다른 인덱스 정밀도, (iii) 셀렉터 분포가 균등에서 크게 벗어나는 블록에서는 계수를 재계산해야 하며, 특히 **비대칭 분포에서는 $\beta \neq 0$ 이 되어 두 엔드포인트를 서로 다르게 이동시켜야 한다**. 문서 제목의 "BC-family" 일반성을 유지하려면 계수를 상수가 아니라 측도 $m_1, m_2, m_3$ 의 함수
$$\alpha = 4\boldsymbol{\sigma}\,\frac{m_2(m_1-m_2) - m_1(m_2-m_3)}{m_2 - m_1^2},\qquad \beta = 4\boldsymbol{\sigma}\,\frac{(m_2-m_3) - m_1(m_1-m_2)}{m_2 - m_1^2}$$
로 두는 것이 정확하다. 실측 셀렉터 분포를 쓸 경우 $m_j$ 는 이미 Theorem 3의 모멘트 집계 과정에서 얻어지므로 추가 비용이 거의 없다.

#### 자기일관성에 관한 주석
$\boldsymbol{\sigma}$ 는 보정 **이전**의 $\mathbf{p}_j$ 로 계산되지만, 엔드포인트를 이동시키면 실제 처짐량도 함께 변한다. 따라서 위 보정은 고정점 해가 아닌 **1차 근사**이며, 필요하다면 $\boldsymbol{\sigma}$ 재계산 1회로 개선할 수 있다. 또한 $\mathbf{p}_j + \tfrac49\boldsymbol{\sigma}$ 가 표현 가능 범위 $[0,1]$ 을 벗어나면 clamp가 발생하여 Jensen 논증과 무관한 오차가 추가된다(특히 $\mathbf{p}_1$ 이 포화에 가까운 밝은 블록).

---

## 5. Exactness of Selectors and Higher Levels

### Theorem 7 (셀렉터 매핑의 Exactness)
최종 sRGB 엔드포인트 $\mathbf{q}_j$(RGB565 양자화 및 8비트 확장을 마친 값)가 정해지면, 자식 팔레트의 리니어 값 $\mathbf{u}^c_k = L\big(\mathbf{q}_0 + t_k(\mathbf{q}_1 - \mathbf{q}_0)\big)$ 는 LUT 12회 호출(4 팔레트 × 3 채널)로 정확하게 구해진다. 따라서 셀렉터 결정은 근사 없이 완벽한 유클리디안 거리 최소화로 이루어진다.
$$s_g = \arg\min_k \|\mathbf{y}_g - \mathbf{u}^c_k\|^2$$

> 여기서의 "Exact"는 **주어진 $\mathbf{q}_j$ 에 대해 셀렉터 할당이 전역 최적**이라는 의미이다. 엔드포인트와 셀렉터를 동시에 최적화하는 결합 문제의 전역해를 보장하지는 않는다(그 경우 RD-optimal 탐색이 필요). 또한 $\mathbf{u}^c_k$ 는 반드시 하드웨어의 정수 보간 규칙과 동일하게 계산되어야 하며, LUT 인덱스는 8비트 sRGB 코드값이어야 한다.

### Theorem S (레벨 $\ge 2$ 모멘트의 Exactness)
레벨 2 텍셀은 레벨 0의 4×4 영역(부모 블록 전체)을 정확히 덮는다. 총 셀렉터 카운트를 $N^{\text{tot}}_{b,k} = \sum_{i}[s_{b,i} = k]$라 하면:
$$\boldsymbol{\mu}_b = \frac{1}{16}\sum_k N^{\text{tot}}_{b,k}\mathbf{u}_{b,k} = \mathbf{U}_b\Big(\frac{1}{16}N^{\text{tot}}_b\Big)$$
정리 1과 동일한 논증에 의해 **다운샘플 값의 계산은 Exact** 이다.

> **⚠ 주장의 범위:** 이것이 "레벨 $\ge 2$ 에는 근사가 전혀 없다"는 뜻은 아니다. 레벨 2 블록 역시 최종적으로 sRGB 엔드포인트를 정해야 하므로 Theorem 6의 현-곡선 갭 근사가 **레벨마다 다시 발생**한다. 본 정리가 보장하는 것은 다음 두 가지다.
> 1. **입력 측 무결성:** 각 레벨의 통계량($\boldsymbol{\mu}, \mathbf{M}_2$)이 레벨 0의 원본 압축 데이터로부터 직접 산출되므로, 상위 레벨을 하위 레벨의 재인코딩 결과로부터 만드는 방식과 달리 **양자화 오차와 감마-리니어 왕복 오차가 레벨을 따라 누적되지 않는다.**
> 2. **왕복 변환 횟수:** 레벨당 감마-리니어 왕복은 정확히 1회로 고정된다.
>
> 즉 오차는 **레벨마다 $O(1)$ 로 유지되며 $O(\text{level})$ 로 누적되지 않는다**는 것이 정확한 주장이다.

---

## 6. Assumptions & Scope (이상화 모델과 실제 구현의 간극)

본 증명의 "Exact"는 다음 이상화 위에서 성립한다. 구현 시 아래 항목은 별도의 오차원으로 취급해야 한다.

1. **엔드포인트 양자화:** $\mathbf{q}_j = S(\mathbf{p}_j + \tfrac49\boldsymbol{\sigma})$ 는 실수값이지만 BC1은 RGB565로 저장한다. 양자화 후에는 $\boldsymbol{\sigma}$ 와 셀렉터를 재계산해야 하며, 양자화 격자(R/B 5비트 ≈ 1/31, G 6비트 ≈ 1/63)가 $\tfrac49\boldsymbol{\sigma}$ 보정량보다 큰 저대비 블록에서는 보정이 양자화에 흡수되어 무효화될 수 있다.
2. **하드웨어 보간 반올림:** 팔레트 $t=\tfrac13, \tfrac23$ 의 정수 보간 반올림 규칙은 구현 정의(implementation-defined)이며 역사적으로 벤더 간 차이가 있었다. Theorem 1·7의 Exactness는 실수 산술 모델 기준이므로, 비트 단위 일치를 요구한다면 대상 디코더의 반올림 규칙을 명시해야 한다.
3. **sRGB EOTF의 볼록성:** $L$ 은 $u \le 0.04045$ 에서 $u/12.92$, 그 외에서 $((u+0.055)/1.055)^{2.4}$ 이다. 각 구간은 볼록이며 접합점에서 기울기가 $0.077399 \to 0.078721$ 로 **증가**하므로 $[0,1]$ 전체에서 볼록이다. 따라서 Theorem 6의 Jensen 논증과 $\boldsymbol{\sigma} \ge \mathbf{0}$ 은 유효하다.
4. **저휘도 선형 구간:** 팔레트 전체가 $u \le 0.04045$ 안에 놓이는 매우 어두운 블록에서는 $L$ 이 아핀이므로 현-곡선 갭이 0이 되고 $\boldsymbol{\sigma} \approx 0$ 이 된다. Theorem 4의 Lemma 역시 이 구간에서는 적용되지 않는다.
5. **BC1 모드 분기:** 위 전개는 4-color mode(패킹된 RGB565 워드가 $\texttt{color}_0 > \texttt{color}_1$)를 전제한다. 3-color + transparent mode에서는 팔레트가 3레벨이 되어 $\mathbf{U}_b \in \mathbb{R}^{3\times3}$, 보정 계수도 $\tfrac13\boldsymbol{\sigma}$ 로 바뀐다.

---

## 7. Numerical Verification (검증 요약)

| 항목 | 방법 | 결과 |
|---|---|---|
| Theorem 1 항등식 | 랜덤 블록 2,000세트 | 부동소수 오차 내 완전 일치 |
| Theorem 3 모멘트 폐쇄 | 동일 | $\boldsymbol{\mu}, \mathbf{M}_2$ 일치 |
| Theorem 4 ANOVA | 동일 | $\boldsymbol{\Sigma} = $ Between $+$ Within 일치 |
| $m_1,m_2,m_3 = \tfrac12,\tfrac7{18},\tfrac13$ | 기호계산 | 일치 |
| $\alpha = \tfrac49\boldsymbol{\sigma},\ \beta = 0$ | 정규방정식 및 직접 $L^2$ 사영 | 양쪽 모두 일치 |
| 연속 측도 대조값 $\tfrac23$ | $\int_0^1 4t(1-t)dt$ | 일치 |
| sRGB EOTF 볼록성 | 접합점 기울기 및 구간별 2계도함수 | 볼록 확인 |
| $\tfrac49$ 의 실효성 | 랜덤 엔드포인트 4,000쌍에 대해 수치 최적 $\alpha$ 탐색 | 최적 $\alpha$ 중앙값 **0.440** ($\tfrac49 = 0.4444$), 무보정 대비 RMSE **0.66배**, 악화 사례 0% |

Theorem 6의 2차 처짐 모델은 근사임에도 수치 최적점과 사실상 일치하며, 이는 보정식의 실효성을 뒷받침한다.
