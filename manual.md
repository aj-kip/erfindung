Erfindung Manual
================

This document defines Erfindung ISA/interpreter and acts as the manual.

I will endeavor to keep this document as concise as possible.

Target features:
 + RISC + some bells and whistles
 + assemble to C++ and dynamically executable byte code (shelved)
 + parts of the virtual machine can operate independently from each other
 + You can run programs using only the memory space and CPU
 + You can upload instructions to the APU or GPU and expect the objects
   to produce proper output and be well behaved.

Erfindung Assembly Language
---------------------------

### Introduction
Erfindung is designed to handle two different types of numbers:
 + unsigned 32 bit integers (also used for pointers)
 + two's complement 32 bit fixed point real numbers

Positive integers may be used as pointers.

Each instruction is strictly 32bits.
There are 8 32bit registers.


Erfindung has instructions that can handle fixed point math.

Erfindung assembly language: is a superset of the provided hardware
instructions.
Terms:
 + Hardware instruction: an instruction defined by the ISA
 + pseudo instruction: an instruction NOT defined by the ISA, but can be
formed from a combination of one or more hardware instructions.

Complete Memory Map
-------------------

| Region                     | Address                    |
|:---------------------------|:---------------------------|
| Device I/O                 | 0x0000 0000 - 0x0000 0010  |
| Program Code               | 0x0100 ????                |
| Free Memory for Stack/Heap | 0x0100 ???? + program size |

Memory Mapped I/O Devices
-------------------------

### Stop Space/Reserved null
Address: 0x0000 0000

|0   -   31|
|----------|
|          |


### GPU
Addresses: 0x0000 0001 - 0x0000 0002

|0 -                      31|32 -              63|
|---------------------------|--------------------|
| write-only command stream | command output ROM |


Command Indentities <br/>
Commands are uploaded to the GPU, first with the command indentity
(a special ISA defined number), followed by its parameters (assume each are
32bit integers, unless specified otherwise).

UPLOAD_SPRITE
 - parameters: width, height, address
 - answers   : (in output ROM, available after write) index for sprite

UNLOAD_SPRITE
 - parameters: index for sprite

DRAW_SPRITE
 - parameters: x pos, y pox, index for sprite

SCREEN_CLEAR
 - takes no parameters

### APU
Addresses (0x0000 0003 - 0x0000 0004)

|0 -                      31|32 -        63|
|---------------------------|--------------|
| write-only command stream | reserved ROM |

### Timer
(Addresses 0x0000 0005 - 0x0000 0006)

|0 - 30|31 -                    31|32 -                  63|
|------|--------------------------|------------------------|
| ...  | wait & sync (write-only) | query duration (fp/ROM) |

### RNG
(Address 0x0000 0007)

|0                  31|
|---------------------|
| random number (ROM) |

### Controller
(Address 0x0000 0008)
All are ROM

|0 - 0|1 -  1|2 -  2|3 -   3|4 - 4|5 - 5|6 -   6|7 - 31|
|-----|------|------|-------|-----|-----|-------|------|
| UP  | DOWN | LEFT | RIGHT | A   | B   | START | ...  |

### Power
(Address 0x0000 0009)

|0 - 30|31 -      31|
|------|------------|
| ...  | is powered |

Flip to zero to HALT the machine

### Bus Error Data
(Address 0x0000 0x000A)

|0 -           31|
|----------------|
| Bus Error Code |

Bus Errors:
 - no error
 - write on ROM
 - read on write-only
 - queue overflow
 - invalid address access

Split bits by device.

### Reserved Addresses
(0x0000 000B - 0x0000 0010)
