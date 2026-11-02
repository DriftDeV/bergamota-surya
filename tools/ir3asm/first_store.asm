@localsize 1, 1, 1
@buf 4  ; g[0]
mov.u32u32 r0.x, 0xcafe0001
mov.u32u32 r0.y, 0x00000000
(rpt5)nop
stib.b.untyped.1d.u32.1.imm r0.x, r0.y, 0
end
nop
