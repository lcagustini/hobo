`include "vram.v"

module ppu
(
  input clk,
  output [7:0] color,
  output hblank,
  output vblank
);

reg [16:0] total_count = 17'b0;
reg [8:0] pixel_count = 9'b0;
reg [7:0] line_count = 8'b0;

reg [9:0] vram_addrs = 1'b0;
reg [7:0] vram_byte = 1'b0;
reg vram_we = 1'b0;

reg vblank;
reg hblank;

vram main_vram
(
  clk,
  vram_we,
  total_count,
  vram_byte,
  color
);

always @ (posedge clk)
begin
  total_count <= total_count + 1;
  pixel_count <= pixel_count + 1;

  if (pixel_count == 320)
  begin
    line_count <= line_count + 1;
    pixel_count <= 9'b0;
    hblank <= 1'b1;
  end
  else
    hblank <= 1'b0;

  if (line_count == 240)
  begin
    line_count <= 8'b0;
    vblank <= 1'b1;
  end
  else
    vblank <= 1'b0;

  if (total_count == 76800)
    total_count <= 1'b0;
end

endmodule
