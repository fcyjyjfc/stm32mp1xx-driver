# RCC (RM0436 §10, pp504-623)

## §10.1 主要特性 (p504)

- 4 个 PLL：整数/小数模式、扩频、运行时改小数比
- 2 个外部振荡器：HSE 8-48MHz、LSE 32.768kHz
- 3 个内部振荡器：HSI、CSI、LSI
- 时钟输出脚供外部器件
- 2 条中断线、2 个唤醒事件
- TrustZone aware、寄存器保护

## §10.2 Figure 46 — RCC block diagram (p505)

**重点。** 完整时钟+复位拓扑结构图。三个电压域：VDD(蓝)、VSW(黄)、VDDCORE(灰)。

## §10.2.1 Table 49 — Pin overview (p506)

RCC 相关引脚功能说明。

## §10.3 复位 (pp507-509)

### §10.3.1 复位源 (p507)

PWR 产生四条复位信号给 RCC：
- por_rst: VDD < VPOR 阈值
- bor_rst: VDD < VBOR 阈值
- vcore_rst: VDDCORE 过低，或 NRST_CORE 引脚有效
- vsw_rst: VSW 过低

VDDCORE 有效后 (OKIN_MR_V33out)，RCC 触发硬件系统初始化 + 内存修复，完成后释放 vcore_rst，处理器开始运行。

### §10.3.2 应用复位与系统复位 (p508)

app_rst (应用复位) 来源：
- NRST 引脚
- por_rst、bor_rst
- IWDG1/2 超时
- MCU 的 MCSYSRST (AIRCR 寄存器，需 OTP_MCU_SYSRST_EN 允许)
- MPU 的 MPSYSRST (RCC_MP_GRSTCSETR 寄存器)
- HSE CSS 故障 (hcss_rst)

nreset (系统复位) = app_rst OR vcore_rst

NRST 引脚被以下信号激活：por_rst、bor_rst、IWDG1/2、MPSYSRST、MCSYSRST（需 OTP 允许）、外部拉低。

RPCTL: 可编程 NRST 最小脉宽。关闭时内嵌脉冲展宽器保证 ≥20μs。

**Figure 46 复位图**

### §10.3.3 局部复位 (p509)

不影响两个处理器的复位：
- cstby_rst: MPU 退出 CStandby 时复位 MPU
- MCURST: MPU 写 RCC_MP_GRSTCSETR 复位 MCU
- MPUP0RST/MPUP1RST: MPU 复位自己的 core0/core1（SCU 不复位）
- WWDG1 超时: 复位 MCU + WWDG1
- MCSYSRST: 若 OTP 不允许，作用域仅限 MCU + WWDG1，同时向 MPU 产生 EXTI 中断

### §10.3.4 CPU 复位生成 (p510)

MPU 可生成以下复位（均通过 RCC_MP_GRSTCSETR）：
- MPSYSRST: 系统复位，MCU 无法阻止
- MCURST: 复位 MCU，MCU 无法阻止。注意：复位前建议让 MCU 先进 CSleep
- MPUP0RST/MPUP1RST: 独立复位 MPU core0/core1，SCU 不复位，不影响 MCU

MCU 可生成：
- MCSYSRST (AIRCR bit2 = SYSRESETREQ): OTP 允许则系统复位，不允许则只复位 MCU+WWDG1，同时向 MPU 发 EXTI 中断

MPU 由 nreset（系统复位）或 cstby_rst（CStandby 局部复位）复位。

### §10.3.5 Watchdog 复位 (p511)

- IWDG1/2（给 MPU）→ 系统复位
- WWDG1（给 MCU）→ MCU 局部复位 + 向 MPU 发 EXTI 中断

### §10.3.6 备份域复位 nreset_vsw (p511)

来源：
- VSWRST 位（RCC_BDCR 写 1）：复位 RTC 寄存器 + RCC_BDCR（VSWRST 位除外），BKPSRAM/RETRAM 不受影响
- VSW 电压超范围：同上，但 BKPSRAM/RETRAM 内容丢失

### §10.3.7 Coresight 调试复位 dbg_rstn (p512)

三种方式：
- DAP 写 CDBGRSTREQ（调试口控制/状态寄存器）
- RCC_DBGCFGR.DBGRST 写 1
- vcore_rst（上电或退出 Standby 时）

### §10.3.8 选项字节加载 (p512)

每次 app_rst 或 vcore_rst 激活时触发（即应用复位或退出 Standby 后）。

### §10.3.9 外设复位 (p512)

通过 RCC_xxxxRSTSETR / RCC_xxxxRSTCLRR 寄存器单独复位外设（"xxxx" = 总线名）。
- 建议先开对应外设时钟再复位
- GPU 等外设复位可能需多个周期，可回读复位位确认完成
- TrustZone 使能时，APB5 外设只能安全访问复位

### §10.3.10 RPCTL 复位脉宽控制 (pp512-513)

RCC_RDLSICR.MRD[4:0] 控制 NRST 引脚最小激活时间：
- MRD=0: 旁路，内嵌脉冲展宽器保证 ~20μs
- MRD≠0: 1~31ms，以 LSI 计时。NRST 立即拉低至少 MRD 时长

**Figure 48 — Reset pulse control**（p513 时序图）

### §10.3.11 RDCTL 复位延迟控制 (pp514-515)

DDR 设备在系统复位前需先进 self-refresh 时启用。**Figure 49 — Reset delay control**（p514）。

流程：
- app_rst 发生 → RDCTL 保持 app_rst_hold → 启动 HSI
- 若在 Stop 模式：直接释放复位
- 若在 Run 模式：启动超时计数器 → 请求 DDRPHYC 让 DDR 进 self-refresh → DDR 确认后：冻结计数器、通知 PWR 设置 DDR pads retention、释放复位
- 超时计数器溢出则不等 DDR 确认，直接释放复位

配置：RCC_MP_APRSTCR
- RDCTLEN: 使能（初始化时设，复位断言时不能改）
- RSTTO[6:0]: 超时值 = round(8 × Tck_ker_dphy × 10^6)，不能设为 0，RDCTLEN=1 时不能改
- 复位后回读 RSTTO 知是否超时（=0 表示上次发生过超时）

### §10.3.12 复位覆盖总结 (pp516-517)

**Table 50 — Reset coverage summary**: 大表，行=被复位功能（VDD域、DDR接口、MPU core0/1、MCU+WWDG1、调试组件、IWDG1/2、硬件初始化、PWR寄存器、RTC等），列=复位线（por_rst～nreset_vsw 共12条），"x"=被该复位线复位。

附注：
- por_rst 断言时同时断言 vcore_rst/nreset/app_rst/bsec_rst/mcu_rst
- vcore_rst 断言时同时断言 nreset/bsec_rst/mcu_rst/dbg_rst
- app_rst 断言时同时断言 nreset/bsec_rst/mcu_rst

**Table 51 — RCC 特定复位/电压域寄存器**（p517）:
- VDD 域（app_rst 复位）：RCC_MP_BOOTCR, RCC_OCENSETR/OCENCLRR/OCRDYR, RCC_HSICFGR, RCC_CSICFGR
- VDD 域（por_rst 复位）：RCC_BR_RSTSCLRR, RCC_MC_RSTSCLRR, RCC_RDLSICR
- VSW 域（nreset_vsw 复位）：RCC_BDCR
- 其余 RCC 寄存器由 nreset 复位

### §10.3.13 复位源识别 (pp518-519)

4 个寄存器：
- RCC_BR_RSTSCLRR: BOOTROM 专用，应用不应使用
- RCC_MC_RSTSCLRR: MCU 应用读写、清标志
- RCC_MP_RSTSCLRR: MPU 应用读写、清标志
- RCC_MP_RSTSSETR: BOOTROM 专用

BOOTROM 读 RCC_BR_RSTSCLRR 确定启动流程，然后将复位源+SBF 标志转写到 RCC_MP_RSTSCLRR。复位源标志由硬件置位，只有 BOOTROM 可以写 RCC_MP_RSTSCLRR，MPU/MCU 只能读和清各自的寄存器。

**Figure 50 — Reset source registers**（p518）

**Table 52 — Reset source identification**（p519）: 15 种复位场景，每行标记对应标志位是否置 1。关键场景：
- por_rst → PORRSTF/BORRSTF/VCORERSTF/PADRSTF
- NRST pad → PADRSTF
- MCSYSRST（OTP 允许）→ MCSYSRSTF/PADRSTF
- MCSYSRST（OTP 不允许）→ 仅 PADRSTF
- MPSYSRST → MPSYSRSTF/PADRSTF
- Standby 退出 → STDBYRSTF
- CStandby 退出 → CSTDBYRSTF
- HSE CSS 故障 → HCSSRSTF/PADRSTF

### §10.3.14 上电与唤醒时序 (pp520-524)

4 种启动场景时序对比：
- 上电（Power-on）：最长，电路未供电，外部稳压器+带隙基准建立→HSI 启动→选项字节加载→BOOTROM→应用
- NRST 复位：参考电压已稳定，快。若 HSI 未使能则需重启延迟，需重载选项字节
- Standby 退出：VDD 未丢所以基准快，但 VDDCORE 丢失→需 HSI 重启+选项字节重载
- Stop 退出：VDDCORE 未丢，慢在时钟恢复。HSI/CSI 快，HSE/PLL 慢

**Figure 51 — Boot sequences versus system states**（p521，4 条时序线对比图）

**MCU HOLD_BOOT**（p521-522）：
- MPU 通过 RCC_MP_GCR.BOOT_MCU 控制 MCU 复位后是否暂停
- BOOT_MCU=0 → MCU 复位后停在 HOLD_BOOT（等效 CStop），直到 MPU 写 1
- BOOT_MCU=1 → MCU 复位后立即进 CRun
- MCU 已运行时可设 BOOT_MCU=0，不影响当前，仅影响下次复位
- 移位后 BOOT_MCU 被系统复位/上电复位/Standby 退出复位

**Figure 52 — HOLD_BOOT examples**（p522，BOOT_MCU=0 vs =1 时序图）

**从 Standby 后的处理器选择**（p522-523）：
- RCC_MP_BOOTCR.MPU_BEN/MCU_BEN（VDD 域，系统复位清零，Standby 退出不清）
- 默认：MCU 在 HOLD_BOOT，仅 MPU 运行 BOOTROM
- 若 MCU_BEN=1：Standby 退出后 BOOTROM 设 BOOT_MCU=1 把控制权给 MCU，MPU 进 CStandby。MCU 可用 SEV 指令唤醒 MPU
- 若 MPU_BEN=1：BOOTROM 继续初始化，跳转 MPU 应用

**Figure 53 — Hold function handled by the BOOTROM**（p524，BOOTROM 启动决策流程图）

## §10.4 时钟功能描述 (pp525-527)

5 个时钟源：HSI(~64MHz)、HSE(8-48MHz)、LSE(32.768kHz)、LSI(~32kHz)、CSI(~4MHz)

4 个 PLL：
- PLL1 → MPU
- PLL2 → AXI-SS (含 APB4/5, AHB5/6) + DDR + GPU
- PLL3 → MCU 总线矩阵 (APB1/2/3, AHB1/2/3/4) + 外设内核时钟
- PLL4 → 外设内核时钟

关键块：
- AXISSCD / MCUSSCD: 处理器和总线矩阵预分频
- AXISSCE / MCUSSCE / PCKE: 时钟门控（外设内核时钟、总线接口、CPU、总线矩阵）
- PKCS: 外设内核时钟的动态切换选择
- MCO1 / MCO2: 两个时钟输出脚

**Figure 54 — Top-level clock tree**（p526，**重点**。完整时钟拓扑：振荡器→DIVM→PLL→DIVP/Q/R→MUX→DIV→各 CPU/总线/外设）

### §10.4.1 时钟命名规范 (p527)

- 总线接口时钟：AHB/APB/AXI，用于寄存器读写。关闭后无法配置寄存器。部分外设只有接口时钟（如 GPIO、DMA）
- 内核时钟（kernel clock）：驱动外设本身工作（如 SAI 生成主时钟、UART 波特率）。与总线时钟解耦，改总线频率不需重配外设
- CPU 时钟：mcu_ck / mpuss_ck
- 总线矩阵时钟：各桥接器（APB/AHB/AXI）

### §10.4.2 振荡器描述 (p527)

**Table 53 — 各系统模式下的振荡器状态**:
| 模式 | HSE/HSI/CSI | LSI | LSE |
|------|------------|-----|-----|
| Run | 全部可用 | 可用 | 可用 |
| Stop/LP-Stop | 需 KERON=1 或外设内核时钟请求才能保持 | 可用 | 可用 |
| Standby/LPLV-Stop | 全部关闭 | 可用 | 可用 |
| VBAT | 全部关闭 | 关闭 | 可用 |

### HSE 振荡器 (pp528-529)

三种时钟源：
- 外部数字时钟：OSC_IN 接满摆幅数字信号，DIGBYP=1, HSEBYP=1, R1 接 GND
- 外部模拟时钟：OSC_IN 接低摆幅模拟信号，经内部 clock squarer 整形，DIGBYP=0, HSEBYP=1, R1 接 VDD，占空比 ~50%
- 晶振/陶瓷谐振器：无源晶振接 OSC_IN/OSC_OUT + 负载电容 CL1/CL2，HSEBYP=0，内部振荡电路起振，精度最高。常用。

**Figure 55 — HSE clock source**（p528，三种配置接线图）

**Figure 56 — HSE clock generation**（p528，HSEBYP/DIGBYP/HSERDY/HSE_CSS 框图）

BOOTROM 通过 R1 接 GND 还是 VDD 检测 HSE 配置方式。

HSERDY：HSEON=1 后计 512 HSE 周期就绪，就绪前 hse_ck 不释放。中断可选（RCC_MP_CIER/RCC_MC_CIER）。

Bypass 切换流程（p529）：
1. RCC_OCENCLRR 关 HSE
2. 等 HSERDY=0
3. 设 DIGBYP（数字=1/模拟=0）
4. RCC_OCENSETR 设 HSEBYP=1
5. RCC_OCENSETR 开 HSE
6. 等 HSERDY=1

晶振配置：HSEBYP=0 → HSEON=1 → 等 HSERDY=1

可在 (LP-)Stop 保持使能（§10.4.7）。

### HSI 振荡器 (p530)

默认时钟源，RC 振荡器。HSIDIV (RCC_HSICFGR) 可选 8/16/32/64 MHz，可动态改不产生毛刺，限制：
- HSI 作为 PLL 参考时钟时不能改 HSIDIV
- HSI 作为外设内核时钟时需确保频率变化不影响外设

HSIDIVRDY 标志（RCC_OCRDYR）指示新分频比是否生效。

优点：无需晶振、启动快（几 µs）、低功耗。缺点：精度不如晶振。

HSIRDY → 就绪前 hsi_ck 不释放。RCC_OCENSETR/RCC_OCENCLRR 控制开关。

**HSI 校准**（p531）：
- 工厂校准值 HSICAL[11:0] (BSEC→RCC_HSICFGR)，app_rst/vcore_rst 后自动加载，覆盖制造工艺偏差
- 用户微调 HSITRIM[6:0]（有符号），补偿运行时电压/温度漂移
- hsi_cal_ck 供 TIM12/TIM15 做运行时校准（§10.6.4），Stop 模式下关闭

**Figure 57 — HSI calibration flow**（p531）

### CSI 振荡器 (pp531-532)

低功耗 RC，~4MHz，可作 CPU/总线/PLL3/PLL4 时钟源。
- 优点：启动快（几 µs）、极低功耗。缺点：只有 4MHz，精度不如 HSI/晶振
- 控制：RCC_OCENSETR/RCC_OCENCLRR.CSION
- 可在 (LP-)Stop 保持使能，可输出到 MCO1

**CSI 校准**（p532）：
- 工厂校准 CSICAL[7:0]，用户微调 CSITRIM[4:0]（无符号），均位于 RCC_CSICFGR
- csi_cal_ck 供 TIM12/TIM15 校准，Stop 模式下关闭

**Figure 58 — CSI calibration flow**（p532）

### LSE 振荡器 (pp532-534)

两种时钟源：晶振/陶瓷谐振器、外部时钟。LSERDY 标志指示就绪。

**Figure 59 — LSE clock generation**（p533，LSEBYP/DIGBYP/LSERDY/LSEDRV/LSE_CSS 框图）

Bypass 流程：DBP=1→关 LSE→等 LSERDY=0→设 DIGBYP→LSEBYP=1→开 LSE→等 LSERDY=1→DBP=0。Bypass 下 LSEON=1 后 LSERDY 立即置 1。

晶振流程：DBP=1→关 LSE→等 LSERDY=0→LSEBYP=0→配 LSEDRV[1:0]→开 LSE→等 LSERDY=1→DBP=0

LSERDY：LSEON=1 后检测 4096 LSE 周期置 1。中断可选（RCC_MP_CIER/RCC_MC_CIER）。

LSEDRV[1:0]：可编程驱动能力，**必须在使能前配置**，使能时不能改。

以上位均在 RCC_BDCR，DBP 在 PWR_CR1。

### LSI 振荡器 (pp534-535)

~32kHz，Stop/LPLV-Stop/Standby 下可保持（供 IWDG/AWU）。LSION/LSIRDY 位于 RCC_RDLSICR。

IWDG 使能后 LSI 强制开，不能关。

### HSE 时钟安全系统 CSS (p535)

监控 HSE 是否停振。HSECSSON 写 1 使能（可 HSEON=0 时预设，HSE 就绪后硬件才真正开启）。软件不能清 HSECSSON，只能由系统复位或进 Standby 时硬件清。

检测到故障→app_rst + nreset + HCSSRSTF 标志 + TAMP 事件（保护 BKPSRAM 数据）。

故障后重选 HSE 前须先清 HCSSRSTF（BOOTROM 处理）。

**Figure 60 — LSE and HSE CSS function**（p536，CSS 故障→TAMP→擦除备份寄存器/BKPSRAM→EXTI/GIC 中断全链路）

### LSE CSS (pp536-537)

LSECSSON=1 使能，只能硬件清（nreset_vsw 或 LSE 故障后），软件不能清。

使能顺序：LSEON=1→等 LSERDY=1→RTCSRC 选 LSE→LSECSSON=1。全模式工作（Run/Stop/Standby/VBAT）。

故障后：
- 硬件立即 RTCSRC→0，断 RTC 时钟
- LSEON/LSECSSON 值不变
- rcc_lsecss_fail→TAMP（保护备份寄存器+BKPSRAM）
- LSECSSF 中断标志（RCC_MP_CIFR）
- 两个标志：LSECSSD（RCC_BDCR）、LSECSSF（RCC_MP_CIFR）

**故障恢复**：LSECSSON=0→LSEON=0→清 LSECSSF→切换 RTC 时钟源→RTCEN=1。重用 LSE CSS 前必须备份域复位（VSWRST）。

### §10.4.4 MCO 时钟输出 (p537)

MCO1/MCO2 两个输出脚，可选时钟源+预分频。
- RCC_MCO1CFGR: MCO1SEL[2:0], MCO1DIV[3:0]
- RCC_MCO2CFGR: MCO2SEL[2:0], MCO2DIV[2:0]
- 偶分频 / MCODIV=0 → 50% 占空比
- 仅在 Run/(LP-)Stop 模式可用

### §10.4.5 PLL 描述 (pp538-544)

**PLL_1600 (PLL1/2)**：VCO 800-1600MHz，输入 8-16MHz。三种模式：整数/小数（13-bit SDM，可动态更新无过冲）/扩频（降 EMI）。3 输出+后分频。

**PLL_800 (PLL3/4)**：VCO 400-800MHz，输入 4-16MHz。三种模式：整数/小数。无扩频。3 输出+后分频。

**Figure 61 — PLL1 and PLL2 block diagram**（p539）
**Figure 62 — PLL3 and PLL4 block diagram**（p539）

控制寄存器：RCC_PLLxCR, RCC_PLLxCFGR1/2, RCC_PLLxFRACR。

参考频率（通过 DIVM1-4 调整）：
- PLL1/2: 8-16MHz
- PLL3/4: 4-16MHz（需设 IFRGE 匹配参考频率）

DIVP 限制：PLL1/2 DIVP 只能 bypass(÷1)或偶数，PLL3 DIVP 只能偶数。50% 占空比建议用偶数分频。

**编程建议**（p540）：
- 使能前确认 refx_ck 在正确范围
- PLL 使能后可动态改 DIVP/Q/R（不需关 PLL）
- PLL 使能时**禁止**改 DIVM/DIVN/IFRGE（无硬件保护）
- 不用输出设 DIVyEN=0
- 关 PLL 前先关 DIVPEN/QEN/REN；PLL 就绪后才能开

**三种模式**（pp541-544）：

| 模式 | PLL1/2 | PLL3/4 |
|------|--------|--------|
| 整数 | FVCO = Fref×2×(DIVN+1) | FVCO = Fref×(DIVN+1) |
| 小数 | FVCO = Fref×2×[(DIVN+1)+FRACV/2^13] | FVCO = Fref×[(DIVN+1)+FRACV/2^13] |
| 扩频 | 三角波调制 VCO，降低 EMI 峰值 | 不支持 |

整数模式：FRACV=0, FRACLE=0→1。Fpll_y = FVCO / (DIVy+1)

小数模式：FRACV≠0, SSCG_CTRL=0。FRACV 可动态改不扰乱输出。

扩频参数（PLL1/2，RCC_PLLxCSGR）：
- MOD_PER[12:0]：调制频率，Fmod = Fref_ck / (4×MOD_PER)，通常 20-50kHz
- INC_STEP[14:0]：调制深度，MD(%) = (MOD_PER×INC_STEP×100) / ((2^15-1)×(DIVN+1)×5)
- SSCG_MODE：0=center-spread, 1=down-spread（不超标称频率）
- 约束：MOD_PER × INC_STEP ≤ 2^15-1

**Figure 63 — Spread spectrum modulation**（p543，center-spread vs down-spread）
**Figure 64 — Triangular waveform generator**（p544）

### §10.4.6 子系统时钟 (p544)

mpuss_ck / axiss_ck / mcuss_ck 三条时钟选择。

系统复位后 HSI 供全部三个。用户运行时可切换。复位后所有 PLL/CSI/HSE 均关闭。

时钟源选择：
- mpuss_ck: HSI / HSE / pll1_p_ck / pll1_p_ck÷MPUDIV → RCC_MPCKSELR.MPUSRC
- axiss_ck: HSI / HSE / pll2_p_ck → RCC_ASSCKSELR.AXISSRC
- mcuss_ck: HSI / HSE / CSI / pll3_p_ck → RCC_MSSCKSELR.MCUSSRC

**动态时钟切换**（pp545-546）：
- 图中 D 标记 = 可在线切换无毛刺
- 切换时两个输入时钟都必须存在，最坏情况输出关断 3 旧周期 + 3 新周期
- xxxSRCRDY：换源→0，切换完成→1。但不能检测当前输入是否停振（已锁定就保持 1）
- 硬件保护：目标时钟就绪（RCC_OCRDYR）后才执行切换

**Figure 65 — Dynamic clock switching**（p546）

**子系统时钟生成**（pp546-547）：
- **Figure 66 — Core and busses clock generation**（p547）

全部分频器可动态在线改：
| 分频器 | 寄存器 | 影响范围 | 最大频率 |
|--------|--------|----------|----------|
| MPUDIV | RCC_MPCKDIVR | MPU (mpuss_ck) | 650/800 MHz |
| AXIDIV | RCC_AXIDIVR | AXI + AHB5/6 + APB4/5 | 266 MHz |
| APB4DIV | RCC_APB4DIVR | APB4 (pclk4) | 133 MHz |
| APB5DIV | RCC_APB5DIVR | APB5 (pclk5) | 133 MHz |
| MCUDIV | RCC_MCUDIVR | MCU + AHB1-4 + APB1-3 | 209 MHz |
| APB1/2/3DIV | RCC_APB1/2/3DIVR | APB1/2/3 (pclk1-3) | 104.5 MHz |

X 标记 = 系统复位后默认 MUX 输入位置。

### §10.4.7 Stop/Standby 模式时钟生成 (pp548-555)

三种系统 Stop：
| 模式 | VDDCORE | 电源能力 |
|------|---------|----------|
| Stop | 不变 | 满载 |
| LP-Stop | 不变 | PMIC LP 模式，受限 |
| LPLV-Stop | 降低 | 受限 |

**进入 Stop**：MPU CStop/CStandby + MCU CStop，RCC 等 DDR self-refresh 后关所有时钟/PLL。CSI/HSI/HSE 除非 KERON=1 否则关闭。LSI/LSE 保持。

Stop 期间 HSE/HSI/CSI 可保持，两种情况：外设请求内核时钟，或 KERON 位置 1（暖机，不供时钟但立即可用）。

两条时钟路径：hse_ck/hsi_ck/csi_ck（处理器+总线）vs hse_ker_ck/hsi_ker_ck/csi_ker_ck（外设内核）。Stop 下只有内核路径激活。

**Figure 67 — Key signals controlling low-power modes**（p549）
**Table 54 — HSI/CSI/HSE states vs system modes**（p550）

**退出 Stop**：外设→EXTI→PWR→PMIC 恢复 VDDCORE→pwrwake_sys→RCC 恢复时钟→处理器唤醒。

**PWRLP_TEMPO**（p552）：等外部 PMIC LP→HP 切换的延迟（RCC_PWRLPDLYCR），用 HSI 计时。MCTMPSKP=1 可绕过延迟加速 MCU 唤醒，但 MCU 不能有 AXI-SS 外设，否则系统复位。

**Figure 68 — Clock restore activation versus system mode**（p552）

**时钟恢复机制（重点）**（pp553-555）：
- PLL1/2 → **自动恢复**，恢复期间寄存器写保护（Table 55）
- PLL3/4 → **不恢复**，退出 Stop 后被硬件复位（PLLxON=0, DIVP/Q/REN=0），软件需重新配置
- 退出 Stop 后 MCU 始终从 HSI 启动：HSION=1, MCUDIV=0, MCUSSRC=0
- MPU 唤醒：mpuss_ck + axiss_ck + mcuss_ck 同时就绪后释放
- MCU 唤醒：mcuss_ck（HSI）+ axiss_ck（仅 MCU 有 AXI-SS 外设时才开）

恢复流程：CKREST=1, AXICKRDY=0→开振荡器→HSI 就绪→PWRLP_TEMPO→PLL1/2 VCO 上电（不等 TEMPO，模拟部分不用 VDDCORE）→等 TEMPO+PLL 就绪→开 PLL 输出→CKREST=0 解保护

**Figure 69 — Clock restore process (flowchart)**（p554）
**Figure 70 — Wake up from CStop processes**（p555）

**Table 55 — 时钟恢复期间写保护的寄存器**（p555）：RCC_MPCKSELR, RCC_ASSCKSELR, RCC_MSSCKSELR, PLL1/2 全套, RCC_RCK12SELR, RCC_OCENSETR/OCENCLRR, RCC_HSICFGR, RCC_CSICFGR, RCC_PWRLPDLYCR。恢复期间写→hard fault。

**进入/退出 Standby**（p556）：MPU CStop/CStandby + MCU CStop + 双方允许。RCC 停所有时钟/PLL（除 LSI/LSE），PWR 关 VDDCORE。退出时 HSI 默认时钟，产生系统复位。RCC 寄存器复位，保留 RCC_MP_BOOTCR, RCC_BDCR, RCC_RDLSICR, RCC_BR_RSTSCLRR, RCC_MC_RSTSCLRR。

### §10.4.8 外设时钟分发 (p556)

外设两个时钟域：
- 总线接口时钟 (ckg_bus_perx)：寄存器读写
- 内核时钟：驱动外设工作

改总线频率不扰外设（如改 APB 频率不影响 UART 传输）。

Table 56 列出各外设时钟及内核时钟选择控制位（灰色=内核时钟，包含 MUX 选择位和各总线最大频率）。

Table 56 查阅指南：每个外设的记录包含：总线接口时钟源及最大频率、内核时钟源选项及 MUX 寄存器字段名和编码值、是否同步。用到具体外设时回头查此表。

#### Ethernet 时钟分发 (pp578-579, Figure 82)

**总体结构**：以太网时钟分三类——总线接口时钟（aclk/hclk6，灰色）、内核时钟 ck_ker_eth 及派生（彩色）、各使能位（MPU/MCU 各一套）。SYSCFG 块提供 ETH_SEL[2:0]（接口类型）、ETH_CLK_SEL、ETH_REF_CLK_SEL、MII_ONLY，决定时钟路径。

**内核时钟 ck_ker_eth（最大 125MHz）**：
- 源选择：ETHSRC（ETHCKSELR）= pll4_p_ck 或 pll3_q_ck（复位默认 pll4_p_ck）
- 门控：ETHCKEN / ETHCKLPEN
- 派生：ETH_CLK 输出引脚（25/50/125MHz 给外部 PHY 做参考）；clk_ptp_ref_i = ck_ker_eth / ETHPTPDIV（1~16，PTP 用）

**RCC_ETHCKSELR 位定义**（RCC §10 寄存器描述）：
- bit0:1 ETHSRC：0 = pll4_p_ck，1 = pll3_q_ck，最大 125MHz
- bit4:7 ETHPTPDIV：对 ETHSRC 选源 1~16 分频 → clk_ptp_ref_i（PTP 时钟，不影响 ETH_CLK/eth_clk_fb）

**AHB6 使能位号**（MP_AHB6ENSETR / MC_AHB6ENSETR）：
- BIT7  ETHCKEN  — 内核时钟 eth_ker_ck 使能
- BIT8  ETHTXEN  — TX 时钟路径使能（输出 GTX_CLK + clk_tx_i + clk_tx_180_i）
- BIT9  ETHRXEN  — RX 时钟路径使能（输出 clk_rx_i + clk_rx_180_i）
- BIT10 ETHMACEN — 总线接口时钟使能（hclk6 + aclk，寄存器访问+DMA）

**SYSCFG 时钟源选择（p1096 §14.2 + SYSCFG_PMCSETR/CLRR 描述）**：
- BIT16 ETH_CLK_SEL（仅 GMII/RGMII 有用）：0 = 外部 ETH_CLK125（PG5 输入，需 AFMux/AF11）；1 = 内部时钟（ETHCKEN 控制的 ck_ker_eth），无视 AFMux。写 0 无效果，清零用 PMCCLRR
- BIT17 ETH_REF_CLK_SEL（RMII 50MHz 选择）：0 = 外部 ETH_RX_CLK/ETH_REF_CLK（PA1 输入，需 AFMux），可用于所有 PHY；1 = 内部时钟 ETH_CLK1（eth_clk_fb，路径 1 输出），**只能用于 RMII PHY**。写 0 无效果，清零用 PMCCLRR
- Bits 23:21 ETH_SEL[2:0]（PHY 接口类型）：000 = GMII/MII，001 = RGMII，100 = RMII，其余保留。**必须在 ETH 处于复位期间、使能 ETH 时钟之前配置**（板级 RGMII → 001）
- PA1 双复用：AF0 = ETH_CLK（RCC 输出，RMII 下 50MHz 参考给 PHY），AF11 = RX_CLK/REF_CLK（输入）；由 AFMux/ETH_REF_CLK_SEL 选
- PG5 AF11 = CLK125（GMII/RGMII 的 125MHz 输入）
- clk_tx_180_i：TX 时钟 180° 相移版（与 clk_tx_i 反相，同频），RGMII 双沿采样内部对齐用，软件不用管

**三条时钟路径（RGMII 视角）**：
1. **内核/总线**：ETHCKEN(BIT7) → ck_ker_eth；ETHMACEN(BIT10) → hclk6/aclk。ETH_CLK/eth_clk_fb 从这里出
2. **TX**：ETH_CLK_SEL(BIT16) 选源（内部 125MHz 或 PG5 外部）→ ETH_SEL=001 → ETHTXEN(BIT8) → GTX_CLK + clk_tx_i + clk_tx_180_i
3. **RX**：ETH_REF_CLK_SEL(BIT17) 选源（外部 PA1 或内部 eth_clk_fb，内部仅 RMII）→ ETH_SEL=001 → ETHRXEN(BIT9) → clk_rx_i + clk_rx_180_i；clk_rmii_i 同路径无独立控制位，ETHTXEN/ETHRXEN 任一使能即输出（RMII 用，图标 50MHz）

**TX 时钟路径**（接口类型由 SYSCFG ETH_SEL[2:0] 决定）：
- MII：外部 PHY 从 ETH_TX_CLK 引脚输入（2.5/25MHz）；ETH_CLK 可输出 25MHz 给 PHY
- GMII：内部 RCC 或 ETH_CLK125 引脚（ETH_CLK_SEL 选），固定 125MHz；GTX_CLK 输出给 PHY
- RGMII：同 GMII 源选择，频率按 mac_speed_o[1:0] 动态 2.5/25/125MHz；GTX_CLK 输出
- RMII：内部 RCC 或 ETH_REF_CLK 引脚（ETH_REF_CLK_SEL 选），50MHz 参考，频率按 mac_speed_o[0] 动态 2.5/25MHz

**RX 时钟路径**：
- MII/GMII/RGMII：直接收 ETH_RX_CLK 引脚（MII 2.5/25MHz，GMII 125MHz，RGMII 2.5/25/125MHz）
- RMII：不用引脚，用发送参考时钟回环 eth_clk_fb，按速率分频（÷20/÷2）得到 2.5/25MHz

**mac_speed_o[1:0]**（MAC 输出给 RCC 的速率信息）：0x = 1000M，10 = 10M，11 = 100M。RGMII 用它动态切 TX 频率，RMII 用它切分频。

**使能位清单**（MPU/MCU 各一套）：ETHCKEN/ETHCKLPEN（内核时钟）、ETHTXEN/ETHTXLPEN（TX）、ETHRXEN/ETHRXLPEN（RX）、ETHMACEN/ETHMACLPEN（总线 aclk/hclk6）、ETHSTPEN（CStop/Stop 保持 PAD 时钟）、ETHPTPDIV（PTP 分频）。

**驱动要点**：
- MDIO 测试最小配置：ETHMACEN（BIT10 总线）+ ETHCKEN（BIT7 内核）+ ETHTXEN（BIT8 TX 路径）；软复位完成需要 ck_ker_eth 在场
- 使能顺序（ETH_SEL 要求）：ETH 进复位 → 配 SYSCFG ETH_SEL（RGMII=001）→ 释放复位 → 使能时钟
- MII/GMII/RGMII 的 RX 时钟来自 PHY 引脚——软复位超时的根因常是外部 PHY 时钟不在场
- 【实测】软复位完成需 TX/RX 两路接口时钟在场: RCC 门控单清 ETHRXEN/ETHTXEN 任一, SWR 卡死 (此两路是 RCC 喂给 MAC 的 eth_clk 域, 与 link 无关 —— 拔线复验 SWR 仍过, 见 TEST_STATUS 修复记录); 引脚 RXC 另论: PHY 从接收数据流恢复 (mae.md 引脚表), 无 link 有信号时仍在, 拔线无信号即停 → 不影响 SWR, 但 MAC 回环 (LM FIFO 读时钟) 无网线不可行. 详见 Driver_Test/TEST_STATUS.md 门控实验行与 LM 无 link 实验行
- 板级已定：接口 = RGMII（ETH_SEL=001），引脚 MDC=PC1/MDIO=PA2（AF11），GTX_CLK=PG4，CLK125=PG5，RX_CLK=PA1（AF11）
- 待确认：ETH_CLK_SEL 选内部还是外部 125MHz（取决于 PG5 是否接 PHY 的 CLK125 输出）；ETHSRC 选哪个 PLL（pll4_p 或 pll3_q）及其频率（RGMII 需 125MHz）

### §10.4.9 时钟概念概述 (pp601-602)

**外设分配**：双核系统，CPU 通过 PERxEN=1 将外设绑定到自己的域。
- PERxEN=1 + PERxLPEN：决定 CSleep 时外设时钟是否保持
- RCC 根据 CPU 模式自动门控外设时钟
- MPU 用 RCC_MP_xxxx 寄存器，MCU 用 RCC_MC_xxxx 寄存器

域行为：MPU 进 CStop→MPU_DOM 外设时钟自动关，但若 MCU 还在跑且其外设挂在 AXI 总线矩阵上，总线矩阵 1 保持有时钟。双方都 CStop→系统可进 Stop/Standby。

**Figure 97 — Peripheral allocation example**（p602）：MPU_DOM vs MCU_DOM 分布图。PWR/RCC 隐式分配给双方。

**内存隐式分配**：
- SYSRAM：隐式归 MPU，MCU 需 RCC_MC_AXIMENSETR 显式使能
- SRAM1-4：隐式归双方（MLAHB 桥有 CPU 跑就开）
- RETRAM：默认双方使能（确保 Standby 退出可访问）
- BKPSRAM：双方各自有使能位

### §10.4.10 外设分配 (p603)

- MPU 可为自己写 RCC_MP_xxxxENSETR，**也能** 替 MCU 写 RCC_MC_xxxxENSETR
- MCU 只能为自己写 RCC_MC_xxxxENSETR，**不能** 写 MPU 的寄存器（可读不可写）
- PERxLPEN 同理控制 CSleep 时的行为

分配时序：写 PERxEN=1 → 回读一次（dummy read，等同步）→ 外设可用

时钟使能延迟（同步电路）：开→最多 2 周期，关→最多 1.5 周期。跨子系统外设额外延迟。

总线时钟未激活时：读→无效数据，写→被忽略（不产生总线错误）。

### §10.4.11 外设时钟门控 (p604)

每个外设的时钟门控受以下信号组合控制：
- MP_PERxEN / MP_PERxLPEN（MPU 侧分配+休眠行为）
- MC_PERxEN / MC_PERxLPEN（MCU 侧）
- mpu_mode（MPU 当前状态）
- mcu_mode（mcu_sleep / mcu_deepsleep）
- req_ker_perx（外设自身内核时钟请求）

**Figure 98 — Peripheral kernel clock enable logic details**（p605）

**Table 63 — 外设时钟使能详情**（pp605-606）：内核时钟供给 4 种条件：
1. CPU 在 CRun
2. CPU 在 CSleep 且 PERxLPEN=1
3. CPU 在 CStop 且 PERxLPEN=1 且外设请求内核时钟且选了 HSI/CSI/HSE
4. CPU 在 CStop 且 PERxLPEN=1 且选了 LSE/LSI

总线接口时钟只在条件 1/2 提供。只有 I2C、U(S)ART 具备内核时钟请求能力。

### §10.4.12 处理器和总线时钟门控 (p607)

MPU (CA7-SS)：mpuss_ck 供双核+SCU。WFI/WFE 关核时钟保留唤醒逻辑。双核进 WFI→L2 进 WFI→mpu_standbywfi_l2 断言。

**Table 64 — MPU 模式详情**：双核 WFI + STPREQ=0→CSleep，双核 WFI + STPREQ=1→CStop。STPREQ=1 时 RCC 关 mpuss_ck + aclk_mpu。

STPREQ_P0/P1 位于 RCC_MP_SREQSETR/RCC_MP_SREQCLRR。

**Figure 99 — MPU low-power interface**（p608）：mpuss_ck(PLL1,650/800MHz)→MPUCD→MPUCE→CA7，axiss_ck(PLL2)→AXISSCD→aclk_mpu→AXI+GIC。SCU 的 mpu_standbywfi_l2 信号参与控制。

**总线矩阵和桥接器时钟门控**（p609）：同外设逻辑，用聚合信号（mpu_peren_axi/mpu_perlpen_axi/mcu_peren_axi/mcu_perlpen_axi）——AXI-SS 上有任一外设被分配就=1。

**Table 65 — AXIM 总线时钟使能详情**（p609）：AXIM 有钟条件：
- MPU 在 CRun 或 CSleep+AXI-SS 外设+LPEN
- MCU 在 CRun 或 CSleep+AXI-SS 外设+LPEN
裸机 MCU 始终 CRun→只要有一个 AXI-SS 外设分配就有时钟。

AXI-SS = AXIM + AHB5 + AHB6 + APB4 + APB5

同类表格（总线门控逻辑，同 Table 65 格式，pp609+）：
- Table 66 — AHB5/AHB6，Table 67 — MLAHB/AHB4，Table 68 — AHB[3:1]，Table 69 — APB[5:1]

备注：MLAHB/AHB4 只要有一方 CPU 不在 CStop 就有钟（RCC/PWR 隐式分配给双方）。AXIM 在 MPU CStop 且 MCU 无 AXI-SS 外设时可保持关闭。

### §10.4.13 低功耗仿真模式 (p614)

调试用，DBGMCU_CR 控制：
- DBGSLEEP：CSleep 仿真，时钟保持，不进低功耗
- DBGSTOP：CStop/Stop/CStandby 仿真，时钟保持，PWR 不发 LP 信号，恢复序列绕过。CStandby 下 GIC 中断线关→唤醒产 MPU 复位
- DBGSTBY：Standby 仿真，时钟保持，GIC/NVIC 中断线关，唤醒产 vcore_rst（但不复位 dbg_rstn，调试组件保留）

### §10.4.14 RCC TrustZone (p615)

RCC_TZCR.TZEN=1→安全模式，MCKPROT 额外控制 mcuss_ck 寄存器安全。
- 安全寄存器只能安全写，读无限制。非安全写→hard fault
- 保护：HSE/HSI/CSI/LSI 设置、MPU 时钟、AXIM/AHB5/APB5 时钟、安全外设时钟/复位/内核时钟
- rcc_tzen 信号供 PWR 块

## §10.5 RCC 中断 (pp616-617)

两套独立中断接口：MPU（TZEN 可设安全）、MCU（始终非安全）。每套两条线：
- 通用中断 (rcc_mpu_it/rcc_mcu_it)：PLL/振荡器就绪、LSE CSS 故障
- 唤醒中断 (rcc_mpu_wkup_it/rcc_mcu_wkup_it)：处理器退出 CStop

全为高电平、电平触发。使能寄存器：RCC_MP_CIER/RCC_MC_CIER。标志寄存器：RCC_MP_CIFR/RCC_MC_CIFR。

事件标志在对应就绪位上升沿置 1。通用中断线=所有已使能标志的 OR，需清掉全部才释放。唤醒中断线：WKUPIE=1 且 RCC 要求退出 CStop 时拉高，WKUPF 写 1 清除。

**Figure 100 — Interrupt block diagram**（p617）

**Table 70 — 中断源与控制**（p617）：

| 标志 | 描述 | 使能 | 清除 | 中断线 |
|------|------|------|------|--------|
| LSIRDYF/CSIRDYF | LSI/CSI 就绪 | xxxIE | 写 1 | rcc_it |
| LSERDYF | LSE 就绪 | LSERDYIE | 写 1 | rcc_it |
| HSERDYF/HSIRDYF | HSE/HSI 就绪 | HSxRDYIE | 写 1 | rcc_it |
| PLL1-4RDYF | PLL 就绪 | PLLxRDYIE | 写 1 | rcc_it |
| LSECSSF | LSE CSS 故障 | LSECSSIE+LSECSSON | 写 1 | rcc_it |
| WKUPF | CStop 唤醒 | WKUPIE | 写 1 | rcc_wkup_it |

## §10.6 应用信息 (pp618-623)

### §10.6.1 动态时钟切换

通用原则：切换时处理器/内存/总线/RCC 自身必须保持有时钟，不超最大频率。改总线时钟可能影响用 APB/AHB 作内核时钟的外设。

**Figure 101 — MPU clock path**（p618）：PLL12SRC→DIVM1→PLL1→MPUSRC→MPUDIV→mpuss_ck(650/800MHz max)

**Figure 102 — PLL2 clock path**（p620）：PLL12SRC→DIVM2→PLL2→AXISSRC→AXIDIV→aclk/hclk[6:5]/APB4/5DIV→pclk4/5(133MHz max)

**Figure 103 — MCU clock path**（p622）：PLL3SRC→DIVM3→PLL3→MCUSSRC→MCUDIV→mcu_ck(209MHz max)→APB1-3DIV→pclk1-3(104.5MHz max)

**改 MPU 频率**（pp618-619）：

| 方式 | 流程 |
|------|------|
| MPUSRC | 开新源→等就绪→切→等 MPUSRCRDY=1→关旧源 |
| MPUDIV | 直接改，不超最大频率，不设 000 且 MPUSRC=3 |
| PLL1 | 切到备时钟→等 MPUSRCRDY→改 PLL1→等 PLL1RDY→切回→关备时钟 |

**改 AXI 频率**（pp620-621）：

| 方式 | 流程 |
|------|------|
| AXISSRC | 检查分频不超→若超先改 AXIDIV/APB4/5DIV 等就绪→开新源→切→等 AXISSRCRDY |
| AXIDIV | 直接改，等 AXIDIVRDY/APB4/5DIVRDY |
| PLL2 DIVP | 备时钟→切 AXISSRC→DIVPEN=0→改 DIVP→DIVPEN=1→切回 |
| PLL2 VCO | 停 GPU+DDR 外设→代码搬 SYSRAM→DDR self-refresh→备时钟→改 PLL2→重初始化 DDR |

**改 MCU 频率**（pp622-623）：

| 方式 | 流程 |
|------|------|
| MCUSSRC | 检查 MCUDIV 不超→若超先改分频等就绪→开新源→切→等 MCUSSRCRDY |
| MCUDIV | 直接改，等 MCUDIVRDY/APB[3:1]DIVRDY |
| PLL3 | 备时钟(HSI/CSI/HSE)→切 MCUSSRC→等 MCUSSRCRDY→改 PLL3→等 PLL3RDY→切回 |

### §10.6.2 PLL 编程 (pp624-628)

**PLL 初始化序列**（p624）：

1. 确保 PLL 已关（DIVPEN/QEN/REN=PLLON=PLLxRDY=0）
2. 开时钟源(HSE/HSI/CSI)→等就绪
3. 选源：RCC_RCK12SELR/RCC_RCK3SELR/RCC_RCK4SELR→等切换完成
4. 配 DIVMx，使参考频率合法
5. 配置 PLL：
   - 整数/扩频：FRACLE=0→FRACV=0→FRACLE=1（SDM 加载 0）
   - 小数：同流程，FRACV=目标值
   - 设 IFRGE(PLL3/4)、SSCG_CTRL(PLL1/2)、DIVN、DIVP/Q/R
   - 扩频加设 MOD_PER、INC_STEP、SSCG_MODE
6. PLLON=1→等 PLLxRDY=1（小数模式下 FRACLE 此时不能回写 0）
7. PLLxRDY=1→开输出 DIVPEN/QEN/REN=1
8. 动态调频（仅小数）：FRACLE=0→写新 FRACV→FRACLE=1

PLLxRDY=1 = 输出在目标频率 ±2% 内。

后分频器在线改（p625）：DIVyEN=0→改 DIVy→DIVyEN=1，不关 PLL。

PLL 关闭：DIVPEN/QEN/REN=0→PLLON=0→等 PLLxRDY=0。

**SSCG 计算示例**（p625）：PLL2→533MHz, MD=0.5%, FMOD=25kHz, 24MHz 晶振→DIVM2=2, Fref=12MHz, DIVN=43, MOD_PER=120, INC_STEP=12。MOD_PER×INC_STEP=1440<2^15-1。实际 MD=0.499%, center-spread FMAX=530.6MHz。

**Table 71 — MPU 频率计划**（pp626-627）：HSE=16/19.2/24/26MHz 的 PLL1 推荐组合。优化：FCOMP 接近 16MHz、DIVN 尽量小、整数模式（降抖动）；MPU/DDRC 用 bypass、避奇数分频尤其是 3（降占空比劣化）。

**Table 72 — AXI/DDR/GPU 频率计划**（pp627-628）：HSE=16/19.2/24/26MHz 的 PLL2 组合。

### §10.6.3 子系统时钟配置实例 (pp629-631)

假设：24MHz HSE，MPU 600MHz(PLL1)，MCU 200MHz(PLL3)，AXI 264MHz(PLL2)。HSI 为当前时钟。完整寄存器编程序列：

1. **开 HSE**：HSEON=1→等 HSERDY=1
2. **参考时钟**：
   - PLL12SRC=1(hse_ck)→等 PLL12SRCRDY=1
   - DIVM1=DIVM2=1 → ref1/ref2=12MHz
   - PLL3SRC=1(hse_ck)→等 PLL3SRCRDY=1
   - DIVM3=2 → ref3=8MHz
3. **PLL1**：DIVN=49→FVCO=1200MHz→Ffout1=600MHz, DIVP=0(bypass), 整数, 关扩频
4. **PLL2**：DIVN=43→FVCO=1056MHz→Ffout2=528MHz, DIVP=1(÷2→264MHz), 整数, 开扩频 down-spread
5. **PLL3**：DIVN=49→FVCO=400MHz, DIVP=1(÷2→200MHz), IFRGE=x1, 整数
6. **总线分频**：MCUDIV=0(÷1=200MHz), AXIDIV→≤266MHz, APB4/5DIV→≤133MHz, APB1/2/3DIV→≤100MHz
7. **切换**：MPUSRC=2(pll1_p_ck), AXISSRC=2(pll2_p_ck), MCUSSRC=3(pll3_p_ck)→等全部 xxxSRCRDY=1

### §10.6.4 TIMx 时钟校准 (p631)

TIM12/TIM15 输入捕获测量时钟：hsi_cal_ck=HSI/1024(~62.5kHz), csi_cal_ck=CSI/128(~31.25kHz)。以 HSE/LSE 为基准校准 HSI/CSI，也可用 HSI 校准 LSI（无晶振时提高 LSI 精度用于 RTC/IWDG）。50MHz 定时器→HSI 精度~0.13%, CSI~0.065%。经 PLL 时可用小数分频微调总线时钟。

### §10.6.5 时钟恢复序列示例 (pp633-636)

4 个时序图展示不同唤醒场景下 PLL/振荡器/时钟的先后关系：

- **Figure 104**（p633）：MPU 唤醒，MCTMPSKP=0（等 PMIC LP→HP）。HSE/HSI→PWRLP_TEMPO→PLL→锁定→释放时钟
- **Figure 105**（p634）：MCU 唤醒，MCTMPSKP=1（不等 PMIC），无 AXI-SS 外设。axiss_ck 保持关闭直到 MCU 分配 AXI-SS 外设
- **Figure 106**（p635）：MCU 执行任务后回 CStop，CKREST=0 后系统重新进 Stop
- **Figure 107**（p636）：快速恢复——全部用 HSI（跳过 PLL 锁定），PWRLP_DLY=0，最快唤醒

