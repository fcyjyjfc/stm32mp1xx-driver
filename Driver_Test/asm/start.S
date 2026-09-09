.equ      AHB4ENSETR,    0X50000a28

.text
.global _start
_start:
  b  reset
  ldr  pc,_undefined_instruction 
  ldr  pc,_software_interrupt 
  ldr  pc,_prefetch_abort 
  ldr  pc,_data_abort 
  ldr  pc,_not_used 
  ldr  pc,_irq 
  ldr  pc,_fiq 

_undefined_instruction: .word  _undefined_instruction 
_software_interrupt: .word  _software_interrupt 
_prefetch_abort:  .word  _prefetch_abort 
_data_abort:   .word  _data_abort 
_not_used:    .word  _not_used 
_irq:     .word  irq_handler 
_fiq:     .word  _fiq 

reset: 
 ldr r0,= _start 
 mcr p15,0,r0,c12,c0,0  // Vector Base Address Register 

  mrs   r0,cpsr 
  bic  r0,r0,#0x1f 
  orr  r0,r0,#0xd3 
  msr  cpsr,r0        //  Enable svc mode of cpu

  // Clean + Invalidate entire L1 D-Cache (32KB, 4-way, 64B line, 128 sets)
  mov  r0, #0                      // way = 0
1:
  mov  r1, #0                      // set = 0
2:
  orr  r2, r0, r1, lsl #6         // val = way[31:30] | set[12:6]
  mcr  p15, 0, r2, c7, c14, 2     // DCCISW: clean + invalidate
  add  r1, r1, #1
  cmp  r1, #128
  blt  2b
  add  r0, r0, #0x40000000        // next way (1 << 30)
  cmp  r0, #0
  bne  1b
  mcr  p15, 0, r0, c7, c10, 4     // DSB

  // Disable D-Cache
  mrc  p15, 0, r0, c1, c0, 0
  bic  r0, r0, #4
  mcr  p15, 0, r0, c1, c0, 0
  mcr  p15, 0, r0, c7, c10, 4     // DSB
  mcr  p15, 0, r0, c7, c5, 4      // ISB

init_stack:
  ldr  r0,stacktop         /*get stack top pointer*/ 

 /********svc mode stack********/ 
  mov  sp,r0 
  sub  r0,#128*4          /*512 byte  for irq mode of stack*/ 
 /****irq mode stack**/ 
  msr  cpsr,#0xd2 
  mov  sp,r0 
  sub  r0,#128*4          /*512 byte  for irq mode of stack*/ 
 /***fiq mode stack***/ 
  msr  cpsr,#0xd1 
  mov  sp,r0 
  sub  r0,#0 
 /***abort mode stack***/ 
  msr  cpsr,#0xd7 
  mov  sp,r0 
  sub  r0,#0 
 /***undefine mode stack***/ 
  msr  cpsr,#0xdb 
  mov  sp,r0 
  sub  r0,#0 
   /*** system mode stack (privileged, same regs as usr) ***/
  msr  cpsr,#0xdF       /* I=1,F=1: 屏蔽 IRQ/FIQ. 外设残留中断 (如 DMA TCIF)
                         * 在外设复位+GIC清理完成前不应响应, 否则
                         * 未初始化的 ISR 或残留挂起会导致误触发.
                         * EnableCpuIrq() 在 TestCommonInit 末尾统一开启. */
  mov  sp,r0

  // Zero .bss: 镜像由 u-boot 整块载入 DDR, .bss 区是上次运行残留的任意值,
  // 不清零则零初始化静态量 (菜单状态/收发包游标/惰性激活标志等) 从脏值
  // 起步 —— 实测 g_activated 残留非 0 导致 ActivateGmac 缓存假命中,
  // 测试 2 打印未初始化局部量 (回归根源, 与 BTIMER 静态标志同族).
  ldr  r0,=__bss_start
  ldr  r1,=__bss_end
  mov  r2,#0
zbss_loop:
  cmp  r0,r1
  bge  zbss_done
  str  r2,[r0],#4
  b    zbss_loop
zbss_done:

 bl main

 .align 4 
 /****  swi_interrupt handler  ****/ 

 /****  irq_handler  ****/
irq_handler:
 sub  lr,lr,#4
 stmfd sp!,{r0-r12,lr}            /* save regs + return addr   */
 bl   do_irq
 ldmfd sp!,{r0-r12,pc}^

stacktop:    .word   stack+4*1024
.data

stack:  .space  4*1024

loop: 
 b loop 
.end 
