Erfindung
=========

Fictitious ISA/ASM-like Interpreter
-----------------------------------

Erfindung (or &quot;Invention&quot; in German), is a ASM-like, GameBoy-esque 
virtual machine and interpreter.
It is intended to be a toy assembly language/virtual console.

Development Progress
--------------------
Design-wise: Design is looking pretty good!

Implementation-wise: just the noise channel, proper documentation, and testing!

My motivation for this project comes from several sources:
--------------------------------------------------------------------
+ frustration with the complexity of Intel x86
+ complexity of trying to implement an emulator (even NES)
+ wanting to design my own language without getting into complex grammar
+ disappointment in my own performance in university u.u

Primary Features of this &quot;ISA&quot;:
---------------------------------------------------------------------
+ RISC (Similar to CHIP8)
+ Monochromatic Screen
+ Abstracted GPU and APU
  + No weird &quot;super low level&quot; instructions
  + describe notes, pitches of notes, which channel directly for the APU
  + describe sprite data, positions, sizes, off-screen video memory for the GPU

This project's design is still in development for a large part.
------------------------
There are only a few things in the project which are &quot;stable&quot;.


+ Most of the Erfindung Assembly Language
  + The design of I/O may change
  + All R-types and J-types
+ C++ Abstractions for the GPU, CPU, fixed point math and the Assembler
