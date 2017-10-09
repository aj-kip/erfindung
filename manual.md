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
 + Positive integers may be used as pointers.
 + Each instruction is strictly 32bits.
 + There are 8 32bit registers.
 + Erfindung has instructions that can handle fixed point math.

Erfindung assembly language: is a superset of the provided hardware
instructions.
Terms:
 + Hardware instruction: an instruction defined by the ISA
 + pseudo instruction: an instruction NOT defined by the ISA, but can be
formed from a combination of one or more hardware instructions.

Sample programs are available in the project's root directory.

### Instruction List

These are the so called "real" instructions that directly correspond to byte code in the virtual machine. Note that some of these instructions have additional parameter forms than listed on this table. They will be covered in Pseudo-Instructions section.

All the following instructions store their answer into the first register.

| Mnemonic  | Parameters |Description                |
|:----------|:---------------------------|            |
| and       | Reg reg (reg/fp/int/label)          |  bitwise operations treat registers as a "bag of bits"                |
| or        |  Reg reg (reg/fp/int/label)          | |
| xor       | Reg reg (reg/fp/int/label)          | |
| not       | Reg           | register has its bits flipped |
| plus      | Reg reg (reg/fp/int/label)          |  covers both fixed point and integers|
| minus     | Reg reg (reg/fp/int/label)          |  covers both fixed point and integers|
| times-int | Reg reg (reg/int/label)           | integers only (as per suffix) |
| times-fp  | Reg reg (reg/fp/label)           | fixed points only (as per suffix) |
| div-int
| div-fp
| mod-int
| mod-fp
| comp-int | | The compare operation behaves very similarly to the other arithmetic operations. This operation produces a four-bit response of flags, where each denotes  |
| comp-fp
| skip
| save
| load
| call

### Pseudo-Instructions
+ times
+ div
+ mod
+ comp
+ jump
+ io
+ push
+ pop

Complete Memory Map
-------------------

| Region                     | Address                    |
|:---------------------------|:---------------------------|
| Device I/O                 | 0x8000 0000 - 0xFFFFFFFF   |
| Program Code               | 0x0000 0000                |
| Free Memory for Stack/Heap | 0x0000 0000 + program size |

Memory Mapped I/O Devices
-------------------------

### Stop Space/Reserved null
Address: 0x8000 0000

|0   -   31|
|----------|
|          |


### GPU
Addresses: 0x8000 0001 - 0x8000 0002

|0 -                      31|32 -              63|
|---------------------------|--------------------|
| write-only command stream | command output ROM |


Command Identities <br/>
Commands are uploaded to the GPU, first with the command identity
(a special ISA defined number), followed by its parameters (assume each are
32bit integers, unless specified otherwise).

UPLOAD_SPRITE
 - parameters: width, height, address, index
 - answers   : (in output ROM, available after write) index for sprite

sprite/texture quad indices
These are organized as a set of nested quads (four at each layer)
They are best described graphically...

Each quad is further sub-divided into even smaller quads, terminating at 8x8 quads.

<table>
<tr><td>0</td><td>1</td></tr>
<tr><td>2</td><td>3</td></tr>
</table>

This means there are:
- 4    "mega"   (128 px) quads
- 16   "large"   (64 px) quads
- 64   "medium"  (32 px) quads
- 256  "small"   (16 px) quads
- 1024 "mini"     (8 px) quads

At the highest level (the entire reserved memory for texture) looks like this:

<table><tr><th> </th><th>128px</th><th>128px</th></tr><tr><th>128px</th><td><table><tr><th>0</th><th>64px</th><th>64px</th></tr><tr><th>64px</th><td>0</td><td>1</td></tr><tr><th>64px</th><td>2</td><td>3</td></tr></table></td><td><table><tr><th>1</th><th>64px</th><th>64px</th></tr><tr><th>64px</th><td>0</td><td>1</td></tr><tr><th>64px</th><td>2</td><td>3</td></tr></table></td></tr><tr><th>128px</th><td><table><tr><th>2</th><th>64px</th><th>64px</th></tr><tr><th>64px</th><td>0</td><td>1</td></tr><tr><th>64px</th><td>2</td><td>3</td></tr></table></td><td><table><tr><th>3</th><th>64px</th><th>64px</th></tr><tr><th>64px</th><td>0</td><td>1</td></tr><tr><th>64px</th><td>2</td><td>3</td></tr></table></td></tr></table>

The complete table is much too large to include here.
If you're curious you can run the following on your local Unix-like machine, provided bcat and lua are installed:
<pre>lua table-gen.lua | bcat</pre>

|31 -  13|12 -      10|9 -                      0|
|--------|------------|--------------------------|
| unused | size value | bit sets, 2 bits a piece |

size value: means how many bit sets are needed to address the texture quad

DRAW_SPRITE
 - parameters: x pos, y pox, index for sprite

SCREEN_CLEAR
 - takes no parameters

### APU
Addresses (0x8000 0003 - 0x8000 0004)

|0 -                      31|32 -        63|
|---------------------------|--------------|
| write-only command stream | reserved ROM |

### Timer
(Addresses 0x8000 0005 - 0x8000 0006)

|0 - 30|31 -                    31|32 -                  63|
|------|--------------------------|------------------------|
| ...  | wait & sync (write-only) | query duration (fp/ROM) |

### RNG
(Address 0x8000 0007)

|0                  31|
|---------------------|
| random number (ROM) |

### Controller
(Address 0x8000 0008)
All are ROM

|0 - 0|1 -  1|2 -  2|3 -   3|4 - 4|5 - 5|6 -   6|7 - 31|
|-----|------|------|-------|-----|-----|-------|------|
| UP  | DOWN | LEFT | RIGHT | A   | B   | START | ...  |

### Power
(Address 0x8000 0009)

|0 - 30|31 -      31|
|------|------------|
| ...  | is powered |

Flip to zero to HALT the machine

### Bus Error Data
(Address 0x8000 0x000A)

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
(0x8000 000B - 0xFFFF FFFF)
