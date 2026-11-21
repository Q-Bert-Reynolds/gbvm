        .include "global.s"

        .area _HOME

_keyboard_serial_receive::
#ifdef PS2_KEYBOARD
        .globl _kb_raw, _kb_raw_errors, _kb_raw_read, _kb_raw_write, _kb_raw_fault

        ; Fixed-bank receiver; interrupts remain masked through the PS/2 frame.
        ; A PS/2 frame is 11 bits: start(0) d0..d7 parity(odd) stop(1).
        ; On entry SB holds the first 8: S d0 d1 d2 d3 d4 d5 d6 (bit 7 = S).
        ldh a, (.SB)
        ld b, a

        ; Collect d7, parity and stop. SB is free now, so seed it with a
        ; marker bit: after exactly 3 shifts SB == 0b00001 d7 P F, which no
        ; other shift count can produce.
        ld a, #0x01
        ldh (.SB), a
        ld a, #0x80
        ldh (.SC), a      ; external clock: ask for more bits
        ld e, #255        ; timeout
kb_rx_pollBitsLoop:
        ldh a, (.SB)
        ld c, a
        and #0xF8
        cp #0x08
        jr z, kb_rx_allBitsRead
        dec e
        jr nz, kb_rx_pollBitsLoop

        ; Timeout: bits were lost (e.g. the ISR ran too late for bit 9) and
        ; the keyboard has since gone idle, so we are already at a frame
        ; boundary and need no drain.
        ld a, #0x01
        ldh (.SC), a      ; stop, internal clock
        ld d, #0          ; code
        ld e, #0x20       ; KB_ERROR_TIMEOUT
        jr kb_rx_queue_frame

kb_rx_allBitsRead:
        ld a, #0x01
        ldh (.SC), a      ; stop, internal clock
        ld e, #0          ; error flags

        bit 7, b          ; start bit must be 0
        jr z, kb_rx_testFinishBit
        ld e, #0x04       ; KB_ERROR_START
kb_rx_testFinishBit:
        bit 0, c          ; stop bit must be 1
        jr nz, kb_rx_buildData
        set 0, e          ; KB_ERROR_STOP

kb_rx_buildData:
        sla b             ; b = d0 d1 d2 d3 d4 d5 d6 0
        bit 2, c
        jr z, kb_rx_reverseAndCount
        set 0, b          ; b = d0 d1 d2 d3 d4 d5 d6 d7

        ; Reverse b into a (d7..d0) while counting set bits in h.
kb_rx_reverseAndCount:
        ld d, #8          ; bits left
        ld h, #0          ; bit sum
kb_rx_reverseAndCountLoop:
        srl b             ; next data bit -> carry
        jr nc, kb_rx_rotateBit
        inc h             ; inc leaves carry intact
kb_rx_rotateBit:
        rla               ; carry -> a
        dec d
        jr nz, kb_rx_reverseAndCountLoop
        ld d, a           ; scan code

        ; Odd parity: (bit sum + P) must be odd.
        ld a, c
        rrca              ; bit 0 = P
        xor h
        and #1
        jr nz, kb_rx_checkErrors
        set 1, e          ; KB_ERROR_PARITY

kb_rx_checkErrors:
        ld a, e
        or a
        jr z, kb_rx_queue_frame

        ; Framing/parity error: we may be misaligned with the keyboard's
        ; frames. Discard bits until the clock stays quiet for longer than a
        ; PS/2 bit period (<= 100us), which can only happen between frames.
        ; One idle-loop pass is 16 M-cycles; 24 passes is ~367us on DMG and
        ; ~183us in CGB double speed. h caps restarts so a noisy clock line
        ; cannot hang the CPU.
        ld h, #0
kb_rx_drainRestart:
        ld l, #24
        ld a, #0x01
        ldh (.SB), a
        ld a, #0x80
        ldh (.SC), a
kb_rx_drainLoop:
        ldh a, (.SB)
        dec a
        jr nz, kb_rx_drainBit     ; marker moved: a bit arrived
        ldh a, (.SC)
        rlca
        jr nc, kb_rx_drainBit     ; transfer completed: re-arm
        dec l
        jr nz, kb_rx_drainLoop
        jr kb_rx_drainDone
kb_rx_drainBit:
        dec h
        jr nz, kb_rx_drainRestart
kb_rx_drainDone:
        ld a, #0x01
        ldh (.SC), a
        ldh a, (.IF)
        and #0xF7         ; drop any transfer-complete flag raised while draining
        ldh (.IF), a

        ; d = scan code, e = error flags
kb_rx_queue_frame:
        ld a, (_kb_raw_write)
        ld c, a
        inc a
        and #31
        ld b, a
        ld a, (_kb_raw_read)
        cp b
        jr z, kb_rx_overflow
        ld b, #0
        ld hl, #_kb_raw
        add hl, bc
        ld (hl), d
        ld hl, #_kb_raw_errors
        add hl, bc
        ld (hl), e
        ld a, c
        inc a
        and #31
        ld (_kb_raw_write), a
        jr kb_rx_arm
kb_rx_overflow:
        ld a, (_kb_raw_fault)
        or #0x80
        ld (_kb_raw_fault), a
kb_rx_arm:
        xor a
        ldh (.SB), a
        ld a, #0x80
        ldh (.SC), a
        ret
#else
        ret
#endif
