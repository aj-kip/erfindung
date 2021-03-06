## Complete Table of Instructions, Pseudo-Instructions, and Directives
Aliases are comma delimited. Directives have no aliases. These instructions have further documentation available, which will follow a template. <br />
Also refer to important notes on how [immediates](#immd-notes) are handled. <br />
This document also contains information pertaining to assembler assumptions both [numeric](#numeric-assumptions) and io instruction related.

### Sample Instruction Template
<p>
Instruction Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Reg for register, Immd for immediate <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Description of behavior <br />
Further comments.
</p>

<style>
.nowrap { white-space: nowrap; }
</style>
<table>
<tr>
<th> Mnemonic  </th><th> Aliases </th>
<th> Mnemonic  </th><th> Aliases </th>
<th> Mnemonic  </th><th> Aliases </th>
</tr>

<tr>
<td> <a href="#and">and</a>       </td><td> &amp; </td>
<td> or       </td><td> | </td>
<td> xor       </td><td> ^ </td>
</tr>

<tr>
<td> not       </td><td> !, ~ </td>
<td> plus      </td><td> + </td>
<td> minus     </td><td> - </td>
</tr>
<tr>
<td> skip       </td><td> ? </td>
<td> save      </td><td> sav, &lt;&lt; </td>
<td> load     </td><td> ld, &gt;&gt; </td>
</tr>
<tr>
<td> set       </td><td> = </td>
<td> rotate      </td><td> rot, @ </td>
<td>      </td><td>  </td>
</tr>
<tr>
<td> io      </td><td>  </td>
<td> call      </td><td>  </td>
<td> jump     </td><td>  </td>
</tr>
<tr>
<td> times     </td><td> mul, multiply, &ast; </td>
<td> times-int       </td><td>
<span class="nowrap">mul-int</span>,
<span class="nowrap">multiply-int</span>,
<span class="nowrap">&ast;-int</span> </td>
<td> times-fp      </td><td>
<span class="nowrap">mul-fp</span>,
<span class="nowrap">multiply-fp</span>,
<span class="nowrap">&ast;-fp</span> </td>

</tr>
<tr>
<td> div    </td><td> divide, / </td>
<td> div-int       </td><td>
<span class="nowrap">divide-int</span>,
<span class="nowrap">/-int</span> </td>
<td> div-fp   </td><td>
<span class="nowrap">divide-fp</span>,
<span class="nowrap">/-fp</span> </td>

</tr>

<tr>
<td> comp     </td><td> compare, cmp, &lt;=&gt; </td>
<td> comp-int       </td><td>
<span class="nowrap">compare-int</span>,
<span class="nowrap">cmp-int</span>,
<span class="nowrap">&lt;=&gt;-int</span>
</td>
<td> comp-fp      </td><td>
<span class="nowrap">compare-fp</span>,
<span class="nowrap">cmp-fp</span>,
<span class="nowrap">&lt;=&gt;-fp</span>
</td>

</tr>
<tr>
<td> mod      </td><td> modulus, % </td>
<td> mod-int      </td><td>
<span class="nowrap">modulus-int</span>,
<span class="nowrap">%-fp</span>
 </td>
<td> mod-fp     </td><td>
<span class="nowrap">modulus-fp</span>,
<span class="nowrap">%-fp</span>
</td>
</tr>
<tr>
<td> push     </td><td>  </td>
<td> pop     </td><td>  </td>
<td>        </td><td>  </td>
</tr>
<tr>
<th> Directive </th><th>  </th>
<th> Directive </th><th>  </th>
<th> Directive </th><th>  </th>
</tr>
<tr>
<td> data       </td><td>  </td>
<td> :     </td><td> (labels) </td>
<td> #     </td><td> (comments) </td>
</tr>

<tr>
<td> assume       </td><td>  </td>
<td>      </td><td>  </td>
<td>      </td><td>  </td>
</tr>

</table>

<h3 id="numeric-assumptions">Numeric Assumptions, important notes</h3>
<p>
The Erfindung CPU can perform operations on both integers and fixed point numbers. Bitwise operations, addition, and subtraction all use the same implementation regardless whether the data is to be treated as fixed point numbers or integers. Other operations where fixed point numbers and integers need separate implementations, have suffixes ('-fp' and '-int').
</p><p>
Assumption directives have the role of eliminating the need for these suffixes. Do note however, if there's an immediate, that immediate's 'type' will override any assumption that was directed. So a <em>times x a 0.8</em> will perform a fixed point operation regardless of a previous <em>assume integer</em>.</p>

Assume Integer directive:
<pre>assume int</pre><pre>assume integer</pre>
Assume Fixed Point directive:
<pre>assume fp</pre><pre>assume fixed-point</pre>

You may also direct the assembler to make no assumptions. (careful this will affect io instruction assumptions also!)
<pre>assume none</pre><pre>assume nothing</pre>

<h3 id="immd-notes">Immediates, important notes</h3>
<p>
The assembler supports several forms of number formats for immediates. The common hexidecimal prefix "0x", binary "0b", and regular decimal. <br />
On the type of data: if a "." (decimal point) is present, the assembler will interpret the value as a fixed point always. Without this decimal point, it will always be interpreted as an integer. Note that a numeric immediate will override any 'type' assumption.
</p>
<p>See also: [Numeric Assumptions](#numeric-assumptions)</p>

<h3 id="and">and</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs bitwise AND as a series of bits from the last two operands and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands.
</p>
<h3 id="or">or</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs bitwise OR as a series of bits from the last two operands and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands.
</p>

<h3 id="xor">xor</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs bitwise XOR (exclusive or) as a series of bits from the last two operands and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands.
</p>

<h3 id="not">not</h3>
<p>
U-Type <br />
Parameters: Reg (Reg) <br />
Real instruction forms: <ul>
<li>Reg Reg </li>
</ul>
Performs bitwise complement as a series of bits from the second operand and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands.
</p>

<h3 id="plus">plus</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs integer or fixed point addition on the last two operands and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands. The effects will be semantically correct for both integers and fixed points, though the implementation itself is the same.
</p>

<h3 id="minus">minus</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs integer or fixed point subtraction on the last two operands and stores the result in the first operand.<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands. The effects will be semantically correct for both integers and fixed points, though the implementation itself is the same.
</p>

<h3 id="skip">skip</h3>
<p>
J-Type <br />
Parameters: Reg (Immd/Label/Special Label)<br />
Real instruction forms: <ul>
<li>Reg </li>
<li>Reg Immd</li>
</ul>
If the first operand is non-zero or the result of bitwise AND between first operand and the second operand is non-zero, then the following instruction will be skipped. <br />
Note that if the line following the this skip is a pseudo-instruction that does/may not produce only one instruction, the assembler will generate a warning. The assembler will not stop you from only skipping part of a pseudo-instruction.<br />
One example of a pseudo-instruction that may emit more than one real instruction is "[io](#io)".
Skip is the principle jump type instruction, inspired by CHIP8's SKIP.
</p>

<h3 id="save">save</h3>
<p>
M-Type <br />
Parameters: Reg Reg/Immd/Label <br />
Reg Reg (Immd/Label)<br />
Real instruction forms: <ul>
<li>Reg Reg Immd</li>
<li>Reg Reg</li>
<li>Reg Immd</li>
</ul>
Saves the data in first operand's data (register) to the address specified by the second operand. Optionally if the second operand is a register and there is a third immediate operand, it will be used as an integer offset. Fixed points immediates are not available for this instruction.<br />
<em>Special case:</em> If the second operand is an immediate rather than a register, then it'll be decoded not as an integer immediate, but rather as an address. The 16 bit address immediate is mapped to a 32 bit value as follows:
</p>

| most significant bit | least significant bits |
|:---------------------|------------------------|
| bit 15 to            | bits 14-0 to           |
| bit 31               | bits 14-0              |

Or alternatively visualized as: <br />
16 bit immediate: 0bNXXX XXXX XXXX XXXX<br />
32 bit used value: 0bN000 0000 0000 0000 0000 XXXX XXXX XXXX<br />

<h3 id="load">load</h3>
<p>
M-Type <br />
Parameters: Reg Reg/Immd/Label <br />
Reg Reg (Immd/Label)<br />
Real instruction forms: <ul>
<li>Reg Reg Immd</li>
<li>Reg Reg</li>
<li>Reg Immd</li>
</ul>
Loads data from the address in the second operand into the first operand. If the second operand is a register, then there may optionally be a third integer immediate operand to act as an offset.<br />
<em>Special case:</em> If the second operand is an immediate rather than a register, then it'll be decoded not as an integer immediate, but rather as an address. The 16 bit address immediate is mapped to a 32 bit value as follows:
</p>

| most significant bit | least significant bits |
|:---------------------|------------------------|
| bit 15 to            | bits 14-0 to           |
| bit 31               | bits 14-0              |

Or alternatively visualized as: <br />
16 bit immediate: 0bNXXX XXXX XXXX XXXX<br />
32 bit used value: 0bN000 0000 0000 0000 0000 XXXX XXXX XXXX<br />

<h3 id="set">set</h3>
<p>
M-Type<br />
Parameters: Reg Reg/Immd/Label<br />
Real instruction forms: <ul>
<li>Reg Reg</li>
<li>Reg Immd</li>
</ul>
Moves the contents of the second operand into the first operand.
</p>

<h3 id="rotate">rotate</h3>
<p>
R-Type <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
If there are three arguments the second argument is rotated, with the third being used as an integer for how many places to rotate. If there are two arguments, the first argument is rotated, with the second used as "how many". The result is placed into the first operand.<br />
Negative integers rotate the bits left, while positive rotates to the right. <br />
This instruction only accepts "integers" and the assembler will issue a warning if the fixed point assumption is active at the time this instruction is requested.
</p>

<h3 id="io">io</h3>
<p>
Pure pseudo-instruction<br />
Parameters: <ul>
<li>read controller/timer/random/gpu/bus-error Reg ...</li>
<li>upload Reg Reg Reg Reg</li>
<li>clear Reg </li>
<li>draw Reg Reg Reg</li>
<li>halt Reg</li>
<li>wait Reg </li>
<li>triangle Reg Immd ...</li>
<li>pulse one/two Reg Immd ...</li>
<li>noise Reg Immd ...</li>
</ul>
This psuedo-instruction emits some number of real set, save, and/or load instructions. Since these may emit many real instructions, it is not recommended that "io" follow and "skip", and the assembler will issue a warning as such (but will not fail the compilation).<br />
Since this psuedo-instruction has so many variants, documentation for it is split up.
</p>

<h3 id="io-read">io read</h3>

<p>
Pure pseudo-instruction<br />
Parameters: DEVICE Reg ... <br />
This psuedo-instruction reads from a memory mapped addressed specified by a device string into the given registers.<br />
As the ellipse implies, the programmer may do a device read and send it to multiple registers at once.
<pre>io read controller x y</pre>
This will read the controller device twice, sending the read data to each register. Note that if reading a device has side effects, then this is not desired, you may mitigate this by using an additional set. It maybe desirable that a device is read multiple times and those values sent to separate registers. For instance:
<pre>io read random x y</pre>
This will load what is very likely to be two different randomly generated values to both x and y in a single line.<br />
Possible devices are as follows: <ul>
<li>controller -> the user input device</li>
<li>timer      -> timer device</li>
<li>random     -> psuedo-random number</li>
<li>gpu        -> GPU output stream</li>
<li>bus-error  -> Bus Error flags, "super special register"</li>
</ul>
<h4>io read controller</h4>
<p>Erfindung's controller is very simple, there are only seven buttons. When reading the controller the buttons which are currently being depressed are corresponded to there own bit-field.</p>
</p>

| bits 31-7 | bit 6 | bit 5 | bit 4 | bit 3 | bit 2| bit 1 | bit 0 |
|-----------|-------|-------|-------|-------|------|-------|-------|
| (unused)  | Start | B     | A     | Right | Left | Down  | Up    |

This read has no side-effects.

<h4>io read timer</h4>
<p>When the timer is read, the value is a 32-bit fixed point number of elapsed time in seconds since the last wait was issue.</p>
This read has no side-effects. <br />
<h4>io read random</h4>
<p>Reads a 32-bit series of randomly generated bits.</p>
This read has a side-effect, reading will cause the PRNG to generate a new psuedo-random value. <br />
<h4>io read gpu</h4>
<p>It is not recommended to read from the GPU output stream. It is currently an unused device, and maybe removed in future versions of this interpreter.</p>
<h4>bus-error</h4>
<p>This is a special flag on the control unit, if it is set than an illegal read or write operation has occurred. If the flag is set, the reading will set the destination register's least significant bit to 1.</p>
<p>Unfortunately there is no direct way to clear the flag at the time of this writing.</p>

### call
### jump
### times
### times-int
### times-fp
### div
### div-int
### div-fp
### comp
### comp-int
### comp-fp
### mod
### mod-int
### mod-fp
### push
### pop
### data
### : (labels)
### # (comments)
### assume
<p>
Assume directives cover [numeric assumptions](#numeric-assumptions) and io related assumptions.
</p>
