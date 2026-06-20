# MDMA 驱动使用说明

## 基本接口

```c
MdmaCfg(MDMA, &cfg);           // 配置通道 (EN=0 时调用)
MdmaEnable(MDMA, ch);          // 使能通道
MdmaDisable(MDMA, ch);         // 禁用通道
MdmaSwTrig(MDMA, ch);          // 软件触发
MdmaGetGisr(MDMA);             // 全局中断状态
MdmaGetChIsr(MDMA, ch);        // 通道中断状态
MdmaGetChEsr(MDMA, ch);        // 通道错误状态
MdmaClearChIf(MDMA, ch, flags);// 清除中断标志
```

## 快捷 Memcpy

```c
MdmaMemcpyInit(0);                      // 初始化通道 0 为 M2M 模式
MdmaMemcpy(dst, src, size);             // 非阻塞拷贝, size <= 65536
while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF));  // 等待完成
MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);
```

## 枚举速查

| 枚举 | 值 | 含义 |
|------|---|------|
| `MDMA_DATA_8BIT` | 0 | 1 字节 |
| `MDMA_DATA_16BIT` | 1 | 2 字节 |
| `MDMA_DATA_32BIT` | 2 | 4 字节 |
| `MDMA_DATA_64BIT` | 3 | 8 字节 |

上表同时用于 `ssize/dsize`（数据宽度）和 `sincos/dincos`（地址步长）。

| 枚举 | 值 | 含义 |
|------|---|------|
| `MDMA_BSIZE_1B` | 0 | 1 字节 burst |
| `MDMA_BSIZE_2B` | 1 | 2 字节 burst |
| `MDMA_BSIZE_4B` | 2 | 4 字节 burst |
| `MDMA_BSIZE_8B` | 3 | 8 字节 burst |
| `MDMA_BSIZE_16B` | 4 | 16 字节 burst |
| `MDMA_BSIZE_32B` | 5 | 32 字节 burst |
| `MDMA_BSIZE_64B` | 6 | 64 字节 burst |
| `MDMA_BSIZE_128B` | 7 | 128 字节 burst |

用户设置 burst SIZE（字节），驱动内部自动计算节拍数：

```
寄存器节拍 = 2^(sburst - ssize)
```

数学原理：`log2(burst字节) = log2(节拍) + log2(数据宽度)`，三者都是 2 的幂。

| 枚举 | 值 | 含义 |
|------|---|------|
| `MDMA_INC_FIXED` | 0 | 地址固定 |
| `MDMA_INC_INCR` | 2 | 地址递增 |
| `MDMA_INC_DECR` | 3 | 地址递减 |

| 枚举 | 值 | 含义 |
|------|---|------|
| `MDMA_TRGM_BUFFER` | 0 | 每次触发传一个 buffer |
| `MDMA_TRGM_BLOCK` | 1 | 每次触发传一个 block |
| `MDMA_TRGM_REP_BLOCK` | 2 | 每次触发传完整个 repeated block |
| `MDMA_TRGM_CHANNEL` | 3 | 每次触发传完整个通道（含链表） |

## 配置约束

| 约束 | 说明 |
|------|------|
| `sburst >= ssize` | burst size 不能小于数据宽度 |
| `sburst <= tlen+1` | burst 不能超过 buffer |
| `sburst <= 128B` | burst 最大 128 字节 |
| `sincos >= ssize` | 步长不能小于数据宽度，否则 ASE |
| `TLEN+1` 为 ssize 和 dsize 的整数倍 | buffer 对齐 |
| `BNDT` 为 ssize 和 dsize 的整数倍 | block 对齐 |

## 典型配置模式

### 线性拷贝 (M2M)

```c
MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
cfg.ch      = 0;
cfg.ssize   = MDMA_DATA_32BIT;
cfg.dsize   = MDMA_DATA_32BIT;
cfg.sinc    = MDMA_INC_INCR;
cfg.dinc    = MDMA_INC_INCR;
cfg.sincos  = MDMA_DATA_32BIT;     // 步长 = 数据宽度
cfg.dincos  = MDMA_DATA_32BIT;
cfg.sburst  = MDMA_BSIZE_64B;      // 64B burst
cfg.dburst  = MDMA_BSIZE_64B;
cfg.tlen    = 63;                   // buffer = 64 bytes
cfg.trgm    = MDMA_TRGM_BLOCK;
cfg.swrm    = 1;
cfg.ctcie   = 1;
cfg.bndt    = 4096;
cfg.src_addr = (uint32_t)src;
cfg.dst_addr = (uint32_t)dst;
```

### Block Repeat (大块拷贝)

BNDT 最大 65536，超过需用 block repeat：

```c
cfg.bndt = 65536;
cfg.brc  = 1;              // 2 blocks = 128KB
cfg.suv  = 0;              // 线性: 地址自动递增, 无额外偏移
cfg.duv  = 0;
cfg.trgm = MDMA_TRGM_REP_BLOCK;
```

### 子区域抽取 (2D DMA)

从大矩阵中提取矩形块，SUV 跳到下一行：

```c
cfg.bndt = sub_width * elem_size;           // 每行传输量
cfg.brc  = sub_height - 1;                  // 行数
cfg.suv  = (total_cols - sub_width) * elem_size;  // 跳到下一行
cfg.duv  = 0;                               // 目的连续
cfg.src_addr = (uint32_t)&matrix[row][col];
```

### Stride 抽取 (隔行/隔列)

SINCOS > SSIZE 实现跳读：

```c
cfg.ssize  = MDMA_DATA_16BIT;      // 数据 2B
cfg.sincos = MDMA_DATA_32BIT;      // 步长 4B, 跳过一个元素
cfg.dincos = MDMA_DATA_16BIT;      // 目的紧凑写入
```

### 反向拷贝

```c
cfg.sinc     = MDMA_INC_DECR;      // 源递减
cfg.dinc     = MDMA_INC_INCR;      // 目的递增
cfg.src_addr = (uint32_t)&src[count - 1];   // 从末尾开始
```

### 字节序交换

```c
cfg.bex = 1;   // byte swap within half-word
cfg.hex = 1;   // half-word swap within word
cfg.wex = 1;   // word swap within double-word
// BEX+HEX = 32-bit 字节反转
// BEX+HEX+WEX = 64-bit 字节反转
```

### 矩阵转置 (链表 + Block Repeat)

将 block 最小化为单个元素，用 DUV 作为列步长：

```c
cfg.bndt = sizeof(int);             // 每个 block = 1 个元素
cfg.brc  = cols - 1;                // 一行的所有元素
cfg.suv  = 0;                       // 源: 连续读一行
cfg.duv  = rows * sizeof(int) - sizeof(int);  // 目的: 跨行写列
// 每行一个链表节点, N 行 = N-1 个节点 + 1 个初始配置
```

## 链表节点

```c
typedef struct {
    uint32_t TCR, BNDTR, SAR, DAR, BRUR, LAR, TBR, RSVD, MAR, MDR;
} __attribute__((aligned(8))) MdmaLinkNode_t;
```

- 40 字节，必须 8 字节对齐
- LAR 指向下一节点，LAR=0 结束
- TCR 中 TRGM/SWRM 必须与初始配置一致
- BNDTR 含 BRC/BRDUM/BRSUM/BNDT
- BRUR 含 DUV[31:16] 和 SUV[15:0]

## 中断标志

```c
#define MDMA_FLAG_TEIF   (1u << 0)  // 传输错误
#define MDMA_FLAG_CTCIF  (1u << 1)  // 通道传输完成
#define MDMA_FLAG_BRTIF  (1u << 2)  // block repeat 完成
#define MDMA_FLAG_BTIF   (1u << 3)  // block 完成
#define MDMA_FLAG_TCIF   (1u << 4)  // buffer 完成 (每个 TLEN+1)
```

## RCC 使能

```c
RCC->MP_AHB6ENSETR = (1u << 0);    // MDMA 时钟使能
```

基地址：0x58000000，32 个通道。
