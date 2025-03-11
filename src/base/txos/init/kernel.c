/*++

Free95 20x/TX Kernel

You may only use this code if you agree to the terms of the Free95 Source Code License agreement (GNU GPL v3) (see LICENSE).
If you do not agree to the terms, do not use the code.


Module Name:

    kernel.c

Abstract:

    This module implements the kernel initialization code.

--*/

#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "../ke/string/string.h"
#include "../ke/idt/idt.h"
#include "../ke/io/io.h"
#include "../ke/memory/heap/kheap.h"
#include "../ke/memory/paging/paging.h"
#include "../ke/disk/disk.h"
#include "../ke/fs/pparser.h"
#include "../ke/disk/streamer.h"

uint16_t* video_mem = 0;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;

uint16_t terminal_make_char(char c, char colour)
{
    return (colour << 8) | c;
}

void terminal_putchar(int x, int y, char c, char colour)
{
    video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(c, colour);
}

void terminal_writechar(char c, char colour)
{
    if (c == '\n')
    {
        terminal_row += 1;
        terminal_col = 0;
        return;
    }

    terminal_putchar(terminal_col, terminal_row, c, colour);
    terminal_col += 1;
    if (terminal_col >= VGA_WIDTH)
    {
        terminal_col = 0;
        terminal_row += 1;
    }
}
void terminal_initialize()
{
    video_mem = (uint16_t*)(0xB8000);
    terminal_row = 0;
    terminal_col = 0;
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            terminal_putchar(x, y, ' ', 0);
        }
    }   
}



void print(const char* str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        terminal_writechar(str[i], 15);
    }
}

static int DbgInit()
{
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
   outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
   outb(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

   // Check if serial is faulty
   if(insb(PORT + 0) != 0xAE)
   {
      return 1;
   }

   // If serial is not faulty set it in normal operation mode
   // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
   outb(PORT + 4, 0x0F);
   return 0;
}

int IoTransEmpty()
{
   return insb(PORT + 5) & 0x20;
}

void DbgPutc(char a)
{
   while (IoTransEmpty() == 0);

   outb(PORT, a);
}

void DbgPrint(const char* str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        DbgPutc(str[i]);
    }
}

int IoSerialReceive()
{
   return insb(PORT + 5) & 1;
}

char DbgGetc()
{
   while (IoSerialReceive() == 0);

   return insb(PORT);
}

static struct paging_4gb_chunk* kernel_chunk = 0;

void kernel_main()
{
	DbgInit();

	DbgPrint("kernel_main() called\n\r");

    terminal_initialize();
    DbgPrint("VGA Text Mode Initialized\n\r");
    print("Free95 0.2.1\nBooted Successfully");

	kheap_init();

	DbgPrint("Kernel Heap Initialized\n\r");

    idt_init();

    DbgPrint("IDT Initialized\n\r");

    kernel_chunk = paging_new_4gb(PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
    
    paging_switch(paging_4gb_chunk_get_directory(kernel_chunk));

    char *ptr = kzalloc(4096);
    paging_set(paging_4gb_chunk_get_directory(kernel_chunk), (void*)0x1000, (uint32_t)ptr | PAGING_ACCESS_FROM_ALL | PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);

    enable_paging();

	DbgPrint("Enabled Paging\n\rPaging Test:\n\r");

	char *ptr2 = (char*)0x1000;
	ptr2[0] = 'A';
	ptr2[1] = 'B';
	
	DbgPrint(ptr2);
	DbgPrint("\n\r");

	DiskInit();
	DbgPrint("Disk Driver Initialized\n\r");

    enable_interrupts();

    DbgPrint("Enabled Interrupts\n\r");

    struct disk_stream *stream = diskstreamer_new(0);
    diskstreamer_seek(stream, 0x201);
    unsigned char c = 0;
    diskstreamer_read(stream, &c, 1);

    while(1);
}
