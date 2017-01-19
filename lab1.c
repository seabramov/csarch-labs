#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long uint64;
typedef unsigned int uint32;
typedef unsigned short int uint16;

#define SEGMENT_DESCRIPTOR_SIZE	8
#define DEFAULT_LDT_NUM	10


#pragma pack(push, 1)

typedef struct dtr
{
	uint16 table_limit; 
	uint32 base_addr;
	uint16 pad;
} dtr_t;

typedef union selector
{
	uint16 sel;
	struct 
	{
		uint16 rpl:2;
		uint16 ti:1;
		uint16 index:13;
	};
} selector_t;

typedef union desc
{
	uint32 lo;
	uint32 hi;
	struct
	{
		uint32 seg_lim_lo:16;
		uint32 base_addr_lo:16;
		uint32 base_addr_mid:8;
		uint32 type:4;
		uint32 s:1;
		uint32 dpl:2;
		uint32 p:1;
		uint32 seg_lim_hi:4;
		uint32 avl:1;
		uint32 l:1;
		uint32 db:1;
		uint32 g:1;
		uint32 base_addr_hi:8;						
	}; 
} desc_t;

#pragma pack(pop)

void app_seg_type(FILE* fp, desc_t desc)
{
	switch (desc.type)
	{
		case 0: 
			fprintf(fp, " Read-Only type\n");
			break;
		case 1: 
			fprintf(fp, " Read-Only, accessed type\n");
			break;
		case 2: 
			fprintf(fp, " Read/Write type\n");
			break;
		case 3: 
			fprintf(fp, " Read/Write, accessed type\n");
			break;
		case 4: 
			fprintf(fp, " Read-Only, expand-down type\n");
			break;
		case 5: 
			fprintf(fp, " Read-Only, expand-down, accessed type\n");
			break;
		case 6: 
			fprintf(fp, " Read/Write, expand-down type\n");
			break;
		case 7: 
			fprintf(fp, " Read/Write, expand-down, accessed type\n");
			break;	
		case 8: 
			fprintf(fp, " Execute-Only type\n");
			break;
		case 9: 
			fprintf(fp, " Execute-Only, accessed type\n");
			break;	
		case 10: 
			fprintf(fp, " Execute/Read type\n");
			break;
		case 11: 
			fprintf(fp, " Execute/Read, accessed type\n");
			break;
		case 12: 
			fprintf(fp, " Execute-Only, conforming type\n");
			break;
		case 13: 
			fprintf(fp, " Execute-Only, conforming, accessed type\n");
			break;
		case 14: 
			fprintf(fp, " Execute/Read, conforming type\n");
			break;
		case 15: 
			fprintf(fp, " Execute/Read, conforming, accessed type\n");	
			break;
	}
}

int sys_seg_type(FILE* fp, desc_t desc)
{
	int ret = 0;

	switch (desc.type)
	{
		case 0: 
			fprintf(fp, " Reserved type\n");
			break;
		case 1: 
			fprintf(fp, " 16-bit TSS (Available) type\n");
			break;
		case 2: 
			fprintf(fp, " LDT type\n");
			ret |= 1; 
			break;
		case 3: 
			fprintf(fp, " 16-bit TSS (Busy) type\n");
			break;
		case 4: 
			fprintf(fp, " 16-bit Call Gate type\n");
			break;
		case 5: 
			fprintf(fp, " Task Gate type\n");
			break;
		case 6: 
			fprintf(fp, " 16-bit Interrupt Gate type\n");
			break;
		case 7: 
			fprintf(fp, " 16-bit Trap Gate type\n");
			break;
		case 8: 
			fprintf(fp, " Reserved type\n");
			break;
		case 9: 
			fprintf(fp, " 32-bit TSS (Available) type\n");
			break;
		case 10: 
			fprintf(fp, " Reserved type\n");
			break;
		case 11: 
			fprintf(fp, " 32-bit TSS (Busy) type\n");
			break;
		case 12: 
			fprintf(fp, " 32-bit Call Gate type\n");
			break;
		case 13: 
			fprintf(fp, " Reserved type\n");
			break;
		case 14: 
			fprintf(fp, " 32-bit Interrupt Gate type\n");
			break;
		case 15: 
			fprintf(fp, " 32-bit Trap Gate type\n");	
			break;
	}

	return ret;
}

int seg_type(FILE* fp, desc_t desc) 
{
	if (desc.s == 1)
	{
		fprintf(fp, "Code and Data segment:\n");
		app_seg_type(fp, desc);		
	}
	else
	{
		fprintf(fp, "System segment:\n");			
		return sys_seg_type(fp, desc);
	}

	return 0;
}

void priv_level(FILE* fp, desc_t desc)
{
	fprintf(fp, " Descriptor privilege level: %d\n", desc.dpl);
}

uint32 get_limit_from_desc(desc_t desc)
{
	return ((desc.seg_lim_hi << 16) | (desc.seg_lim_lo));
}

uint32 get_base_from_desc(desc_t desc)
{
	return ((desc.base_addr_hi << 24) | (desc.base_addr_mid << 16) | (desc.base_addr_lo));
}	

int seg_desc_handler(FILE* fp, desc_t desc)
{
	int ret = 0;

	if (desc.p == 1)
	{
		ret = seg_type(fp, desc);
		
		if (ret != 0)
			printf("ret code: %d\n", ret);
		
		priv_level(fp, desc);		
		fprintf(fp, " Base address: 0x%x\n", get_base_from_desc(desc));
		fprintf(fp, " Table limit: 0x%x\n", get_limit_from_desc(desc));
	}

	return ret;
}

void ldt_handler(desc_t desc)
{
	uint32 base_addr = 0;
	uint32 table_limit = 0;
	uint32 ldt_entries_num;
	desc_t* ldt_ptr;	
	int i;
	FILE* fp;

	fp = fopen("ldt.txt", "w+");

	base_addr = get_base_from_desc(desc);
	table_limit = get_limit_from_desc(desc);

	printf("ldt base addr: 0x%x\n", base_addr);
	printf("ldt table_limit: 0x%x\n", table_limit);

	ldt_ptr = (desc_t*) base_addr;
	ldt_entries_num = table_limit / SEGMENT_DESCRIPTOR_SIZE;

	for (i = 0; i < ldt_entries_num; i++)
	{
		seg_desc_handler(fp, ldt_ptr[i]);
	}

	fclose(fp);
}

void tss_handler(desc_t desc)
{
	uint32 base_addr = 0;
	uint32 table_limit = 0;
	uint32 tss_entries_num;
	desc_t* tss_ptr;	
	int i;
	FILE* fp;

	fp = fopen("tss.txt", "w+");

	base_addr = get_base_from_desc(desc);
	table_limit = get_limit_from_desc(desc);

	tss_ptr = (desc_t*) base_addr;
	tss_entries_num = table_limit / SEGMENT_DESCRIPTOR_SIZE;

	for (i = 0; i < tss_entries_num; i++)
	{
		seg_desc_handler(fp, tss_ptr[i]);
	}

	fclose(fp);
}

void sysseg_handler(int is_sysseg, desc_t desc)
{
	switch(is_sysseg)
	{
		case 1: 
			ldt_handler(desc);
			break;
		case 2: 
			tss_handler(desc);
			break;
		case 3:
			ldt_handler(desc);
			tss_handler(desc);
			break;
		default:
			printf("error\n");
			break;
	}	
}


int main()
{
	dtr_t _gdtr;
	dtr_t _idtr;
	FILE* fp;
	FILE* idt_fp;
	desc_t* desc_ptr;
	desc_t* idt_ptr;	
	uint32 gdtr_entries_num;
	uint32 idt_entries_num;

	int i;
	int is_sysseg = 0;

	fp = fopen("dtr.txt", "w+");

	if (fp == NULL)
		printf("file open error 0\n");

	printf("Hello world !\n");

	__asm { 
		sgdt _gdtr
		sidt _idtr
	}

	printf("GDT:\ntable limit: 0x%x, base address: 0x%x \n", 
		_gdtr.table_limit,
		_gdtr.base_addr
	);

	printf("IDT:\ntable limit: 0x%x, base address: 0x%x \n", 
		_idtr.table_limit,
		_idtr.base_addr
	);

	fprintf(fp, "table limit: 0x%x, base address: 0x%x \n", 
		_gdtr.table_limit,
		_gdtr.base_addr
	);


	desc_ptr = (desc_t*)_gdtr.base_addr;
	gdtr_entries_num = _gdtr.table_limit / SEGMENT_DESCRIPTOR_SIZE;

	for (i = 0; i < gdtr_entries_num; i++)
	{
		is_sysseg = seg_desc_handler(fp, desc_ptr[i]);
	
		if (is_sysseg)
		{
			sysseg_handler(is_sysseg, desc_ptr[i]);
		}
	}
	
	fclose(fp);

	idt_fp = fopen("idt.txt", "w+");

	if (idt_fp == NULL)
		printf("file open error 1\n");


	fprintf(idt_fp, "table limit: 0x%x, base address: 0x%x \n", 
		_idtr.table_limit,
		_idtr.base_addr
	);

	idt_ptr = (desc_t*)_idtr.base_addr;
	idt_entries_num = _idtr.table_limit / SEGMENT_DESCRIPTOR_SIZE;

	for (i = 0; i < idt_entries_num; i++)
	{
		is_sysseg = seg_desc_handler(idt_fp, idt_ptr[i]);		
	}

	fclose(idt_fp);

	return 0;
}

