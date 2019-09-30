/* Fake6502 CPU emulator core v1.1 *******************
 * (c)2011 Mike Chambers (miker00lz@gmail.com)       *
 *****************************************************
 * v1.1 - Small bugfix in BIT opcode, but it was the *
 *        difference between a few games in my NES   *
 *        emulator working and being broken!         *
 *        I went through the rest carefully again    *
 *        after fixing it just to make sure I didn't *
 *        have any other typos! (Dec. 17, 2011)      *
 *                                                   *
 * v1.0 - First release (Nov. 24, 2011)              *
 *****************************************************
 * LICENSE: This source code is released into the    *
 * public domain, but if you use it please do give   *
 * credit. I put a lot of effort into writing this!  *
 *                                                   *
 *****************************************************
 * Fake6502 is a MOS Technology 6502 CPU emulation   *
 * engine in C. It was written as part of a Nintendo *
 * Entertainment System emulator I've been writing.  *
 *                                                   *
 * A couple important things to know about are two   *
 * defines in the code. One is "UNDOCUMENTED" which, *
 * when defined, allows Fake6502 to compile with     *
 * full support for the more predictable             *
 * undocumented instructions of the 6502. If it is   *
 * undefined, undocumented opcodes just act as NOPs. *
 *                                                   *
 * The other define is "NES_CPU", which causes the   *
 * code to compile without support for binary-coded  *
 * decimal (BCD) support for the ADC and SBC         *
 * opcodes. The Ricoh 2A03 CPU in the NES does not   *
 * support BCD, but is otherwise identical to the    *
 * standard MOS 6502. (Note that this define is      *
 * enabled in this file if you haven't changed it    *
 * yourself. If you're not emulating a NES, you      *
 * should comment it out.)                           *
 *                                                   *
 * If you do discover an error in timing accuracy,   *
 * or operation in general please e-mail me at the   *
 * address above so that I can fix it. Thank you!    *
 *                                                   *
 *****************************************************
 * Usage:                                            *
 *                                                   *
 * Fake6502 requires you to provide two external     *
 * functions:                                        *
 *                                                   *
 * uint8_t read6502(uint16_t address)                *
 * void write6502(uint16_t address, uint8_t value)   *
 *                                                   *
 * You may optionally pass Fake6502 the pointer to a *
 * function which you want to be called after every  *
 * emulated instruction. This function should be a   *
 * void with no parameters expected to be passed to  *
 * it.                                               *
 *                                                   *
 * This can be very useful. For example, in a NES    *
 * emulator, you check the number of clock ticks     *
 * that have passed so you can know when to handle   *
 * APU events.                                       *
 *                                                   *
 * To pass Fake6502 this pointer, use the            *
 * hookexternal(void *funcptr) function provided.    *
 *                                                   *
 * To disable the hook later, pass NULL to it.       *
 *****************************************************
 * Useful functions in this emulator:                *
 *                                                   *
 * void reset6502()                                  *
 *   - Call this once before you begin execution.    *
*                                                   *
* void exec6502(uint32_t tickcount)                 *
*   - Execute 6502 code up to the next specified    *
*     count of clock ticks.                         *
*                                                   *
* void step6502()                                   *
*   - Execute a single instrution.                  *
*                                                   *
* void irq6502()                                    *
*   - Trigger a hardware IRQ in the 6502 core.      *
*                                                   *
* void nmi6502()                                    *
*   - Trigger an NMI in the 6502 core.              *
*                                                   *
* void hookexternal(void *funcptr)                  *
*   - Pass a pointer to a void function taking no   *
*     parameters. This will cause Fake6502 to call  *
*     that function once after each emulated        *
*     instruction.                                  *
*                                                   *
*****************************************************
* Useful variables in this emulator:                *
*                                                   *
* uint32_t clockticks6502                           *
*   - A running total of the emulated cycle count.  *
*                                                   *
* uint32_t instructions                             *
*   - A running total of the total emulated         *
*     instruction count. This is not related to     *
*     clock cycle timing.                           *
*                                                   *
*****************************************************/

#include <stdio.h>
#include <stdint.h>

//6502 defines
#define UNDOCUMENTED //when this is defined, undocumented opcodes are handled.
//otherwise, they're simply treated as NOPs.

//#define NES_CPU      //when this is defined, the binary-coded decimal (BCD)
//status flag is not honored by ADC and SBC. the 2A03
//CPU in the Nintendo Entertainment System does not
//support BCD operation.

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#define BASE_STACK     0x100

#define saveaccum(n) cpu.a = (uint8_t)((n) & 0x00FF)


//flag modifier macros
#define setcarry() cpu.status |= FLAG_CARRY
#define clearcarry() cpu.status &= (~FLAG_CARRY)
#define setzero() cpu.status |= FLAG_ZERO
#define clearzero() cpu.status &= (~FLAG_ZERO)
#define setinterrupt() cpu.status |= FLAG_INTERRUPT
#define clearinterrupt() cpu.status &= (~FLAG_INTERRUPT)
#define setdecimal() cpu.status |= FLAG_DECIMAL
#define cleardecimal() cpu.status &= (~FLAG_DECIMAL)
#define setoverflow() cpu.status |= FLAG_OVERFLOW
#define clearoverflow() cpu.status &= (~FLAG_OVERFLOW)
#define setsign() cpu.status |= FLAG_SIGN
#define clearsign() cpu.status &= (~FLAG_SIGN)


//flag calculation macros
#define zerocalc(n) {\
  if ((n) & 0x00FF) clearzero();\
  else setzero();\
}

#define signcalc(n) {\
  if ((n) & 0x0080) setsign();\
  else clearsign();\
}

#define carrycalc(n) {\
  if ((n) & 0xFF00) setcarry();\
  else clearcarry();\
}

#define overflowcalc(n, m, o) { /* n = result, m = accumulator, o = memory */ \
  if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow();\
  else clearoverflow();\
}

#include "cpu.h"

CPU_State cpu = {0};

//externally supplied functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);

//a few general functions used by various other functions
void push16(uint16_t pushval) {
  write6502(BASE_STACK + cpu.sp, (pushval >> 8) & 0xFF);
  write6502(BASE_STACK + ((cpu.sp - 1) & 0xFF), pushval & 0xFF);
  cpu.sp -= 2;
}

void push8(uint8_t pushval) {
  write6502(BASE_STACK + cpu.sp--, pushval);
}

uint16_t pull16() {
  uint16_t temp16;
  temp16 = read6502(BASE_STACK + ((cpu.sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((cpu.sp + 2) & 0xFF)) << 8);
  cpu.sp += 2;
  return(temp16);
}

uint8_t pull8() {
  return (read6502(BASE_STACK + ++cpu.sp));
}

void reset6502() {
  cpu.pc = (uint16_t)read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
  cpu.a = 0;
  cpu.x = 0;
  cpu.y = 0;
  cpu.sp = 0xFD;
  cpu.status |= FLAG_CONSTANT;
}


static void (*addrtable[256])();
static void (*optable[256])();
uint8_t penaltyop, penaltyaddr;

//addressing mode functions, calculates effective addresses
static void imp() { //implied
}

static void acc() { //accumulator
}

static void imm() { //immediate
  cpu.ea = cpu.pc++;
}

static void zp() { //zero-page
  cpu.ea = (uint16_t)read6502((uint16_t)cpu.pc++);
}

static void zpx() { //zero-page,X
  cpu.ea = ((uint16_t)read6502((uint16_t)cpu.pc++) + (uint16_t)cpu.x) & 0xFF; //zero-page wraparound
}

static void zpy() { //zero-page,Y
  cpu.ea = ((uint16_t)read6502((uint16_t)cpu.pc++) + (uint16_t)cpu.y) & 0xFF; //zero-page wraparound
}

static void rel() { //relative for branch ops (8-bit immediate cpu.value, sign-extended)
  cpu.reladdr = (uint16_t)read6502(cpu.pc++);
  if (cpu.reladdr & 0x80) cpu.reladdr |= 0xFF00;
}

static void abso() { //absolute
  cpu.ea = (uint16_t)read6502(cpu.pc) | ((uint16_t)read6502(cpu.pc+1) << 8);
  cpu.pc += 2;
}

static void absx() { //absolute,X
  uint16_t startpage;
  cpu.ea = ((uint16_t)read6502(cpu.pc) | ((uint16_t)read6502(cpu.pc+1) << 8));
  startpage = cpu.ea & 0xFF00;
  cpu.ea += (uint16_t)cpu.x;

  if (startpage != (cpu.ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
    penaltyaddr = 1;
  }

  cpu.pc += 2;
}

static void absy() { //absolute,Y
  uint16_t startpage;
  cpu.ea = ((uint16_t)read6502(cpu.pc) | ((uint16_t)read6502(cpu.pc+1) << 8));
  startpage = cpu.ea & 0xFF00;
  cpu.ea += (uint16_t)cpu.y;

  if (startpage != (cpu.ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
    penaltyaddr = 1;
  }

  cpu.pc += 2;
}

static void ind() { //indirect
  uint16_t eahelp, eahelp2;
  eahelp = (uint16_t)read6502(cpu.pc) | (uint16_t)((uint16_t)read6502(cpu.pc+1) << 8);
  eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
  cpu.ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
  cpu.pc += 2;
}

static void indx() { // (indirect,X)
  uint16_t eahelp;
  eahelp = (uint16_t)(((uint16_t)read6502(cpu.pc++) + (uint16_t)cpu.x) & 0xFF); //zero-page wraparound for table pointer
  cpu.ea = (uint16_t)read6502(eahelp & 0x00FF) | ((uint16_t)read6502((eahelp+1) & 0x00FF) << 8);
}

static void indy() { // (indirect),Y
  uint16_t eahelp, eahelp2, startpage;
  eahelp = (uint16_t)read6502(cpu.pc++);
  eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //zero-page wraparound
  cpu.ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
  startpage = cpu.ea & 0xFF00;
  cpu.ea += (uint16_t)cpu.y;

  if (startpage != (cpu.ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
    penaltyaddr = 1;
  }
}

static uint16_t getvalue() {
  if (addrtable[cpu.opcode] == acc) return((uint16_t)cpu.a);
  else return((uint16_t)read6502(cpu.ea));
}

static uint16_t getvalue16() {
  return((uint16_t)read6502(cpu.ea) | ((uint16_t)read6502(cpu.ea+1) << 8));
}

static void putvalue(uint16_t saveval) {
  if (addrtable[cpu.opcode] == acc) cpu.a = (uint8_t)(saveval & 0x00FF);
  else write6502(cpu.ea, (saveval & 0x00FF));
}


//instruction handler functions
static void adc() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a + cpu.value + (uint16_t)(cpu.status & FLAG_CARRY);

  carrycalc(cpu.result);
  zerocalc(cpu.result);
  overflowcalc(cpu.result, cpu.a, cpu.value);
  signcalc(cpu.result);

#ifndef NES_CPU
  if (cpu.status & FLAG_DECIMAL) {
    clearcarry();

    if ((cpu.a & 0x0F) > 0x09) {
      cpu.a += 0x06;
    }
    if ((cpu.a & 0xF0) > 0x90) {
      cpu.a += 0x60;
      setcarry();
    }

    cpu.clockticks6502++;
  }
#endif

  saveaccum(cpu.result);
}

static void and() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a & cpu.value;

  zerocalc(cpu.result);
  signcalc(cpu.result);

  saveaccum(cpu.result);
}

static void asl() {
  cpu.value = getvalue();
  cpu.result = cpu.value << 1;

  carrycalc(cpu.result);
  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void bcc() {
  if ((cpu.status & FLAG_CARRY) == 0) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void bcs() {
  if ((cpu.status & FLAG_CARRY) == FLAG_CARRY) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void beq() {
  if ((cpu.status & FLAG_ZERO) == FLAG_ZERO) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void bit() {
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a & cpu.value;

  zerocalc(cpu.result);
  cpu.status = (cpu.status & 0x3F) | (uint8_t)(cpu.value & 0xC0);
}

static void bmi() {
  if ((cpu.status & FLAG_SIGN) == FLAG_SIGN) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void bne() {
  if ((cpu.status & FLAG_ZERO) == 0) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void bpl() {
  if ((cpu.status & FLAG_SIGN) == 0) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void brk() {
  cpu.pc++;
  push16(cpu.pc); //push next instruction address onto stack
  push8(cpu.status | FLAG_BREAK); //push CPU cpu.status to stack
  setinterrupt(); //set interrupt flag
  cpu.pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

static void bvc() {
  if ((cpu.status & FLAG_OVERFLOW) == 0) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void bvs() {
  if ((cpu.status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
    cpu.oldpc = cpu.pc;
    cpu.pc += cpu.reladdr;
    if ((cpu.oldpc & 0xFF00) != (cpu.pc & 0xFF00)) cpu.clockticks6502 += 2; //check if jump crossed a page boundary
    else cpu.clockticks6502++;
  }
}

static void clc() {
  clearcarry();
}

static void cld() {
  cleardecimal();
}

static void cli() {
  clearinterrupt();
}

static void clv() {
  clearoverflow();
}

static void cmp() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a - cpu.value;

  if (cpu.a >= (uint8_t)(cpu.value & 0x00FF)) setcarry();
  else clearcarry();
  if (cpu.a == (uint8_t)(cpu.value & 0x00FF)) setzero();
  else clearzero();
  signcalc(cpu.result);
}

static void cpx() {
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.x - cpu.value;

  if (cpu.x >= (uint8_t)(cpu.value & 0x00FF)) setcarry();
  else clearcarry();
  if (cpu.x == (uint8_t)(cpu.value & 0x00FF)) setzero();
  else clearzero();
  signcalc(cpu.result);
}

static void cpy() {
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.y - cpu.value;

  if (cpu.y >= (uint8_t)(cpu.value & 0x00FF)) setcarry();
  else clearcarry();
  if (cpu.y == (uint8_t)(cpu.value & 0x00FF)) setzero();
  else clearzero();
  signcalc(cpu.result);
}

static void dec() {
  cpu.value = getvalue();
  cpu.result = cpu.value - 1;

  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void dex() {
  cpu.x--;

  zerocalc(cpu.x);
  signcalc(cpu.x);
}

static void dey() {
  cpu.y--;

  zerocalc(cpu.y);
  signcalc(cpu.y);
}

static void eor() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a ^ cpu.value;

  zerocalc(cpu.result);
  signcalc(cpu.result);

  saveaccum(cpu.result);
}

static void inc() {
  cpu.value = getvalue();
  cpu.result = cpu.value + 1;

  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void inx() {
  cpu.x++;

  zerocalc(cpu.x);
  signcalc(cpu.x);
}

static void iny() {
  cpu.y++;

  zerocalc(cpu.y);
  signcalc(cpu.y);
}

static void jmp() {
  cpu.pc = cpu.ea;
}

static void jsr() {
  push16(cpu.pc - 1);
  cpu.pc = cpu.ea;
}

static void lda() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.a = (uint8_t)(cpu.value & 0x00FF);

  zerocalc(cpu.a);
  signcalc(cpu.a);
}

static void ldx() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.x = (uint8_t)(cpu.value & 0x00FF);

  zerocalc(cpu.x);
  signcalc(cpu.x);
}

static void ldy() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.y = (uint8_t)(cpu.value & 0x00FF);

  zerocalc(cpu.y);
  signcalc(cpu.y);
}

static void lsr() {
  cpu.value = getvalue();
  cpu.result = cpu.value >> 1;

  if (cpu.value & 1) setcarry();
  else clearcarry();
  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void nop() {
  switch (cpu.opcode) {
    case 0x1C:
    case 0x3C:
    case 0x5C:
    case 0x7C:
    case 0xDC:
    case 0xFC:
      penaltyop = 1;
      break;
  }
}

static void ora() {
  penaltyop = 1;
  cpu.value = getvalue();
  cpu.result = (uint16_t)cpu.a | cpu.value;

  zerocalc(cpu.result);
  signcalc(cpu.result);

  saveaccum(cpu.result);
}

static void pha() {
  push8(cpu.a);
}

static void php() {
  push8(cpu.status | FLAG_BREAK);
}

static void pla() {
  cpu.a = pull8();

  zerocalc(cpu.a);
  signcalc(cpu.a);
}

static void plp() {
  cpu.status = pull8() | FLAG_CONSTANT;
}

static void rol() {
  cpu.value = getvalue();
  cpu.result = (cpu.value << 1) | (cpu.status & FLAG_CARRY);

  carrycalc(cpu.result);
  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void ror() {
  cpu.value = getvalue();
  cpu.result = (cpu.value >> 1) | ((cpu.status & FLAG_CARRY) << 7);

  if (cpu.value & 1) setcarry();
  else clearcarry();
  zerocalc(cpu.result);
  signcalc(cpu.result);

  putvalue(cpu.result);
}

static void rti() {
  cpu.status = pull8();
  cpu.value = pull16();
  cpu.pc = cpu.value;
}

static void rts() {
  cpu.value = pull16();
  cpu.pc = cpu.value + 1;
}

static void sbc() {
  penaltyop = 1;
  cpu.value = getvalue() ^ 0x00FF;
  cpu.result = (uint16_t)cpu.a + cpu.value + (uint16_t)(cpu.status & FLAG_CARRY);

  carrycalc(cpu.result);
  zerocalc(cpu.result);
  overflowcalc(cpu.result, cpu.a, cpu.value);
  signcalc(cpu.result);

#ifndef NES_CPU
  if (cpu.status & FLAG_DECIMAL) {
    clearcarry();

    cpu.a -= 0x66;
    if ((cpu.a & 0x0F) > 0x09) {
      cpu.a += 0x06;
    }
    if ((cpu.a & 0xF0) > 0x90) {
      cpu.a += 0x60;
      setcarry();
    }

    cpu.clockticks6502++;
  }
#endif

  saveaccum(cpu.result);
}

static void sec() {
  setcarry();
}

static void sed() {
  setdecimal();
}

static void sei() {
  setinterrupt();
}

static void sta() {
  putvalue(cpu.a);
}

static void stx() {
  putvalue(cpu.x);
}

static void sty() {
  putvalue(cpu.y);
}

static void tax() {
  cpu.x = cpu.a;

  zerocalc(cpu.x);
  signcalc(cpu.x);
}

static void tay() {
  cpu.y = cpu.a;

  zerocalc(cpu.y);
  signcalc(cpu.y);
}

static void tsx() {
  cpu.x = cpu.sp;

  zerocalc(cpu.x);
  signcalc(cpu.x);
}

static void txa() {
  cpu.a = cpu.x;

  zerocalc(cpu.a);
  signcalc(cpu.a);
}

static void txs() {
  cpu.sp = cpu.x;
}

static void tya() {
  cpu.a = cpu.y;

  zerocalc(cpu.a);
  signcalc(cpu.a);
}

//undocumented instructions
#ifdef UNDOCUMENTED
static void lax() {
  lda();
  ldx();
}

static void sax() {
  sta();
  stx();
  putvalue(cpu.a & cpu.x);
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void dcp() {
  dec();
  cmp();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void isb() {
  inc();
  sbc();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void slo() {
  asl();
  ora();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void rla() {
  rol();
  and();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void sre() {
  lsr();
  eor();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}

static void rra() {
  ror();
  adc();
  if (penaltyop && penaltyaddr) cpu.clockticks6502--;
}
#else
#define lax nop
#define sax nop
#define dcp nop
#define isb nop
#define slo nop
#define rla nop
#define sre nop
#define rra nop
#endif


static void (*addrtable[256])() = {
  /*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
  /* 0 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 0 */
  /* 1 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 1 */
  /* 2 */    abso, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 2 */
  /* 3 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 3 */
  /* 4 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 4 */
  /* 5 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 5 */
  /* 6 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abso, /* 6 */
  /* 7 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 7 */
  /* 8 */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* 8 */
  /* 9 */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* 9 */
  /* A */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* A */
  /* B */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* B */
  /* C */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* C */
  /* D */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* D */
  /* E */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* E */
  /* F */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx  /* F */
};

static void (*optable[256])() = {
  /*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |      */
  /* 0 */      brk,  ora,  nop,  slo,  nop,  ora,  asl,  slo,  php,  ora,  asl,  nop,  nop,  ora,  asl,  slo, /* 0 */
  /* 1 */      bpl,  ora,  nop,  slo,  nop,  ora,  asl,  slo,  clc,  ora,  nop,  slo,  nop,  ora,  asl,  slo, /* 1 */
  /* 2 */      jsr,  and,  nop,  rla,  bit,  and,  rol,  rla,  plp,  and,  rol,  nop,  bit,  and,  rol,  rla, /* 2 */
  /* 3 */      bmi,  and,  nop,  rla,  nop,  and,  rol,  rla,  sec,  and,  nop,  rla,  nop,  and,  rol,  rla, /* 3 */
  /* 4 */      rti,  eor,  nop,  sre,  nop,  eor,  lsr,  sre,  pha,  eor,  lsr,  nop,  jmp,  eor,  lsr,  sre, /* 4 */
  /* 5 */      bvc,  eor,  nop,  sre,  nop,  eor,  lsr,  sre,  cli,  eor,  nop,  sre,  nop,  eor,  lsr,  sre, /* 5 */
  /* 6 */      rts,  adc,  nop,  rra,  nop,  adc,  ror,  rra,  pla,  adc,  ror,  nop,  jmp,  adc,  ror,  rra, /* 6 */
  /* 7 */      bvs,  adc,  nop,  rra,  nop,  adc,  ror,  rra,  sei,  adc,  nop,  rra,  nop,  adc,  ror,  rra, /* 7 */
  /* 8 */      nop,  sta,  nop,  sax,  sty,  sta,  stx,  sax,  dey,  nop,  txa,  nop,  sty,  sta,  stx,  sax, /* 8 */
  /* 9 */      bcc,  sta,  nop,  nop,  sty,  sta,  stx,  sax,  tya,  sta,  txs,  nop,  nop,  sta,  nop,  nop, /* 9 */
  /* A */      ldy,  lda,  ldx,  lax,  ldy,  lda,  ldx,  lax,  tay,  lda,  tax,  nop,  ldy,  lda,  ldx,  lax, /* A */
  /* B */      bcs,  lda,  nop,  lax,  ldy,  lda,  ldx,  lax,  clv,  lda,  tsx,  lax,  ldy,  lda,  ldx,  lax, /* B */
  /* C */      cpy,  cmp,  nop,  dcp,  cpy,  cmp,  dec,  dcp,  iny,  cmp,  dex,  nop,  cpy,  cmp,  dec,  dcp, /* C */
  /* D */      bne,  cmp,  nop,  dcp,  nop,  cmp,  dec,  dcp,  cld,  cmp,  nop,  dcp,  nop,  cmp,  dec,  dcp, /* D */
  /* E */      cpx,  sbc,  nop,  isb,  cpx,  sbc,  inc,  isb,  inx,  sbc,  nop,  sbc,  cpx,  sbc,  inc,  isb, /* E */
  /* F */      beq,  sbc,  nop,  isb,  nop,  sbc,  inc,  isb,  sed,  sbc,  nop,  isb,  nop,  sbc,  inc,  isb  /* F */
};

static const uint32_t ticktable[256] = {
  /*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
  /* 0 */      7,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    4,    4,    6,    6,  /* 0 */
  /* 1 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 1 */
  /* 2 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    4,    4,    6,    6,  /* 2 */
  /* 3 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 3 */
  /* 4 */      6,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    3,    4,    6,    6,  /* 4 */
  /* 5 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 5 */
  /* 6 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    5,    4,    6,    6,  /* 6 */
  /* 7 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 7 */
  /* 8 */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* 8 */
  /* 9 */      2,    6,    2,    6,    4,    4,    4,    4,    2,    5,    2,    5,    5,    5,    5,    5,  /* 9 */
  /* A */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* A */
  /* B */      2,    5,    2,    5,    4,    4,    4,    4,    2,    4,    2,    4,    4,    4,    4,    4,  /* B */
  /* C */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* C */
  /* D */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* D */
  /* E */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* E */
  /* F */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7   /* F */
};


void nmi6502() {
  push16(cpu.pc);
  push8(cpu.status);
  cpu.status |= FLAG_INTERRUPT;
  cpu.pc = (uint16_t)read6502(0xFFFA) | ((uint16_t)read6502(0xFFFB) << 8);
}

void irq6502() {
  push16(cpu.pc);
  push8(cpu.status);
  cpu.status |= FLAG_INTERRUPT;
  cpu.pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

uint8_t callexternal = 0;
void (*loopexternal)();

void exec6502(uint32_t tickcount) {
  cpu.clockgoal6502 += tickcount;

  while (cpu.clockticks6502 < cpu.clockgoal6502) {
    cpu.opcode = read6502(cpu.pc++);
    cpu.status |= FLAG_CONSTANT;

    penaltyop = 0;
    penaltyaddr = 0;

    (*addrtable[cpu.opcode])();
    (*optable[cpu.opcode])();
    cpu.clockticks6502 += ticktable[cpu.opcode];
    if (penaltyop && penaltyaddr) cpu.clockticks6502++;

    cpu.instructions++;

    if (callexternal) (*loopexternal)();
  }

}

void step6502() {
  cpu.opcode = read6502(cpu.pc++);
  cpu.status |= FLAG_CONSTANT;

  penaltyop = 0;
  penaltyaddr = 0;

  (*addrtable[cpu.opcode])();
  (*optable[cpu.opcode])();
  cpu.clockticks6502 += ticktable[cpu.opcode];
  if (penaltyop && penaltyaddr) cpu.clockticks6502++;
  cpu.clockgoal6502 = cpu.clockticks6502;

  cpu.instructions++;

  if (callexternal) (*loopexternal)();
}

void hookexternal(void *funcptr) {
  if (funcptr != (void *)NULL) {
    loopexternal = funcptr;
    callexternal = 1;
  } else callexternal = 0;
}
