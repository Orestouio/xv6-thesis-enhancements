#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

.code16
.globl start
start:
  cli
  cld

  xorw    %ax,%ax
  movw    %ax,%ds
  movw    %ax,%es
  movw    %ax,%ss

  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

  ljmpl    $(SEG_KCODE<<3), $(start32)

.code32
start32:
  movw    $(SEG_KDATA<<3), %ax
  movw    %ax, %ds
  movw    %ax, %es
  movw    %ax, %ss
  movw    $0, %ax
  movw    %ax, %fs
  movw    %ax, %gs

  movl    (start-4), %esp

  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4

  movl    (start-12), %eax
  movl    %eax, %cr3

  movl    %cr0, %eax
  orl     $(CR0_PE|CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  call    *(start-8)

  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx
spin:
  jmp     spin

.p2align 2
gdt:
  SEG_NULLASM
  SEG_ASM(STA_X|STA_R, 0, 0xffffffff)
  SEG_ASM(STA_W, 0, 0xffffffff)

gdtdesc:
  .word   (gdtdesc - gdt - 1)
  .long   gdt