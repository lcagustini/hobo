.segment "CODE"
reset:
  ldx #$FF
  txs

  ldx #$FF
loop:
  stx $2000
  dex
  jmp loop
