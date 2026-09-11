/* The complete model travels inside the EFI executable, already in RAM. */
.section .rodata.payload,"a",@progbits
.balign 64
.global bm_payload_start, bm_payload_end
.hidden bm_payload_start, bm_payload_end
bm_payload_start:
	.incbin ".build/payload.bin"
bm_payload_end:
.section .note.GNU-stack,"",@progbits
