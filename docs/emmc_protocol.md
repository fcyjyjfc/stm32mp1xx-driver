# eMMC 协议笔记 (JESD84-B51 v5.1)

参考: JEDEC JESD84-B51 《Embedded Multi-Media Card (e•MMC) Electrical Standard (5.1)》

## 操作模式

五种模式:

| 模式 | 说明 |
|------|------|
| Boot mode | 上电后 / CMD0(0xF0F0F0F0) / 硬件复位后进入 |
| Device identification mode | Boot 完成后或双方不支持 Boot，CMD3 收到后退出 |
| Interrupt mode | 主机和卡同时进入/退出，无数据传输 |
| Data transfer mode | RCA 分配后进入 |
| Inactive mode | 电压/访问模式不匹配，或 CMD15 发送后 |

## CMD 线模式

| 状态 | 模式 |
|------|------|
| Idle / Ready (识别阶段) | Open-drain |
| Stand-by 及之后 | Push-pull |

---

## 分区结构

- Boot Area Partition 1 (≥128KB)
- Boot Area Partition 2 (≥128KB)
- RPMB Partition (≥128KB 的整数倍)
- User Data Area (默认访问对象)
- General Purpose Area Partition 1~4 (可选, 一次可编程)
- Enhanced User Data Area (User Data 内的一段, 可配起始位置和大小)

分区配置通过 CMD6 SWITCH 写 EXT_CSD 对应字段, PERM_WRITE 保护。

---

## 6.4 设备识别模式

所有通信仅用 CMD 线。

### 6.4.1 设备复位

- CMD0 arg=0x00000000 → Idle State
- **默认 RCA = 0x0001**
- 默认驱动阶段 = 最低档 (DSR 寄存器)
- 时钟 = f_OD (识别时钟, ≤400kHz)
- CMD0 在除 Inactive 外的所有状态有效
- CMD0 arg=0xFFFFFFFA: Pre-boot 状态是启动命令, 其他状态视为复位

### 6.4.2 访问模式验证 (2GB 以上)

eMMC 设备**不再使用 CMD1 的电压范围字段**。设备固定返回:

| 状态 | ≤2GB | >2GB |
|------|------|------|
| Busy | 0x00FF8080 | 0x40FF8080 |
| Ready | 0x80FF8080 | 0xC0FF8080 |

- 设备不会因电压不匹配进入 Inactive (与 SD 不同)
- HCS bit (bit30) 指示扇区寻址
- 主机应通过 EXT_CSD SEC_COUNT 重新确认寻址模式

### 6.4.3 Busy 到 Ready

- 主机必须重复 CMD1 直到 busy 位 (bit31) 清除
- 初始化期间不能改变操作电压或访问模式
- 需要修改必须 CMD0 复位重新开始
- CMD15 (GO_INACTIVE_STATE) 发送已寻址设备到 Inactive

### 6.4.4 设备识别过程

```
CMD1 → 等 Ready → CMD2 (CID) → CMD3 (RCA) → Stand-by State
```

1. CMD1: 线与广播, 获取操作条件
2. CMD2 (ALL_SEND_CID): 各卡竞争发 CID (唯一 ID), bit 位仲裁, 失败者留在 Ready State
3. CMD3 (SET_RELATIVE_ADDR): 分配 RCA, 设备从 open-drain 切换到 push-pull
4. 重复 CMD2+CMD3 直到 N_ID 个时钟内无起始位 → 所有卡已识别

---

## 6.5 中断模式 (eMMC 专用)

### 用途
卡有事件时主动通知主机, 避免主机频繁轮询。

### 流程
1. 卡在 **Stand-by State**
2. 主机发 CMD40 (GO_IRQ_STATE) → 卡进入 Wait-IRQ State
3. 卡内部事件触发 → 在 CMD 线以 **open-drain** 发中断响应 (类似 CID 竞争)
4. 主机检测到起始位 → 接收响应 → 卡回 Stand-by State

### 主机主动退出
主机发 CMD40 响应且 RCA=0x0000, 卡检测到起始位后退出。

### 与 SDIO 中断对比
SDIO: 卡在 D1 引脚发信号 (电平触发)
eMMC: 卡在 CMD 线发响应报文

---

## 6.6 数据传输模式

设备在 Stand-by State 后进入 push-pull 模式。SDMMC_CK 先从 f_OD 提升到 f_PP。

### 状态流转 (Figure 27)

```
Stand-by  →  CMD7  →  Transfer  →  读写命令  →  Sending-data/Receive-data
                                                      ↓
                                                Programming (写后忙)
                                                      ↓
                                                  Transfer
```

### 数据传输特点
- 读命令可被 CMD12 中止: CMD17/18/21/30
- 写命令必须先中止再 CMD7 取消选择: CMD24/25/26/27
- 写有缓冲 (下一块可提前发), 但 CMD26/27/写保护/擦除无缓冲
- 编程期间不接收参数设置命令 (CMD16/35/36), 不接收读命令
- CMD7 选另一个卡: 当前卡进 Disconnect State, 后台继续编程, 释放 DAT0
- CMD7 re-select 后回到 Programming State

### 6.6.1 命令集与扩展设置

设备上电后默认: eMMC 标准命令集, 单数据线 DAT0.

主机通过 CMD6 SWITCH 修改 EXT_CSD 寄存器:
- **Properties segment**: 设备能力 (只读)
- **Modes segment**: 当前工作模式 (可写)

读 EXT_CSD: CMD8 (SEND_EXT_CSD) → 返回 512 字节数据块

写 EXT_CSD: CMD6 SWITCH, 三种访问模式:

| Access Bits | 名称 | 操作 |
|-------------|------|------|
| 00 | Command Set | 切换命令集 (Cmd Set 字段) |
| 01 | Set Bits | 将 Value 中为 1 的位设到 Index 指向的字节 |
| 10 | Clear Bits | 将 Value 中为 1 的位清除 |
| **11** | **Write Byte** | **将 Value 直接写入 Index 指向的字节 (最常用)** |

- Index 字段: 0~191 有效 (192~255 → SWITCH_ERROR)
- CMD6 响应类型: **R1b** (带 D0 Busy)
- Busy 结束后主机应发 CMD13 检查状态, 确认 SWITCH 成功
- Table 6 — EXT_CSD access mode 定义了上述四种访问模式

### 6.6.2 高速模式选择

超过 26MHz 的时钟必须先切换时序模式:

| 时钟频率 | 所需时序模式 |
|---------|------------|
| 0~26 MHz | Backward Compatible Timing (上电默认) |
| 26~52 MHz | High Speed |
| 52~200 MHz | HS200 或 HS400 |

**注意:** 时序切换过程中不要用 CMD13 查询 busy 完成状态 (异步切换导致 CRC 可能异常), 即使发 CMD13 也要忽略 CRC 错误。

参考: Table 4 — Speed mode and Clock Frequency

#### 6.6.2.1 HS 模式切换

- 前提: 设备符合 v4.0 或以上 (通过 EXT_CSD_REV 判断)
- 操作: CMD6 SWITCH 写 **0x01** 到 HS_TIMING[185]
- 上电/复位后接口时序 = "Backward Compatible Timing" (Table A.222)
- 写无效值 → SWITCH_ERROR 置位, HS_TIMING 不变, HS 未启用
- HS_TIMING 有效值定义在 7.4.65

#### 6.6.2.2 HS200 模式切换 (暂不做)

HS200 仅在 VCCQ = 1.8V 或 1.2V 下有效, 总线宽度为 SDR 4bit 或 8bit。

切换步骤 (Figure 28):
1. CMD7 选中设备 + CMD42 解锁
2. 读 DEVICE_TYPE[196] 确认支持 HS200
3. 读 DRIVER_STRENGTH[197] (可选)
4. CMD6: 设置 HS_TIMING[185] = HS200 bit + Driver Strength 值 → R1b → 等 busy → CMD13 确认
5. 提频到 ≤200MHz
6. CMD21 执行 tuning (见 6.6.5)

**注意:** tuning 之前先切好总线宽度, 让 tuning 在实际工作宽度下进行。

#### 6.6.2.3 HS400 模式切换 (暂不做)

HS400 仅在 1.8V/1.2V 下有效, 仅 DDR 8bit。

常规流程 9 步 (Figure 29): 先切 HS200 → tuning → 退 HS → 设 DDR 8bit → 切 HS400

Enhanced Strobe 流程 9 步 (Figure 30): 先切 HS → 设 BUS_WIDTH=0x86 (DDR 8bit + Enhanced Strobe) → 切 HS400, 不需 tuning

### 6.6.3 功率等级选择

- 上电默认 Power Class 0 (最低电流)
- 主机读 EXT_CSD 的 PWR_CL_ff_vvv 字段了解各电压/频率/总线宽度下的电流等级
- 需要升功率: CMD6 SWITCH 写 POWER_CLASS[187]
- 有效值定义在 7.4.55 (PWR_CL_ff_vvv[203:200], [237:236], PWR_CL_DDR_ff_vvv[239:238], [253])
- 写无效值 → SWITCH_ERROR

**基本读写不需要升功率, Power Class 0 足够。**

### 6.6.4 总线测试流程

用 CMD19 + CMD14 检测总线上哪些 DAT 线功能正常。

- 第一步: CMD19 → 主机每根 DAT 线发特定数据 pattern
- 第二步: CMD14 → 卡回送反相的 pattern
- 设备只读 pattern 的前 2 bit, 其余忽略
- 主机也只检查回送数据的前 2 bit
- DAT1~7 有内部上拉, 未连接的线回读为 "1"
- DDR 模式下 CMD19/CMD14 是非法命令
- 最小数据长度: 2 字节 (可选 CRC16)

Pattern 对照:
- **1-bit 测试**: 仅 DAT0 发 "10" pattern, 回送 "01" (Table 8)
- **4-bit 测试**: DAT0/2 发 "10", DAT1/3 发 "01" (Table 9)
- **8-bit 测试**: DAT0/2/4/6 发 "10", DAT1/3/5/7 发 "01" (Table 10)

**如果硬件连接确定无误, 可跳过此步骤直接切总线宽度。**

### 6.6.5 总线采样调谐 (HS200 专用)

HS200 模式下用 CMD21 (Send Tuning Block) 读 128 字节已知 pattern (Figure 32), 主机端逐步调整采样点扫完整个 UI, 找到最佳采样位置。

- CMD21 仅 HS200 模式 + 设备解锁状态有效, 其他状态视为非法
- 4-bit 模式 tuning block = 64 字节, 8-bit = 128 字节
- 设备保证 150ms 内完成 40 次 CMD21
- CMD21 无需先发 CMD16 设置块大小
- Figure 31 — Send Tuning Command 时序

### 6.6.6 总线宽度选择

- 通过 CMD6 SWITCH 写 BUS_WIDTH[183] 字节 (Modes Segment)
- 上电/复位默认值: **0x00** (1-bit, 仅 DAT0)
- 有效值定义见 7.4.67
- 写无效值 → SWITCH_ERROR, BUS_WIDTH 不变
- **此寄存器只写 (write only)**

### 6.6.7 数据读

物理格式: 空闲 DAT 高电平 → 起始位(0) → 数据流 → CRC16 → 结束位(1)
- SDR: 每块一个 CRC16
- DDR: 每块两个 CRC16 (奇数/偶数字节分离)
- HS400: 数据同步于 DS (Data Strobe) 而非 CLK

时序参考: Figure 42, Figure 43, Figure 49

#### 6.6.7.1 块读

**CMD17 (READ_SINGLE_BLOCK):** 读一块 → 回 Transfer State

**CMD18 (READ_MULTIPLE_BLOCK):** 连续多块, 两种模式:

| 模式 | 触发方式 | 卡停止条件 |
|------|---------|-----------|
| Open-ended | CMD18 (前面没有 CMD23) | 收到 CMD12 |
| 预定义块数 | CMD23 → CMD18 | 传完指定块数, 自动回 Tran |

**关键规则:**
- CMD23 arg=0 → 接受, 但后续按 open-ended 处理 (必须 CMD12)
- 预定义块数完成后再发 CMD12 → **非法命令** (卡已不在 Sending-data State)
- 任何多块读都可随时用 CMD12 中止
- 错误条件: ADDRESS_OUT_OF_RANGE / BLOCK_LEN_ERROR / ADDRESS_MISALIGN
- 多块读中途出错 → 卡停发数据, 留在 Data State, **主机必须发 CMD12 中止**, 错误状态在 CMD12 响应中返回
- CMD16 设了非 512B 块大小用于密码操作后, 读数据前必须重新设回 512B

### 6.6.8 数据写

卡对每块数据做 CRC 校验 → CRC 失败在 DAT0 线上通知主机 → 该块丢弃不写 → 后续块全部忽略。

#### 6.6.8.1 块写

**CMD24 (WRITE_SINGLE_BLOCK):** 写一块 → CRC status → Busy (DAT0 拉低) → Programming State

**CMD25 (WRITE_MULTIPLE_BLOCK):** 三种模式:

| 模式 | 触发 | 说明 |
|------|------|------|
| **Open-ended** | CMD25, 前面无 CMD23 | 连续收块, 直到 CMD12 |
| **预定义块数** | CMD23 → CMD25 | 传完指定块数自动停, 不要发 CMD12 |
| **Reliable Write** | CMD23 arg bit31=1 | 掉电原子性保护, 每扇区新旧数据二选一 |

**与读一致的关键规则:**
- CMD23 arg=0 → open-ended
- 预定义块数完成后发 CMD12 → **非法命令** (卡已不在 rcv state)
- 写中途出错 → 卡留在 Receive State, 后续数据全忽略, 必须 CMD12 中止

**Reliable Write 细节:**
- CMD16 块大小被忽略, 强制 512B/扇区
- 必须扇区对齐
- 掉电后每扇区要么全旧要么全新, 不会出现半新半旧
- 被 HPI 中断: 已完成扇区存新数据, 剩余扇区保持旧数据
- REL_WR_SEC_C[222] 设为 1 即可, 不影响 reliable write 本身

**写后忙 (Busy) 处理:**
- 卡 CRC 校验通过 → 开始编程 → 拉低 DAT0
- 主机可发 CMD13 查 READY_FOR_DATA 状态位
- 也可 CMD7 取消选择 → 卡进 Disconnect State → DAT0 释放 → 后台继续编程
- CMD7 re-select → 卡回 Programming State → 重新拉低 DAT0

### 6.6.9 擦除

擦除单位 = Erase Group, 大小定义在 CSD (ERASE_GROUP_DEF=0) 或 EXT_CSD (ERASE_GROUP_DEF=1)。

三步骤:
1. CMD35 (ERASE_GROUP_START) — 起始地址
2. CMD36 (ERASE_GROUP_END) — 结束地址
3. CMD38 (ERASE) — 执行, arg=0x00000000

CMD38 支持的参数 (Table 11, Table 12):

| CMD38 arg | 功能 |
|-----------|------|
| 0x00000000 | 标准 Erase |
| 0x00000001 | Trim (按 write block 而非 erase group) |
| 0x00000003 | Discard |
| 0x80000000 | Secure Erase (bit31=1, 安全清除+所有副本) |
| 0x80000001 | Secure Trim Step 1 (标记 write block) |
| 0x80008000 | Secure Trim Step 2 (清除已标记块+副本) |

**关键规则:**
- CMD35/36/38 必须严格按顺序发, 中间插非擦除命令 → ERASE_RESET
- CMD35/36/38 顺序错乱 → ERASE_SEQ_ERROR
- 地址超出范围 → ADDRESS_OUT_OF_RANGE
- 擦除范围含写保护块 → 保护块跳过, WP_ERASE_SKIP 置位
- 擦除期间 DAT0 拉低 busy, 与写一致
- CMD0/CMD15/硬件复位/掉电 会中止擦除, 数据状态不确定

### 6.6.10 TRIM

按 **write block** 粒度擦除 (Erase 按 erase group)。适合清除单个写块而非整个 erase group。流程与 Erase 相同 (CMD35+36+38), 但 CMD38 arg bit0=1。

### 6.6.11 Sanitize

写 EXT_CSD[165] SANITIZE_START 触发, 从 unmapped 地址空间物理清除数据。执行期间 DAT0 拉低 busy。被 HPI/掉电/CMD0/硬件复位中断后需重新发起。

### 6.6.12 Discard

类似 Trim, 但**不保证读回的数据是 0 或 1** (可能返回部分或全部原始数据)。用于告诉设备某些数据不再需要, 设备在后台垃圾回收时处理。CMD38 arg bit0=1 bit1=1。

与 Trim 的区别: Trim 保证读回全 0 或全 1, Discard 不保证。

### 6.6.13 Secure Erase (已废弃)

与 Erase 类似, CMD38 arg bit31=1。要求设备安全清除 erase group 及所有副本。**v4.51+ 已废弃, 改用 Erase + Sanitize。** (Table 12)

### 6.6.14 Secure Trim (已废弃)

按 write block 安全清除。两步操作: Step 1 标记 write block (CMD35+36+38 arg bit31=1 bit0=1), 可重复多次; Step 2 执行清除 (CMD35+36+38 arg bit31=1 bit15=1)。**同样已废弃。**

### 6.6.15 Write protect management (写保护管理)

写保护组 (WP-Group) 管理, 相关命令: CMD28~31 (Table 53, Class 6)。

#### CLASS_6_CTRL 模式选择 (EXT_CSD[59])

CMD28~31 的行为由 EXT_CSD[59] CLASS_6_CTRL 控制:

| CLASS_6_CTRL | CMD28 | CMD29 | CMD30 | CMD31 |
|-------------|-------|-------|-------|-------|
| **0x00** (默认) | SET_WRITE_PROT | CLR_WRITE_PROT | SEND_WRITE_PROT | SEND_WRITE_PROT_TYPE |
| **0x01** | 释放 WP-Group | 忽略 (NOP) | 返回释放状态 | 返回 64-bit 全零 |

默认值 0x00 为标准写保护模式。0x01 用于动态容量管理 (释放 WP 组), 使用前需先写 CLASS_6_CTRL, 使用后恢复为 0x00。

#### CMD28/29 — SET/CLR write protect (Table 53)

- **CMD28**: `ac [31:0] data_address → R1b SET_WRITE_PROT`
  设置指定 WP 组的写保护位。写保护组大小由 CSD (WP_GRP_SIZE) 或 EXT_CSD (HC_WP_GRP_SIZE) 决定。
- **CMD29**: `ac [31:0] data_address → R1b CLR_WRITE_PROT`
  清除指定 WP 组的写保护位。仅对临时/上电写保护有效, 对永久保护无效。

两者均为 R1b 响应, 等 DAT0 Busy 释放后才完成。

#### CMD30 — SEND_WRITE_PROT (Table 53 Note 2)

- **CMD30**: `adtc [31:0] data_address → R1 SEND_WRITE_PROT`
- **数据格式**: 32 bits (4 字节) WP 状态 + 16 bits CRC
  - 每个 bit 代表一个 WP 组的状态: 0=未保护, 1=已保护
  - 覆盖 32 个 WP 组, bit 0 (最低位) = 包含 data_address 的组
  - data_address 超出范围的组 → 对应 bit 为 0
- **数据长度**: 4 字节 (SDMMC_BLKSIZ_4B)

#### CMD31 — SEND_WRITE_PROT_TYPE (Table 53 Note 4)

- **CMD31**: `adtc [31:0] data_address → R1 SEND_WRITE_PROT_TYPE`
- **数据格式**: 64 bits (8 字节) WP 类型 + 16 bits CRC
  - 每 2 bits 表示一个 WP 组的保护类型 (32 组 × 2 bits):
    - `00` = 未保护
    - `01` = 临时写保护 (TMP_WRITE_PROTECT)
    - `10` = 上电写保护 (上电后清除)
    - `11` = 永久写保护 (PERM_WRITE_PROTECT, 不可清除)
  - 最低 2 bits = 包含 data_address 的组
- **数据长度**: 8 字节 (SDMMC_BLKSIZ_8B)
- **注意**: 实测部分 eMMC 芯片 CMD30/31 数据回读功能未实现 (返回全零), 但 CMD28/29 写保护功能正常。

#### 写保护组大小

WP-Group 大小计算 (HC 设备, ERASE_GROUP_DEF=1):

```
WP_GRP_SIZE (blocks) = HC_ERASE_GRP_SIZE[224] × HC_WP_GRP_SIZE[221] × 1024
```

### 6.6.16 Extended Security Protocols Pass Through Commands

安全协议透传, 通过 CMD56 和 CMD57 实现扩展安全功能。

### 6.6.17 Production State Awareness (PSA)

生产状态感知: 设备在产线态 vs 正常态。产线态下 INIT_TIMEOUT 缩短 (见 EXT_CSD[241] INI_TIMEOUT_AP), 方便产线快速启动。

### 6.6.18 Field Firmware Update (FFU)

现场固件升级: CMD6 进入 FFU 模式 → CMD62 传数据 → 等 busy → CMD0 复位或发 CMD62+参数确认。

### 6.6.19 Device lock/unlock operation

CMD42 + 密码数据块 (需先用 CMD16 设块大小=1 byte):
- **Lock**: 锁定设备, 上电后可设自动锁定
- **Unlock**: 密码解锁
- **Force Erase**: 强制擦除 (忘记密码时用), 擦除用户数据区, 清除密码, 解除锁定

### 6.6.20 Application-specific commands

两类厂商扩展命令:

#### APP_CMD (CMD55)

**与 SD 的 CMD55 完全一样**: 收到 CMD55 后, 设备把紧跟的下一条命令解释为 ACMD (Application-Specific Command)。设备状态中 APP_CMD 位 (bit 5) 指示是否已进入 ACMD 等待状态。ACMD 编号由厂商定义, 无限制。如果 CMD55 后面发了一条没有 ACMD 定义的命令, 设备按标准命令处理。

#### GEN_CMD (CMD56)

通用命令/厂商专用数据通道:
- 数据传输格式与单块读写相同 (CMD17/24), 但**参数不是地址而是传输方向** (bit0=0 写方向, bit0=1 读方向)
- 数据块不是存储数据, 而是**厂商自定义格式和含义**
- 必须在 Transfer State 下发送, 响应 R1
- SDR 模式块大小用 CMD16 定义, DDR 模式固定 512B

### 6.6.21 Sleep (CMD5)

设备在 Stand-by ↔ Sleep 两状态间切换:

| 操作 | CMD5 arg bit15 | 说明 |
|------|---------------|------|
| SLEEP | 1 | Standby → Sleep, DAT0 拉低 busy, 释放后进入 Sleep |
| AWAKE | 0 | Sleep → Standby, DAT0 拉低 busy, 释放后进入 Standby |

- Sleep 状态下只响应 CMD0(复位) 和 CMD5, 其他命令全忽略
- **Sleep 状态下可关闭 VCC 电源**, 实现最低功耗
- AWAKE 之前必须先恢复 VCC 到最低工作电压
- 状态切换超时: EXT_CSD[216] S_A_TIMEOUT
- Sleep 状态功耗: EXT_CSD[220] S_C_VCC, [219] S_C_VCCQ

### 6.6.22 RPMB (Replay Protected Memory Block)

14 页, 安全认证存储区域。通过共享密钥 + HMAC-SHA256 做读写认证, 有写计数器防重放攻击。仅 Class0/2/4 命令可访问。**暂不实现。**

### 6.6.23~6.6.24 DDR 模式选择与操作

双沿采样 (DDR), 同频率下带宽翻倍。只在 HS/HS200 基础上启用。

**切换顺序 (6.6.23):** HS_TIMING=0x1 → BUS_WIDTH=5/6 (4-bit/8-bit DDR), 主机 DDR 模式。
**DDR 模式限制:** CMD16/CMD42/CMD19/CMD14/CMD11/CMD20 非法, 块大小固定 512B。
**已实现: DDR52 (8-bit DDR, 52MHz, 104 MB/s)。**

### 6.6.25~6.6.40 其余高级特性

| 节 | 内容 | 用途 |
|----|------|------|
| 6.6.25 | Background Operations | 后台垃圾回收/磨损均衡, EXT_CSD[163] BKOPS_EN |
| 6.6.26 | HPI (High Priority Interrupt) | CMD12/CMD13 中断当前操作, 降低延迟 |
| 6.6.27 | Context Management | 多上下文并存, 快速切换 |
| 6.6.28 | Data Tag | 数据和标签配对传输 |
| 6.6.29 | Packed Commands | 多条写命令打包发送 (减少 overhead) |
| 6.6.30 | Exception Events | 设备主动报告异常事件 (温升/寿命到等) |
| 6.6.31 | Cache | EXT_CSD[33] CACHE_CTRL, 缓存写数据, 需 FLUSH_CACHE[32] |
| 6.6.32 | Features cross matrix | 各特性与速度模式/总线宽度的兼容表 |
| 6.6.33 | Dynamic Capacity Management | 动态容量管理 |
| 6.6.34 | Large sector size | 大扇区 (4096B) 模式, EXT_CSD[61-63] |
| 6.6.36 | Power Off Notification | 掉电通知, EXT_CSD[34] |
| 6.6.39 | Command Queuing | 命令队列 (CQE), 15 页, eMMC 5.0+ |
| 6.6.40 | Secure Write Protect | 安全写保护模式 |

**以上全部暂不实现, 仅做功能索引。**

---

## 6.7 时钟控制

(待提取)

## 6.8 错误条件

(待提取)

## 6.9 最低性能

定义 Speed Class 和性能测量方法, 适用于卡本身性能等级, 不影响驱动实现。

## 6.10 命令

### 6.10.1 命令类型

四种命令类型:

| 缩写 | 全称 | 说明 |
|------|------|------|
| **bc** | broadcast | 广播, 无响应 |
| **bcr** | broadcast with response | 广播, 带响应 |
| **ac** | addressed (point-to-point) | 寻址命令, DAT 线上无数据传输 |
| **adtc** | addressed data transfer | 寻址数据传输命令, DAT 线上有数据 |

### 6.10.2 命令格式 (Table 47)

所有命令 48-bit 固定长度, 52MHz 下传输时间 0.92μs:

| 位 | 47 | 46 | [45:40] | [39:8] | [7:1] | 0 |
|----|----|----|---------|--------|-------|---|
| 内容 | Start=0 | Direction=1 (Host) | Command Index | Argument (32-bit) | CRC7 | End=1 |
| 宽度 | 1 | 1 | 6 | 32 | 7 | 1 |

### 6.10.3 命令类 (Table 48)

12 个命令类 (CCC), Class 0 是强制必须支持的:

| Class | 名称 | 涉及命令 |
|-------|------|---------|
| 0 | Basic | CMD0,1,2,3,4,5,6,7,8,9,10,12,13,14,15,19 |
| 1 | Obsolete | CMD11 |
| 2 | Block Read | CMD16,17,18,21,23 |
| 3 | Obsolete | CMD20 |
| 4 | Block Write | CMD16,23,24,25,26,27,49 |
| 5 | Erase | CMD35,36,38 |
| 6 | Write Protection | CMD28,29,30,31 |
| 7 | Lock Device | CMD42 |
| 8 | Application-specific | CMD55,56 |
| 9 | I/O Mode | CMD39,40 |
| 10 | Security Protocols | CMD53,54 |
| 11 | Command Queuing | CMD44,45,46,47,48 |
| 12 | Reserved | — |

### 6.10.4 命令详细描述

#### Table 49 — Basic commands (Class 0/1)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD0 | bc | 0x00000000 | — | GO_IDLE_STATE | 复位到 Idle |
| | bc | 0xF0F0F0F0 | — | GO_PRE_IDLE_STATE | 复位到 Pre-Idle |
| | bc | 0xFFFFFFFA | — | BOOT_INITIATION | 启动 Alternative Boot |
| **CMD1** | **bcr** | **OCR (无 busy)** | **R3** | **SEND_OP_COND** | **查询操作条件, R3 无 CRC** |
| CMD2 | bcr | stuff bits | R2 | ALL_SEND_CID | 所有卡发 CID, 线与仲裁 |
| CMD3 | ac | [31:16] RCA | R1 | SET_RELATIVE_ADDR | 分配 RCA |
| CMD4 | bc | [31:16] DSR | — | SET_DSR | 编程 DSR |
| **CMD5** | **ac** | **[31:16] RCA, [15] Sleep/Awake** | **R1b** | **SLEEP_AWAKE** | **Stand-by ↔ Sleep** |
| **CMD6** | **ac** | **见下方详细格式** | **R1b** | **SWITCH** | **切换模式 / 写 EXT_CSD** |
| CMD7 | ac | [31:16] RCA | R1/R1b | SELECT/DESELECT_CARD | 选择/取消选择设备, RCA=0 取消选择 |
| **CMD8** | **adtc** | **stuff bits** | **R1** | **SEND_EXT_CSD** | **读 512 字节 EXT_CSD 数据块** |
| CMD9 | ac | [31:16] RCA | R2 | SEND_CSD | 读 CSD 寄存器 |
| CMD10 | ac | [31:16] RCA | R2 | SEND_CID | 读 CID 寄存器 |
| CMD11 | ac | — | — | obsolete | 已废弃 |
| CMD12 | ac | [31:16] RCA, [0] HPI | R1 | STOP_TRANSMISSION | 中止数据传输; HPI=1 中断内部操作 |
| CMD13 | ac | [31:16] RCA, [15] SQS, [0] HPI | R1 | SEND_STATUS | SQS=0 查状态, SQS=1 查队列状态(QSR) |
| CMD14 | adtc | stuff bits | R1 | BUSTEST_R | 总线测试回读 |
| CMD15 | ac | [31:16] RCA | — | GO_INACTIVE_STATE | 进入 Inactive |
| CMD19 | adtc | stuff bits | R1 | BUSTEST_W | 总线测试发送 |

**CMD6 SWITCH 参数格式 (Table 49):**

| 位 | [31:26] | [25:24] | [23:16] | [15:8] | [7:3] | [2:0] |
|----|---------|---------|---------|--------|-------|-------|
| 内容 | 0 | Access | Index | Value | 0 | Cmd Set |
| 说明 | 保留 | 00=CmdSet, 01=SetBits, 10=ClearBits, **11=WriteByte** | EXT_CSD 字节索引 (0~191) | 写入值 | 保留 | 命令集选择 |

- Index > 191 → SWITCH_ERROR
- 响应类型: **R1b** (带 DAT0 Busy)
- 常用: Access=3 (WriteByte), Cmd Set=0
  - BUS_WIDTH=2 (8-bit): `(3<<24) | (183<<16) | (2<<8)` = **0x03B70200**
  - HS_TIMING=1: `(3<<24) | (185<<16) | (1<<8)` = **0x03B90100**

#### Table 50 — Block-oriented read commands (Class 2)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD16 | ac | block length | R1 | SET_BLOCKLEN | 设块大小 (默认 512B, 见 CSD) |
| CMD17 | adtc | data address | R1 | READ_SINGLE_BLOCK | 读单块 |
| CMD18 | adtc | data address | R1 | READ_MULTIPLE_BLOCK | 多块读, 直到 CMD12 或 CMD23 预定数 |
| CMD21 | adtc | stuff bits | R1 | SEND_TUNING_BLOCK | HS200 tuning (128 字节 pattern) |

注: ≤2GB 用字节地址, >2GB 用扇区地址。

#### Table 52 — Block-oriented write commands (Class 4)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| **CMD23** | **ac** | **[31] ReliableWrite, [15:0] block count** | **R1** | **SET_BLOCK_COUNT** | **预定义块数 (CMD18/25 之前发)** |
| CMD24 | adtc | data address | R1 | WRITE_BLOCK | 写单块 |
| CMD25 | adtc | data address | R1 | WRITE_MULTIPLE_BLOCK | 多块写 |
| CMD26 | adtc | stuff bits | R1 | PROGRAM_CID | 编程 CID (仅一次, 通常厂商用) |
| CMD27 | adtc | stuff bits | R1 | PROGRAM_CSD | 编程 CSD 可编程位 |
| CMD49 | adtc | stuff bits | R1 | SET_TIME | 设 RTC 时间 |

**CMD23 参数位:**
- [31]: Reliable Write Request (可靠写请求, 掉电保护)
- [30]: 0=非 packed, 1=packed
- [24]: Forced Programming (强制写穿缓存)
- [15:0]: Number of blocks

#### Table 54 — Erase commands (Class 5)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD35 | ac | data address | R1 | ERASE_GROUP_START | 擦除起始地址 |
| CMD36 | ac | data address | R1 | ERASE_GROUP_END | 擦除结束地址 |
| **CMD38** | **ac** | **见下方** | **R1b** | **ERASE** | **执行擦除/Trim/Discard** |

CMD38 参数: [31] Secure Erase, [15] Force GC, [1] Discard, [0] Trim

#### Table 55 — I/O mode commands (Class 9)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD39 | ac | [31:16] RCA, [15] RW, [14:8] RegAddr, [7:0] Data | R4 | FAST_IO | 快速 I/O 读写寄存器 |
| CMD40 | bcr | stuff bits | R5 | GO_IRQ_STATE | 进入中断模式 |

#### Table 56 — Lock Device (Class 7)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD42 | adtc | stuff bits | R1 | LOCK_UNLOCK | 设密码/锁定/解锁, 数据块大小用 CMD16 设 |

#### Table 57 — Application-specific commands (Class 8)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD55 | ac | [31:16] RCA | R1 | APP_CMD | 下一条命令解释为 ACMD |
| CMD56 | adtc | [0] RD/WR (1=读, 0=写) | R1 | GEN_CMD | 通用/厂商数据通道, 块大小 CMD16 设 |

#### Table 58 — Security Protocols (Class 10)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD53 | adtc | Security Protocol | R1 | PROTOCOL_RD | 安全协议读, 块数 CMD23 预定, 块固定 512B |
| CMD54 | adtc | Security Protocol | R1 | PROTOCOL_WR | 安全协议写, 块数 CMD23 预定, 块固定 512B |

#### Table 59 — Command Queue (Class 11)

| CMD | Type | Arg | Resp | 缩写 | 说明 |
|-----|------|-----|------|------|------|
| CMD44 | ac | DD/Priority/TaskID/BlockCnt | R1 | QUEUED_TASK_PARAMS | 定义队列任务参数 |
| CMD45 | ac | Start block address | R1 | QUEUED_TASK_ADDRESS | 定义队列任务地址 |
| CMD46 | adtc | [20:16] Task ID | R1 | EXECUTE_READ_TASK | 执行队列读任务 |
| CMD47 | adtc | [20:16] Task ID | R1 | EXECUTE_WRITE_TASK | 执行队列写任务 |
| CMD48 | ac | [20:16] TaskID, [3:0] TM op-code | R1b | CMDQ_TASK_MGMT | 丢弃任务/清空队列 |

## 6.11 设备状态转换表 (Tables 60-62)

核心规则: 什么状态下发什么命令, 设备会转换到什么状态。

### 与驱动开发相关的关键转换:

| 当前状态 | 命令 | 新状态 | 说明 |
|----------|------|--------|------|
| Idle | CMD1 (busy) | Idle | 保持 Idle, 继续轮询 |
| Idle | CMD1 (ready) | Ready | 初始化完成 |
| Ready | CMD2 (win bus) | Ident | CID 发送完成 |
| Ready | CMD2 (lose bus) | Ready | 线与仲裁失败, 重试 |
| Ident | CMD3 | Stby | RCA 分配完成 |
| Stby | CMD7 (addressed) | Tran | 选中设备, 可读写 |
| Stby | CMD7 (not addressed) | Stby | 取消选择, 保持不变 |
| Tran | CMD17/18 | Data | 开始读数据 |
| Tran | CMD24/25 | Rcv | 开始收写数据 |
| Tran | CMD6 | Prg | SWITCH 操作, DAT0 busy |
| Data | 传输完成 | Tran | 自动返回 |
| Rcv | 传输完成 | Prg | 开始编程, DAT0 busy |
| Prg | Busy 结束 | Tran | 编程完成, 自动返回 |
| Tran | CMD5 (Sleep) | Slp | 进入睡眠 |
| Slp | CMD5 (Awake) | Stby | 唤醒, 回到 Stand-by |

### 重要注意事项

- **CMD24/25 在 Prg 状态下**: 不建议发, 设备可能接受也可能拒绝 (兼容性问题, NOTE 1/2)
- **CMD12 在 pre-defined 块数完成后**: 设备已不在 data/rcv state, 此时 CMD12 是非法命令
- **CMD13 (SEND_STATUS)**: 几乎所有状态都能发, 是最安全的状态查询手段
- **CMD0 (0x00000000)**: 除 Inactive 外所有状态都能复位到 Idle
- **CMD15**: 几乎所有状态都能进 Inactive
- Boot mode 状态未包含在表中 (NOTE 3)

## 6.12 响应 (Tables 63-67)

所有响应在 CMD 线上传输, 以 Start=0, Direction=0 (Device) 开头, 以 End=1 结尾。除 **R3 外都有 CRC7**。

### R1 (48-bit, Table 63) — 普通响应

| 位 | 47 | 46 | [45:40] | [39:8] | [7:1] | 0 |
|----|----|----|---------|--------|-------|---|
| 内容 | Start=0 | Direction=0 | Command Index (6-bit) | Device Status (32-bit) | CRC7 | End=1 |

### R1b — 同 R1 + DAT0 Busy

R1 响应后 DAT0 被拉低, 表示设备忙。用于 CMD5/CMD6/CMD7/CMD24/CMD25/CMD28/29/38 等需要等待完成的命令。

### R2 (136-bit, Table 64) — CID/CSD 寄存器

| 位 | 135 | 134 | [133:128] | [127:1] | 0 |
|----|-----|-----|-----------|---------|---|
| 内容 | Start=0 | Dir=0 | Check bits (111111) | CID/CSD 内容 (127-bit, 含内部 CRC7) | End=1 |

用于 CMD2, CMD9, CMD10。**注意**: 寄存器 bit[0] 被 End bit 替代, 实际传输 bits[127:1]。

### R3 (48-bit, Table 65) — OCR 寄存器 (无 CRC)

| 位 | 47 | 46 | [45:40] | [39:8] | [7:1] | 0 |
|----|----|----|---------|--------|-------|---|
| 内容 | Start=0 | Dir=0 | Reserved (111111) | OCR (32-bit) | Fixed (1111111) | End=1 |

**用于 CMD1。无 CRC7, [7:1] 固定 1111111。** Busy=0 (低电平) 表示忙, bit31=1 表示就绪。

### R4 (48-bit, Table 66) — Fast I/O

| 位 | 47 | 46 | [45:40] | [39:8] Argument Field | [7:1] | 0 |
|----|----|----|---------|----------------------|-------|---|
| 内容 | Start=0 | Dir=0 | CMD39 (100111) | RCA[31:16] + Status[15] + RegAddr[14:8] + Data[7:0] | CRC7 | End=1 |

用于 CMD39 FAST_IO。

### R5 (48-bit, Table 67) — 中断请求

| 位 | 47 | 46 | [45:40] | [39:8] Argument Field | [7:1] | 0 |
|----|----|----|---------|----------------------|-------|---|
| 内容 | Start=0 | Dir=0 | CMD40 (101000) | RCA[31:16] + IRQ Data[15:0] | CRC7 | End=1 |

用于 CMD40 GO_IRQ_STATE。如果是主机发的响应, RCA=0x0000。

## 6.13 设备状态 (Table 68-70)

R1 响应的 [39:8] 位是 32-bit Device Status。

### 两种位类型

| 类型 | 符号 | 说明 | 清除条件 |
|------|------|------|---------|
| **Error bit** | E | 错误条件检测 | 发送报告该错误的响应后自动清除 (Clear Cond. B) |
| **Status bit** | S | 信息字段, 不影响命令执行 | 持久性, 根据设备状态变化 (Clear Cond. A) |

### 两种检测模式

| 模式 | 符号 | 说明 |
|------|------|------|
| **Response Mode** | R | 命令解析阶段检测, 在**该命令的响应**中报告, 命令不执行 |
| **Execution Mode** | X | 命令执行阶段检测, 在**下一条命令的响应**中报告 (通常通过 CMD12 或 CMD13) |

### Table 68 — 32-bit Device Status 字段

| Bit | 标识符 | 类型 | 检测 | 说明 |
|-----|--------|------|------|------|
| 31 | ADDRESS_OUT_OF_RANGE | E | R/X | 地址越界 |
| 30 | ADDRESS_MISALIGN | E | R/X | 地址未对齐物理块 |
| 29 | BLOCK_LEN_ERROR | E | R | 块大小非法 |
| 28 | ERASE_SEQ_ERROR | E | R | 擦除序列错误 |
| 27 | ERASE_PARAM | E | X | 擦除参数无效 |
| 26 | WP_VIOLATION | E | X | 写保护冲突 |
| 25 | DEVICE_IS_LOCKED | S | R | 设备已锁定 (0=解锁, 1=锁定) |
| 24 | LOCK_UNLOCK_FAILED | E | X | 锁定/解锁失败 |
| 23 | COM_CRC_ERROR | E | R | 命令 CRC 错误 (**注意**: 设备不响应此命令, 错误在下一条正常命令的响应中报告) |
| 22 | ILLEGAL_COMMAND | E | R | 当前状态不允许的命令 (**注意**: 同 CRC 错误, 不响应, 在下一条响应中报告) |
| 21 | DEVICE_ECC_FAILED | E | X | 内部 ECC 纠正失败 |
| 20 | CC_ERROR | E | R | 设备内部错误 (非主机命令相关) |
| 19 | ERROR | E | X | 执行期间通用错误 (如读写失败) |
| 18-17 | — | — | — | Obsolete, 忽略 |
| 16 | CID/CSD_OVERWRITE | E | X | CID/CSD 非法覆写 |
| 15 | WP_ERASE_SKIP | E | X | 部分地址空间因写保护被跳过 |
| 14 | — | — | — | Reserved |
| 13 | ERASE_RESET | E | R | 擦除序列被非擦除命令打断 |
| **12:9** | **CURRENT_STATE** | **S** | **R** | **当前设备状态 (0=Idle, 1=Ready, 2=Ident, 3=Stby, 4=Tran, 5=Data, 6=Rcv, 7=Prg, 8=Dis, 9=Btst, 10=Slp)** |
| **8** | **READY_FOR_DATA** | **S** | **R** | **缓冲区就绪 (对应 DAT0 信号)** |
| **7** | **SWITCH_ERROR** | **E** | **X** | **CMD6 SWITCH 失败 (Index 越界/Value 无效)** |
| 6 | EXCEPTION_EVENT | S | R | 异常事件发生, 需查 EXCEPTION_EVENTS_STATUS |
| 5 | APP_CMD | S | R | CMD55 后置位, 下一条命令解释为 ACMD |
| 4 | — | — | — | Reserved |
| 3:2 | — | — | — | Reserved for Application Specific |
| 1:0 | — | — | — | Reserved for Manufacturer Test Mode |

### Table 69 — 命令与状态位交叉引用

每个命令可能在响应中触发哪些状态位 (R/X/S 标记), 详情见手册 Table 69。

### Table 70 — 各命令类对应的有效状态位

不同命令类的响应中哪些位有意义, 例如 Class 2 命令 (CMD16-23) 的响应中 bits 31-5 都有效。详情见手册 Table 70。

## 6.14 存储阵列分区 (Figure 35)

- 基本传输单位: **1 字节**
- **Block**: 面向读写命令的单位, 大小可编程或固定 (见 CSD)
- **Erase Group**: 最小可擦除单元, 大小在 CSD (ERASE_GROUP_DEF=0) 或 EXT_CSD (ERASE_GROUP_DEF=1)
- **WP-Group**: 最小写保护单元, 大小 = Erase Group 的倍数

## 6.15 时序

### 6.15.1 命令与响应时序

- **识别模式** (CMD1, CMD2, CMD3): open-drain, 设备响应延迟 N_ID/N_CR 个时钟 (Figure 36, 37)
- **数据传输模式**: push-pull 模式, 命令后跟 2 个 Z bit (方向切换), 然后 P bit (Figure 38)
- **R1b 响应**: DAT0 在命令结束位后 2 个时钟周期拉低 (非 HS200/HS400); HS200/HS400 下 2~4 个时钟 (Figure 39)
- 最后响应到下一命令: 至少 N_RC 个时钟 (Figure 40)
- 最后一命令到下一命令: 至少 N_CC 个时钟 (Figure 41)

### 6.15.2 数据读时序

- 单块读 (CMD17): 从读命令结束位起 N_AC 延迟后开始数据传输 (Figure 42)
- 多块读 (CMD18): 连续数据流, CMD12 结束 (Figure 43, 44)
- SDR: 单 CRC16; DDR: 两个 CRC16 (奇偶字节分离)

### 6.15.3 数据写时序

- 单块写 (CMD24): 主机在设备响应后 N_WR 开始发数据 → CRC Status Token → DAT0 Busy (Figure 45, 46)
- CRC Status Token: 010 = 成功, 101 = 失败
- 多块写: CMD12 中止, HS200/HS400 下需注意 CMD12 与 CRC Status 的时序 (Figure 60)

### 6.15.7 时序值

所有时序参数定义在 Table 71。HS200/HS400 特殊时序参数在 Table 72 (NAC≥8, NCRC=2~8)。

### 6.15.11 硬件复位时序 (Table 73)

- tRSTW (RST_n 脉冲宽度): ≥1μs
- tRSCA (RST_n 到命令时间): ≥200μs (需 74 个时钟周期后才能发 CMD1/CMD0)

---

## 7. 设备寄存器 (Chapter 7)

七种寄存器: OCR, CID, CSD, EXT_CSD, RCA, DSR, QSR。

### 7.1 OCR 寄存器 (Table 74)

32-bit, 通过 CMD1 读取 (R3 响应, 无 CRC):

| Bits | 字段 | 说明 |
|------|------|------|
| [6:0] | Reserved | 000 0000b |
| [7] | VCCQ 1.70-1.95V | Dual voltage 器件支持 |
| [14:8] | 2.0-2.6V | 000 0000b |
| [23:15] | 2.7-3.6V | 1 1111 1111b |
| [28:24] | Reserved | 0 0000b |
| [30:29] | Access Mode | 00b=byte mode, 10b=sector mode |
| [31] | Busy | 0=忙, 1=就绪 |

### 7.2 CID 寄存器 (128-bit, Table 75)

| 字段 | 位范围 | 宽度 | 说明 |
|------|--------|------|------|
| MID | [127:120] | 8 | 制造商 ID, JEDEC 分配 |
| [119:114] | Reserved | 6 | — |
| CBX | [113:112] | 2 | 00=Device, 01=BGA, 10=POP |
| OID | [111:104] | 8 | OEM/Application ID |
| PNM | [103:56] | 48 | 产品名称 (ASCII) |
| PRV | [55:48] | 8 | 产品版本 |
| PSN | [47:16] | 32 | 序列号 |
| MDT | [15:8] | 8 | 制造日期 |
| CRC | [7:1] | 7 | CID CRC7 |

### 7.3 CSD 寄存器 (128-bit, Table 78)

CMD9 (SEND_CSD) 读取, R2 长响应。CSD 提供设备访问时序、支持的命令类、容量等信息。

#### 关键字段 (Table 78-79):

| 字段 | 位范围 | 宽度 | 说明 |
|------|--------|------|------|
| CSD_STRUCTURE | [127:126] | 2 | CSD 版本, 01b=v2.0 (HC 设备) |
| TAAC | [119:112] | 8 | 读访问时间 (Table 81) |
| NSAC | [111:104] | 8 | 读访问时间因子 (时钟周期数 × 100) |
| **TRAN_SPEED** | **[103:96]** | **8** | **最大时钟频率 (Table 82)** |
| **CCC** | **[95:84]** | **12** | **命令类支持位图 (Table 83, 详见 §6.10.3)** |
| READ_BL_LEN | [83:80] | 4 | 最大读块大小 (2^READ_BL_LEN 字节) |
| READ_BL_PARTIAL | [79] | 1 | 非对齐读支持 |
| WRITE_BLK_MISALIGN | [78] | 1 | 写块不对齐支持 |
| READ_BLK_MISALIGN | [77] | 1 | 读块不对齐支持 |
| DSR_IMP | [76] | 1 | DSR 寄存器实现 |
| C_SIZE | [73:62] | 12 | 设备容量 (CSD v1.0) |
| VDD_R_CURR_MIN | [61:59] | 3 | 读电流最小值 |
| VDD_W_CURR_MIN | [55:53] | 3 | 写电流最小值 |
| ERASE_GRP_SIZE | [46:42] | 5 | 擦除组大小 (ERASE_GROUP_DEF=0 时) |
| ERASE_GRP_MULT | [41:37] | 5 | 擦除组倍数 |
| WP_GRP_SIZE | [36:32] | 5 | 写保护组大小 |
| WRITE_BL_LEN | [25:22] | 4 | 最大写块大小 |

**注意**: 对于 HC 设备 (容量 >2GB), 容量通过 EXT_CSD SEC_COUNT[215:212] 获取, CSD 中的 C_SIZE 字段不适用。

**TRAN_SPEED 编码** (Table 82): 低 3 bits = 速率单位, bits [6:3] = 倍率, 查表获得 Hz。

**CCC (Command Classes) 编码** (Table 83):

| Bit | 类 | 命令 |
|-----|-----|------|
| 0 | Class 0 (Basic) | CMD0~15, 19 |
| 1 | Class 1 (Obsolete) | CMD11 |
| 2 | Class 2 (Block Read) | CMD16~18, 21, 23 |
| 3 | Class 3 (Obsolete) | CMD20 |
| 4 | Class 4 (Block Write) | CMD16, 23~27, 49 |
| 5 | Class 5 (Erase) | CMD35~38 |
| **6** | **Class 6 (Write Protection)** | **CMD28~31** |
| 7 | Class 7 (Lock Device) | CMD42 |
| 8 | Class 8 (Application-specific) | CMD55~56 |
| 9 | Class 9 (I/O Mode) | CMD39~40 |
| 10 | Class 10 (Security Protocols) | CMD53~54 |
| 11 | Class 11 (Command Queuing) | CMD44~48 |

### 7.4 EXT_CSD 寄存器 (512 字节)

CMD8 读取, CMD6 SWITCH 修改 Modes Segment (字节 0~191)。

#### 关键寄存器字段 (仅列出基本读写相关):

| 字节 | 名称 | 大小 | 类型 | 说明 |
|------|------|------|------|------|
| **183** | **BUS_WIDTH** | 1 | **W/E_P** | **总线宽度, 上电后=0x00 (1-bit)** |
| **185** | **HS_TIMING** | 1 | **R/W/E_P** | **时序接口 + Driver Strength** |
| **192** | **EXT_CSD_REV** | 1 | R | EXT_CSD 版本 |
| **196** | **DEVICE_TYPE** | 1 | R | 设备支持的速度模式 |
| [212:215] | **SEC_COUNT** | 4 | R | 扇区数, 容量 = SEC_COUNT x 512B |
| **59** | **CLASS_6_CTRL** | 1 | R/W | Class 6 命令模式: 0x00=标准 WP, 0x01=释放组 |
| [162:161] | HPI_MGMT | 1 | R/W | HPI 管理 |

#### HS_TIMING[185] (7.4.65, Table 142-143)

| Bits | 字段 | 值 | 说明 |
|------|------|-----|------|
| [3:0] | Timing Interface | 0x0 | Backward Compatible (上电默认) |
| | | **0x1** | **High Speed (≤52MHz)** |
| | | 0x2 | HS200 (≤200MHz) |
| | | 0x3 | HS400 (≤200MHz DDR) |
| [7:4] | Driver Strength | 0x0 | 默认驱动强度 (Type 0) |

#### BUS_WIDTH[183] (7.4.67, Table 144-145)

| Bits | 字段 | 值 | 说明 |
|------|------|-----|------|
| [3:0] | Bus Mode | 0 | 1-bit (上电默认) |
| | | 1 | 4-bit |
| | | **2** | **8-bit SDR** |
| | | 5 | 4-bit DDR (需 HS_TIMING=0x1) |
| | | 6 | 8-bit DDR (需 HS_TIMING=0x1) |
| [7] | Enhanced Strobe | 0 | 仅 Data Out + CRC Response 时提供 Strobe |

#### 容量计算 (7.4.52)

```
Device density = SEC_COUNT[215:212] × 512B
容量(MB) = SEC_COUNT / 2048  (因为 1MB = 2048 扇区)
```

### 6.7 时钟控制

SDR 模式: 数据和时钟的时序关系; DDR 模式: 双沿采样。时钟频率限制取决于时序模式 (6.6.2)。

### 6.8 错误条件

- CRC 校验错误
- 非法命令
- 地址越界/未对齐
- 读操作提前读导致 OUT_OF_RANGE (可忽略)
- 具体错误位定义见 6.13 Table 68

