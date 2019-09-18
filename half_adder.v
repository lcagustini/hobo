///////////////////////////////////////////////////////////////////////////////
// File Downloaded from http://www.nandland.com
///////////////////////////////////////////////////////////////////////////////
module half_adder
(
  i_bit1,
  i_bit2,
  o_sum,
  o_carry
);

input  i_bit1;
input  i_bit2;
output o_sum;
output o_carry;

assign o_sum   = i_bit1 ^ i_bit2;  // bitwise xor
assign o_carry = i_bit1 & i_bit2;  // bitwise and

endmodule // half_adder

module half_adder_tb;

reg r_BIT1 = 0;
reg r_BIT2 = 0;
wire w_SUM;
wire w_CARRY;

half_adder half_adder_inst
(
  .i_bit1(r_BIT1),
  .i_bit2(r_BIT2),
  .o_sum(w_SUM),
  .o_carry(w_CARRY)
);

initial
begin
  r_BIT1 = 1'b0;
  r_BIT2 = 1'b0;
  #10;
  $display("%d%d", w_CARRY, w_SUM);
  r_BIT1 = 1'b0;
  r_BIT2 = 1'b1;
  #10;
  $display("%d%d", w_CARRY, w_SUM);
  r_BIT1 = 1'b1;
  r_BIT2 = 1'b0;
  #10;
  $display("%d%d", w_CARRY, w_SUM);
  r_BIT1 = 1'b1;
  r_BIT2 = 1'b1;
  #10;
  $display("%d%d", w_CARRY, w_SUM);
end

endmodule // half_adder_tb
