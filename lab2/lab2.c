#include "stdio.h"
#include "stdlib.h"

typedef unsigned int uint32;
typedef unsigned short int uint16;

#define CR0_PG (1<<31) //0x80000000
#define CR4_PSE (1<<4) //0x00000010

#define LARGE_PAGE_SIZE 4194304 // 4Mb     
#define PAGE_SIZE 0x1000
#define PF_NUM 14
#define BAD_PDI 0x222
#define BAD_ADDR 0x88812345 //1000 1000 10(00)
#define MAP_INDEX   377

//P=1, RW=1 (write), US=0 (kernel), 0,0, 0,0, 1,0, 000
//0000 1000 0011
#define PTE_KERNEL 0x083    

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
uint32 bad_pde_virtual_address;
uint32 my_pf_count = 0;

void __declspec(naked) pf_handler(void)
{
    __asm {
        pushfd
        push eax
        
        mov eax, cr2
        cmp eax, BAD_ADDR        
        jz my

        //call original
        pop eax
        popfd
        push old_selector        
        push old_handler
        retf
        //original will perform iretd himself

my:
        inc my_pf_count
        mov eax, bad_pde_virtual_address
        or [eax], 1
        //mov eax, cr3
        //mov cr3, eax
        invlpg [eax]

        //[esp+14h] flags
        //[esp+10h] cs
        //[esp+0Ch] eip
        //[esp+08h] error code
        //[esp+04h] flags again
        //[esp+00h] eax
        //add [esp+0Ch], 6 //skip instruction that generate #PF
        
        pop eax
        popfd
        add esp, 4
        iretd
    }
}

void main()
{
    uint32 _cr0 = 0;
    uint32 _cr4 = 0;
    DTR _idt;
    uint16 _cs;
    PIDTENTRY p_pf_entry_in_idt;
    IDTENTRY my_pf_entry;    
    uint32 my_handler = (uint32)&pf_handler;
    PPTE pd;
    int pdi;
    uint32* p;    
    uint32 addr;
    uint32* ptr;
    uint32 _cr3;

    __asm {
        sidt _idt
        mov ax, cs
        mov _cs, ax
    }

    p_pf_entry_in_idt = &(((PIDTENTRY)_idt.base)[PF_NUM]);

    printf("14.1: 0x%08X'%08X \n", p_pf_entry_in_idt->raw1, p_pf_entry_in_idt->raw0);

    old_handler = (p_pf_entry_in_idt->offset_hi << 16) | (p_pf_entry_in_idt->offset_lo);
    old_selector = p_pf_entry_in_idt->selector;
    p_pf_entry_in_idt->offset_hi = (uint16)(my_handler >> 16);
    p_pf_entry_in_idt->offset_lo = (uint16)(my_handler & 0xFFFF);
    p_pf_entry_in_idt->selector = _cs;

    printf("14.2: 0x%08X'%08X \n", p_pf_entry_in_idt->raw1, p_pf_entry_in_idt->raw0);
    
    printf("IDT: 0x%08x\n", _idt.base);
    printf("old_handler: 0x%04X:0x%08x\n", old_selector, old_handler);
    printf("my_handler 0x%04X:0x%08x\n", _cs, my_handler);

    {
        uint32 p = (uint32)malloc(2 * LARGE_PAGE_SIZE);
        p = (p & (~0x3fffff)) + LARGE_PAGE_SIZE;        
        pd = (PPTE)p;
    }

    printf("pd addr: 0x%x\n", (uint32)pd);

    for (pdi = 0; pdi < 1024; pdi++)
    {
        pd[pdi].raw = PTE_KERNEL;
        pd[pdi].pfn = (pdi*(1 << (22 - 12))); //4Mb
    }

    printf("pd[0] before access by software: %x\n", pd[0]);

    pd[BAD_PDI].P = 0;
    bad_pde_virtual_address = (uint32)&pd[BAD_PDI];

    pd[MAP_INDEX].pfn = ((uint32)pd >> 12); 
    
    //    addr = 0x5E400000;
    addr = (MAP_INDEX << 22);     
    printf("virtual address of page directory map is: 0x%x\n", addr);

    __asm {
        //prepare PD
        mov eax, pd
        mov cr3, eax
        //prepare 4Mb support
        mov eax, cr4
        or eax, 0x10
        mov cr4, eax
        //paging on
        mov eax, cr0
        or eax, 0x80000000
        mov cr0, eax
        //invalidate translation caches (recommended)
        mov eax, cr3
        mov cr3, eax
    }
    
    printf("Paging should work! \n");
    printf("Generating PF... \n");

    p = (uint32*)(BAD_ADDR);
    *p = 0; //generate #PF

    printf("my_pf_count = %d \n",my_pf_count);
    printf("Paging should work again! \n");

    _cr3 = 0;

    __asm {
        push eax
        mov eax, cr3
        mov _cr3, eax
        pop eax
    }

    printf("pd virt addr %x\n", (uint32)pd);
    printf("_cr3: %x\n", _cr3);

    __asm {
        push eax
        mov eax, cr0
        mov _cr0, eax
        pop eax
    }

    printf("0x%08X - %s\n",
        _cr0,
        _cr0 & CR0_PG ? "Paging on" : "Paging off"
    );

    ptr = (uint32*)addr;

    // After access by software Access and Dirty bits are set  
    // in page directory entries 0 and 1
    printf("pd[0]: %x\n", pd[0]);
    printf("pd[1]: %x\n", pd[1]);
    printf("pd[2]: %x\n", pd[2]);
    printf("pd[3]: %x\n", pd[3]);
    printf("pd[4]: %x\n", pd[4]);
    printf("pd[4]: %x\n", pd[377]);                
    printf("pde: 0x%x\n", ptr[0]);
    printf("pde: 0x%x\n", ptr[1]);
    printf("pde: 0x%x\n", ptr[2]);
    printf("pde: 0x%x\n", ptr[3]);
    printf("pde: 0x%x\n", ptr[4]);            
    printf("pde: 0x%x\n", ptr[337]);

}
