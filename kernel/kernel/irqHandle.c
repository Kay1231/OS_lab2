#include "x86.h"
#include "device.h"
#include "keyboard.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

// 屏幕规格宏
#define VGA_BASE 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ATTR 0x0C


void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);

// 滚屏辅助函数
static void scrollScreen() {
    uint16_t *vga = (uint16_t *)VGA_BASE;
    // 上移一行
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    // 清空最后一行
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga[i] = ' ' | (VGA_ATTR << 8);
    }
    displayRow = VGA_HEIGHT - 1;
}


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case 0xd: // General Protection Exception
			GProtectFaultHandle(tf);
			break;
		case 0x21: // Keyboard Interrupt
			KeyboardHandle(tf);
			break;
		case 0x80: // System Call
			syscallHandle(tf);
			break;
		default:assert(0);
	}
	outByte(0x20, 0x20);
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	uint16_t *vga = (uint16_t *)VGA_BASE;
	if(code == 0xe){ // 退格符
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		// 如果缓冲区不空且光标不在行首，删除最后一个字符
        if (bufferHead != bufferTail && displayCol > 0) {
            bufferTail = (bufferTail - 1 + MAX_KEYBUFFER_SIZE) % MAX_KEYBUFFER_SIZE;
            // 更新屏幕：删除字符
            displayCol--;
            uint16_t *vga = (uint16_t*)0xB8000;
            int pos = displayRow * 80 + displayCol;
            vga[pos] = (0x0C << 8) | ' ';  // 空格覆盖
        }
	}else if(code == 0x1c){ // 回车符
		// TODO: 处理回车情况
		int next = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
        if (next != bufferHead) {
            keyBuffer[bufferTail] = '\n';
            bufferTail = next;
        }
        displayCol = 0;
        displayRow++;
        if (displayRow >= 25) scrollScreen();
        // 屏幕换行
	}else if(code < 0x81){ // 正常字符
		// TODO: 注意输入的大小写的实现、不可打印字符的处理
		char c = getChar(code);
        if (c != 0) {
            int next = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
            if (next != bufferHead) {
                keyBuffer[bufferTail] = c;
                bufferTail = next;
            }
            // 屏幕回显
            int pos = displayRow * VGA_WIDTH + displayCol;
            vga[pos] = (VGA_ATTR << 8) | c;
            displayCol++;
            if (displayCol >= VGA_WIDTH) {
                displayCol = 0;
                displayRow++;
                if (displayRow >= VGA_HEIGHT) scrollScreen();
            }
        }
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA); //TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
		if (character == '\n') {
            displayCol = 0;
            displayRow++;
			if (displayRow >= VGA_HEIGHT) scrollScreen();
        } else {
            int pos = displayRow * 80 + displayCol;
            vga[pos] = (0x0C << 8) | character;
            displayCol++;
            if (displayCol >= 80) {
                displayCol = 0;
                displayRow++;
				if (displayRow >= VGA_HEIGHT) scrollScreen();
            }
        }
    }
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	// 等待直到有按键
    while (bufferHead == bufferTail) {
        // 可以短暂开中断，避免死锁
        asm volatile("sti; hlt; cli");
    }
    char c = keyBuffer[bufferHead];
    bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
    tf->eax = (uint32_t)c;  // 返回值放入 eax
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	char *buf = (char*)tf->edx;   // 用户缓冲区地址
    int maxlen = tf->ebx;         // 最大长度（包括结尾 '\0'）
    int sel = USEL(SEG_UDATA);
    int count = 0;
    char c;

    asm volatile("movw %0, %%es"::"m"(sel));

    while (count < maxlen - 1) {
        // 等待键盘输入
        while (bufferHead == bufferTail) {
            asm volatile("sti; hlt; cli");
        }
		c = keyBuffer[bufferHead];
        bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

        if (c == '\n') {
            break;
        } else {
            asm volatile("movb %0, %%es:(%1)"::"r"(c), "r"(buf+count));
            count++;
        }
    }
    // 添加字符串结束符
    asm volatile("movb %0, %%es:(%1)"::"r"('\0'), "r"(buf+count));
    tf->eax = count; // 返回实际读入字符数
}
