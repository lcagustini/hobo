module vram
(
  input clk,
  input we,
  input [9:0] addr,
  input [7:0] data,
  output [7:0] out
);

reg [7:0] memory[1023:0];
reg [5:0] addr_reg;

always @ (posedge clk)
begin
  if (we)
    memory[addr] <= data;

  addr_reg <= addr;
end

assign out = memory[addr_reg];

endmodule

module vram_tb;

reg [9:0] addrs;
reg [7:0] byte;
reg we;
reg clk;
wire [7:0] out;

vram main_vram
(
  clk,
  we,
  addrs,
  byte,
  out
);

always #20 clk <= !clk;

initial
begin
  byte = 8'b10;
  addrs = 10'b100;
  we = 1;
  #40;
  addrs = 10'b100;
  we = 0;
  #40;
  $display("%d", out);
end

endmodule
