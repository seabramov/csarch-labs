#include <stdio.h>
#include <stdlib.h>

typedef unsigned int uint32;
typedef unsigned short int uint16;

#define UD_NUM 6

#pragma pack(push, 1)
typedef struct _DTR {
    uint16 limit;
    uint32 base;
    uint16 padding;
} DTR;


typedef union _IDTENTRY {
    struct {
        uint16 offset_lo;
        uint16 selector;
        uint16 smth;
        uint16 offset_hi;
    };
    struct {
        uint32 raw0;
        uint32 raw1;
    };
} IDTENTRY, *PIDTENTRY;

typedef union _PTE {
    struct {
       uint32 P:1;
       uint32 RW:1;
       uint32 US:1;
       uint32 PWT:1;
       uint32 PCD:1;
       uint32 A:1;
       uint32 D:1;
       uint32 PAT_PS:1;
       uint32 G:1;
       uint32 OS:3;
       uint32 pfn:20;
    };
    uint32 raw;
} PTE,*PPTE;

#pragma pack(pop)

uint32 old_handler;
uint32 old_selector;
uint32 my_ud_count = 0;

void print_message(void)
{
        printf("I'm #UD (invalide opcode) exception handler !!!\n");
}

void __declspec(naked) ud_handler(void)
{
    __asm {
        //[esp+08h] flags
        //[esp+04h] cs
        //[esp+00h] eip
        // There is no error code for #UD
		call print_message
		inc my_ud_count
		add [esp], 2        
		iretd
    }
}

int main()
{
    DTR _idt;
    uint16 _cs;
    PIDTENTRY p_ud_entry_in_idt;
    uint32 my_handler = (uint32)&ud_handler;

    __asm {
        sidt _idt
        mov ax, cs
        mov _cs, ax
    }

    p_ud_entry_in_idt = &(((PIDTENTRY)_idt.base)[UD_NUM]);

    printf("Default #UD: 0x%08X'%08X \n", p_ud_entry_in_idt->raw1, p_ud_entry_in_idt->raw0);

    old_handler = (p_ud_entry_in_idt->offset_hi << 16) | (p_ud_entry_in_idt->offset_lo);
    old_selector = p_ud_entry_in_idt->selector;
    p_ud_entry_in_idt->offset_hi = (uint16)(my_handler >> 16);
    p_ud_entry_in_idt->offset_lo = (uint16)(my_handler & 0xFFFF);
    p_ud_entry_in_idt->selector = _cs;

    printf("My #UD: 0x%08X'%08X \n", p_ud_entry_in_idt->raw1, p_ud_entry_in_idt->raw0);
	printf("IDT: 0x%08x\n", _idt.base);
    printf("old_handler: 0x%04X:0x%08x\n", old_selector, old_handler);
    printf("my_handler 0x%04X:0x%08x\n", _cs, my_handler);

    printf("My ud count before #UD: %d\n", my_ud_count);
    
    // UD2 opcode: 0x0f0b
    __asm {
        byte 0x0F, 0x0B
    }

    printf("My ud count after #UD: %d\n", my_ud_count);
    printf("Good Finish !!!\n");

    return 0;
}
