module vram
(
  input clk,
  input we,
  input [16:0] addr,
  input [7:0] data,
  output [7:0] out
);

reg [7:0] memory[131071:0];
reg [5:0] addr_reg;

always @ (posedge clk)
begin
  if (we)
    memory[addr] <= data;

  addr_reg <= addr;
end

assign out = memory[addr_reg];

endmodule
