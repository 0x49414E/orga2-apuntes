/* ** por compatibilidad se omiten tildes **
================================================================================
 TALLER System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Rutinas del controlador de interrupciones.
*/
#include "pic.h"

#define PIC1_PORT 0x20
#define PIC2_PORT 0xA0

#define ICW1_CASCADE_ICW4 0x11
#define ICW2_TYPE_20 0x20
#define ICW3_SLAVE_CONNECTED 0x4
#define ICW4_NOT_BUFFERED 0x1
#define OCW1 0xFF

#define ICW2_TYPE_28 0x28
#define ICW3_CONNECT_MASTER 0x2

static __inline __attribute__((always_inline)) void outb(uint32_t port,
                                                         uint8_t data) {
  __asm __volatile("outb %0,%w1" : : "a"(data), "d"(port));
}
void pic_finish1(void) { outb(PIC1_PORT, 0x20); }
void pic_finish2(void) {
  outb(PIC1_PORT, 0x20);
  outb(PIC2_PORT, 0x20);
}

void pic1_remap() {
  outb(PIC1_PORT, ICW1_CASCADE_ICW4);
  outb(PIC1_PORT + 1, ICW2_TYPE_20);
  outb(PIC1_PORT + 1, ICW3_SLAVE_CONNECTED);
  outb(PIC1_PORT + 1, ICW4_NOT_BUFFERED);
  outb(PIC1_PORT + 1, OCW1);
}

void pic2_remap() {
  outb(PIC2_PORT, ICW1_CASCADE_ICW4);
  outb(PIC2_PORT + 1, ICW2_TYPE_28);
  outb(PIC2_PORT + 1, ICW3_CONNECT_MASTER);
  outb(PIC2_PORT + 1, ICW4_NOT_BUFFERED);
  outb(PIC2_PORT + 1, OCW1);
}

// COMPLETAR: implementar pic_reset()
void pic_reset() {
  pic1_remap();
  pic2_remap();
}


void pic_enable() {
  outb(PIC1_PORT + 1, 0x00);
  outb(PIC2_PORT + 1, 0x00);
}

void pic_disable() {
  outb(PIC1_PORT + 1, 0xFF);
  outb(PIC2_PORT + 1, 0xFF);
}
