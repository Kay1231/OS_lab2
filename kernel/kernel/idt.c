#include "x86.h"
#include "device.h"

#define INTERRUPT_GATE_32   0xE
#define TRAP_GATE_32        0xF

/* IDT表的内容 */
struct GateDescriptor idt[NR_IRQ]; // NR_IRQ=256, defined in x86/cpu.h

/* 初始化一个中断门(interrupt gate) */
static void setIntr(struct GateDescriptor *ptr, uint32_t selector, uint32_t offset, uint32_t dpl) {
	// TODO: 初始化interrupt gate
// 填充偏移量
    ptr->offset_15_0  = offset & 0xFFFF;
    ptr->offset_31_16 = (offset >> 16) & 0xFFFF;
    // 填充段选择子（使用 KSEL 宏转换为真正的选择子）
    ptr->segment = KSEL(selector);
    // 填充保留位
    ptr->pad0 = 0;
    // 填充门类型：32位中断门
    ptr->type = STS_IG32;
    // 系统段标志（0表示系统段）
    ptr->system = 0;
    // 特权级
    ptr->privilege_level = dpl;
    // 存在位（1表示有效）
    ptr->present = 1;
}

/* 初始化一个陷阱门(trap gate) */
static void setTrap(struct GateDescriptor *ptr, uint32_t selector, uint32_t offset, uint32_t dpl) {
	// TODO: 初始化trap gate
// 填充偏移量
    ptr->offset_15_0  = offset & 0xFFFF;
    ptr->offset_31_16 = (offset >> 16) & 0xFFFF;
    // 填充段选择子（使用 KSEL 宏转换为真正的选择子）
    ptr->segment = KSEL(selector);
    // 填充保留位
    ptr->pad0 = 0;
    // 填充门类型：32位陷阱门
    ptr->type = STS_TG32;
    // 系统段标志（0表示系统段）
    ptr->system = 0;
    // 特权级
    ptr->privilege_level = dpl;
    // 存在位（1表示有效）
    ptr->present = 1;
}

/* 声明函数，这些函数在汇编代码里定义 */
void irqEmpty();
void irqErrorCode();

void irqDoubleFault(); // 0x8
void irqInvalidTSS(); // 0xa
void irqSegNotPresent(); // 0xb
void irqStackSegFault(); // 0xc
void irqGProtectFault(); // 0xd
void irqPageFault(); // 0xe
void irqAlignCheck(); // 0x11
void irqSecException(); // 0x1e
void irqKeyboard(); 

void irqSyscall();

void initIdt() {
	int i;
	/* 为了防止系统异常终止，所有irq都有处理函数(irqEmpty)。 */
	for (i = 0; i < NR_IRQ; i ++) {
		setTrap(idt + i, SEG_KCODE, (uint32_t)irqEmpty, DPL_KERN);
	}
	/*
	 * init your idt here
	 * 初始化 IDT 表, 为中断设置中断处理函数
	 */
	/* Exceptions with error code */
	setTrap(idt + 0x8, SEG_KCODE, (uint32_t)irqDoubleFault, DPL_KERN);
	// TODO: 填好剩下的表项
	setTrap(idt + 0xa, SEG_KCODE, (uint32_t)irqInvalidTSS, DPL_KERN);
	setTrap(idt + 0xb, SEG_KCODE, (uint32_t)irqSegNotPresent, DPL_KERN);
	setTrap(idt + 0xc, SEG_KCODE, (uint32_t)irqStackSegFault, DPL_KERN);
	setTrap(idt + 0xd, SEG_KCODE, (uint32_t)irqGProtectFault, DPL_KERN);
	setTrap(idt + 0xe, SEG_KCODE, (uint32_t)irqPageFault, DPL_KERN);
	setTrap(idt + 0x11, SEG_KCODE, (uint32_t)irqAlignCheck, DPL_KERN);
	setTrap(idt + 0x1e, SEG_KCODE, (uint32_t)irqSecException, DPL_KERN);
	/* Exceptions with DPL = 3 */
	// TODO: 填好剩下的表项 
	// int3 断点异常 (0x3)
    setTrap(idt + 0x3, SEG_KCODE, (uint32_t)irqEmpty, DPL_USER);
    // into 溢出异常 (0x4)
    setTrap(idt + 0x4, SEG_KCODE, (uint32_t)irqEmpty, DPL_USER);
    // bound 越界异常 (0x5)
    setTrap(idt + 0x5, SEG_KCODE, (uint32_t)irqEmpty, DPL_USER);
    /* 硬件中断：键盘中断 (0x21，IRQ1，使用中断门关中断) */
    setIntr(idt + 0x21, SEG_KCODE, (uint32_t)irqKeyboard, DPL_KERN);
    /* 系统调用：int 0x80 (DPL=3允许用户态调用，使用陷阱门保持中断开启) */
    setTrap(idt + 0x80, SEG_KCODE, (uint32_t)irqSyscall, DPL_USER);
	/* 写入IDT */
	saveIdt(idt, sizeof(idt));
}
