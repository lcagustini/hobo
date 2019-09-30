typedef struct {
  uint64_t id;
  //6502 CPU registers
  uint16_t pc;
  uint8_t sp, a, x, y, status;


  //helper variables
  uint32_t instructions; //keep track of total instructions executed
  uint32_t clockticks6502, clockgoal6502;
  uint16_t oldpc, ea, reladdr, value, result;
  uint8_t opcode, oldstatus;
} CPU_State;
