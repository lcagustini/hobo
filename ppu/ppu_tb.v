`include "ppu.v"
`timescale 1us/1ns

module ppu_tb;

reg clk = 1'b0;
wire [7:0] color;
wire vblank;
wire hblank;

ppu ppu0
(
  clk,
  color,
  hblank,
  vblank
);

always #20
begin
  $display("V: %d H: %d -- %d", vblank, hblank, color);

  clk <= !clk;
end

endmodule
