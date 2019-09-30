`include "vram.v"
`timescale 1us/1ns

module vram_tb;

reg [9:0] addrs = 1'b0;
reg [7:0] byte = 1'b0;
reg we = 1'b0;
reg clk = 1'b0;
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

  byte = 8'b11;
  addrs = 10'b101;
  we = 1;
  #40;
  addrs = 10'b101;
  we = 0;
  #40;
  $display("%d", out);

  addrs = 10'b100;
  #40;
  $display("%d", out);
end

endmodule
