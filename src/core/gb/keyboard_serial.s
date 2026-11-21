        .include "global.s"

        .area _HOME

_keyboard_serial_receive::
#ifdef PS2_KEYBOARD
        .globl _kb_raw, _kb_raw_errors, _kb_raw_read, _kb_raw_write, _kb_raw_fault

        ; Fixed-bank receiver; interrupts remain masked through the PS/2 frame.
        xor a
        ld (_kb_rx_error), a
        ld (_kb_rx_code), a
        ldh a, (0x01)
        ld b, a;store first 8 bits of scan code (S0123456)

kb_rx_testStartBit:
        bit 7, b;test start bit, should always be 0
        jr z, kb_rx_waitForLastBits
kb_rx_startBitError:
        ld a, #0x04
        ld (_kb_rx_error), a
        jp kb_rx_queue_frame;bail early if start bit not 0

kb_rx_waitForLastBits:
        ld a, #0x80
        ldh (0x02), a      ;ask for more bits using keyboard clock
        xor a
        ld (_kb_rx_error), a ;reset current error code
        ld a, b          ;S0123456
        swap a           ;3456S012
        rrca             ;23456S01
        and #249 ;23456xxx, bit 7 and parity unknown
        or #1 ;23456xxF, finish bit is always 1
        ld c, a          ;expected value after shift
        ld e, #255          ;timer
kb_rx_pollBitsLoop:
        ldh a, (0x01)
        ld d, a;actual value
        and #249;exit condition
        cp c;if bits & exit == expected, done
        jr z, kb_rx_allBitsRead
        dec e
        jr z, kb_rx_timeout
        jr kb_rx_pollBitsLoop
kb_rx_timeout:
        ld a, (_kb_rx_error)
        or #0x20
        ld (_kb_rx_error), a

kb_rx_allBitsRead:
        ld c, d;replace incomplete expected value with actual value (234567PF)
        ld a, #0x01
        ldh (0x02), a;stop asking for stuff, use internal clock

kb_rx_testFinishBit:
        bit 0, c;test finish bit, should always be 1
        jr nz, kb_rx_storeParityBit
kb_rx_finishBitError:
        ld a, (_kb_rx_error)
        or #0x01
        ld (_kb_rx_error), a

kb_rx_storeParityBit:
        ld a, c
        srl a;bit 0 is parity
        and #1
        ld e, a

kb_rx_storeDataBits:
        sla b  ;0123456x
        ld a, c;234567PF
        srl a  ;x234567P
        srl a  ;xx234567
        or b;01234567
        ld d, a;data bits

        ; ;data (d) is 01234567, needs to be 76543210 in a
        ; ;we can count bits as we do so to test Parity Bit
kb_rx_reverseAndCountDataBits:
        ld b, #8;bits left
        ld c, #0;bit sum
kb_rx_reverseAndCountLoop:
        srl d;shift data right out of d into carry
        rl a;rotate data left from carry into a
        bit 0, a
        jr z, kb_rx_decrementBitsLeft
kb_rx_addBit:
        inc c;if new bit is 1, increment sum
kb_rx_decrementBitsLeft:
        dec b;bits left
        jr nz, kb_rx_reverseAndCountLoop
kb_rx_storeScanCode:
        ld (_kb_rx_code), a

kb_rx_testParityBit:
        ld a, c;bit sum
        and #1
        cp e;bit 0 of e is parity, and bit 0 of a is even/odd. These should never be equal.
        jr nz, kb_rx_queue_frame
kb_rx_parityBitError:
        ld a, (_kb_rx_error)
        or #0x02
        ld (_kb_rx_error), a

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
        ld a, (_kb_rx_code)
        ld (hl), a
        ld hl, #_kb_raw_errors
        add hl, bc
        ld a, (_kb_rx_error)
        ld (hl), a
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
        ldh (0x01), a
        ld a, #0x80
        ldh (0x02), a
        ret

        .area _DATA
_kb_rx_error:
        .ds 1
_kb_rx_code:
        .ds 1
#else
        ret
#endif
