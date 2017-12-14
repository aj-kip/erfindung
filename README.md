Erfindung
=========

Fictitious ISA/ASM-like Interpreter
-----------------------------------

![Travis CI Build Status](https://travis-ci.org/aj-kip/erfindung.svg?branch=master)
[![CodeFactor](https://www.codefactor.io/repository/github/aj-kip/erfindung/badge/master)](https://www.codefactor.io/repository/github/aj-kip/erfindung/overview/master)

Erfindung (or &quot;Invention&quot; in German), is a ASM-like, GameBoy-esque
virtual machine and interpreter.
It is intended to be a toy assembly language/virtual console.
A comprehensive manual is available [here](manual/index.md)

Development Progress
--------------------
This project is mostly finished, which is not to say it cannot be extended in the future.
Here's what remains:
+ more comprehensive unit testing (APU, GPU)
+ cleaner / more documentation
+ demo programs

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
