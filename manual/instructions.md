## Complete Table of Instructions, Pseudo-Instructions, and Directives
Aliases are comma delimited. Directives have no aliases. These instructions have further documentation available, which will follow a template. <br />
Also refer to important notes on how [immediates](#immd-notes) are handled. <br />
This document also contains information pertaining to assembler assumptions both [numeric](#numeric-assumptions) and io instruction related.
<strong>Super Important Note:</strong> if it is found that the software fails to follow the documentation, then the documentation takes precedence, and the misbehavior is classed as a bug. (however with this document being in beta/incomplete, an error <em>may</em> occur in the documents!)
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
<th> R-Types  </th><th> Aliases </th>
<th> R-Types  </th><th> Aliases </th>
<th> R-Types  </th><th> Aliases </th>
</tr>

<tr>
<td> <a href="#and">and</a>       </td><td> &amp; </td>
<td> <a href="#or">or</a>       </td><td> | </td>
<td> <a href="#xor">xor</a>       </td><td> ^ </td>
</tr>
<tr>
<td> <a href="#plus">plus</a>      </td><td> + </td>
<td> <a href="#minus">minus</a>     </td><td> - </td>
<td> <a href="#rotate">rotate </a>     </td><td> rot, @ </td>
</tr>
<tr>
<td> <a href="#times"> times</a>     </td><td> mul, multiply, &ast; </td>
<td> <a href="#times-int"> times-int</a>       </td><td>
<span class="nowrap">mul-int</span>,
<span class="nowrap">multiply-int</span>,
<span class="nowrap">&ast;-int</span> </td>
<td> <a href="#times-fp"> times-fp</a>      </td><td>
<span class="nowrap">mul-fp</span>,
<span class="nowrap">multiply-fp</span>,
<span class="nowrap">&ast;-fp</span> </td>

</tr>
<tr>
<td> <a href="#div"> div</a>    </td><td> divide, / </td>
<td> <a href="#div-int"> div-int</a>       </td><td>
<span class="nowrap">divide-int</span>,
<span class="nowrap">/-int</span> </td>
<td> <a href="#div-fp"> div-fp</a>   </td><td>
<span class="nowrap">divide-fp</span>,
<span class="nowrap">/-fp</span> </td>

</tr>

<tr>
<td> <a href="#comp"> comp</a>     </td><td> compare, cmp, &lt;=&gt; </td>
<td> <a href="#comp-int"> comp-int</a>       </td><td>
<span class="nowrap">compare-int</span>,
<span class="nowrap">cmp-int</span>,
<span class="nowrap">&lt;=&gt;-int</span>
</td>
<td> <a href="#comp-fp">comp-fp</a>      </td><td>
<span class="nowrap">compare-fp</span>,
<span class="nowrap">cmp-fp</span>,
<span class="nowrap">&lt;=&gt;-fp</span>
</td>

</tr>
<tr>
<td> <a href="#mod"> mod</a>      </td><td> modulus, % </td>
<td> <a href="#mod-int"> mod-int</a>      </td><td>
<span class="nowrap">modulus-int</span>,
<span class="nowrap">%-fp</span>
 </td>
<td> <a href="#mod-fp"> mod-fp</a>     </td><td>
<span class="nowrap">modulus-fp</span>,
<span class="nowrap">%-fp</span>
</td>
<tr>
<th> J-Types  </th><th> Aliases </th>
<th> M-Types  </th><th> Aliases </th>
<th> U-Types  </th><th> Aliases </th>
</tr>

</tr>
<tr>
<td> <a href="#skip">skip</a>       </td><td> ? </td>
<td> <a href="#save">save</a>      </td><td> sav, &lt;&lt; </td>
<td> <a href="#not">not</a>       </td><td> !, ~ </td>
</tr>
<tr>
<td> <a href="#call">call </a>     </td><td>  </td>
<td> <a href="#load">load</a>     </td><td> ld, &gt;&gt; </td>

<td>     </td><td></td>

</tr>
<tr>
<td> <a href="#jump">jump </a>    </td><td>  </td>
<td> <a href="#set">set</a>       </td><td> = </td>
<td>        </td><td>  </td>

</tr>


<tr>
<th> Pseudo Instructions </th><th>  </th>
<th> Special Characters </th><th> Meaning </th>
<th> Directives </th><th>  </th>
</tr>
<tr>
<td> <a href="#push"> push</a>     </td><td>  </td>
<td> <a href="#comments"> #</a>     </td><td> <a href="#comments">comments</a> </td>

<td> <a href="#assume">assume</a>       </td><td>  </td>


</tr>
<tr>
<td> <a href="#pop"> pop</a>     </td><td>  </td>
<td> <a href="#labels"> :</a>     </td><td> <a href="#labels"> labels</a> </td>
<td> <a href="#data"> data</a>       </td><td>  </td>
</tr>

<tr>
<td> <a href="#io">io </a>     </td><td>  </td>

<td>      </td><td>  </td>
<td>      </td><td>  </td>
</tr>

</table>
<h3 id="io-assumptions">IO Assumptions</h3>
<p>
The assembler maybe directed to automatically save and restore registers used in io directives. By default the assembler will save and restore register values automatically. This can be explicitly controlled through the <em>assume</em> directive.
</p>
<p>This will </p>
<h3 id="numeric-assumptions">Numeric Assumptions</h3>
<p>
The Erfindung CPU can perform operations on both integers and fixed point numbers. Bitwise operations, addition, and subtraction all use the same implementation regardless whether the data is to be treated as fixed point numbers or integers. Other operations where fixed point numbers and integers need separate implementations, have suffixes ('-fp' and '-int').
</p><p>
Assumption directives have the role of eliminating the need for these suffixes. Do note however, if there's an immediate, that immediate's 'type' will override any assumption that was directed. So a <em>times x a 0.8</em> will perform a fixed point operation regardless of any previous <em>assume integer</em>.</p>
<p>The assembler will by default assume integer. </p>
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
32 bit used value: 0bN000 0000 0000 0000 0XXX XXXX XXXX XXXX<br />

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
<em>Special Pseudo-Instruction</em>
You may issue a <em>load x</em> which the assembler will treat "x" as a pointer to load content from and store that content into register x. This special case is not available for save.<br />
<em>Special case:</em> If the second operand is an immediate rather than a register, then it'll be decoded not as an integer immediate, but rather as an address. The 16 bit address immediate is mapped to a 32 bit value as follows:
</p>

| most significant bit | least significant bits |
|:---------------------|------------------------|
| bit 15 to            | bits 14-0 to           |
| bit 31               | bits 14-0              |

Or alternatively visualized as: <br />
16 bit immediate: 0bNXXX XXXX XXXX XXXX<br />
32 bit used value: 0bN000 0000 0000 0000 0XXX XXXX XXXX XXXX<br />

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
<li><a href="#io-read">read controller/timer/random/gpu/bus-error Reg ...</a></li>
<li><a href="#io-upload">upload Reg Reg Reg Reg</a></li>
<li><a href="#io-clear">clear Reg</a></li>
<li><a href="#io-draw">draw Reg Reg Reg</a></li>
<li><a href="#io-halt">halt Reg</a></li>
<li><a href="#io-wait">wait Reg</a></li>
<li><a href="#io-note">triangle Reg Immd ...</a></li>
<li><a href="#io-note">pulse one/two Reg Immd ...</a></li>
<li><a href="#io-note">noise Reg Immd ...</a></li>
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
<h3 id="io-upload">io upload</h3>
<h3 id="io-clear">io clear</h3>
<h3 id="io-draw">io draw</h3>
<h3 id="io-halt">io halt</h3>
<h3 id="io-wait">io wait</h3>
<h3 id="io-note">io triangle/pulse/noise ...</h3>

<h3 id="call">call</h3>
<p>
J-Type <br />
Parameters: Reg/Immd<br />
Real instruction forms: <ul>
<li>Reg </li>
<li>Immd</li>
</ul>
The call instruction pushes PC + 1 onto the stack, increments SP by one and jumps to the address specified by the given immediate (as integer). This in effect acts as a function call (barring any epilogue/prologue of callee and caller register pushes and pops). <br />
This instruction exist for the reason that it is not possible to get the same behavior with just a jump instruction, especially light of skip instruction's single PC increment behavior. <br />
A <em>pop pc</em> acts as this ISA's ret instruction (from the very real x86).
</p>
<h3 id="jump">jump</h3>
Pure pseudo-instruction<br />
Jump is simply a set <em>pc (Reg/Immd)</em>
<p>

<h3 id="times">times</h3>
<p>
Pure Pseudo-Instruction <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs integer or fixed point multiplication on the last two operands and stores the result in the first operand.<br />
The Assembler emits a real times-int or times-fp depending on its numeric assumptions. If an immediate is present, than the immediate takes precedence in determining which real instruction is emitted. That is a fixed point immediate will cause a times-fp to be generated, labels/integers will cause a times-int to be emitted. <br />
<strong>Note: </strong>Labels are permissible for use with this instruction, however it is not recommended. It may cause unexpected behavior. I've failed to find a compelling reason to prohibit labels from appearing in this instruction.
</p>

<h3 id="times-int">times-int</h3>
<p>
R-Type<br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs an explicit integer multiplication on the last two operands and stores the result in the first operand.<br />
Unlike "times" or any instruction where operation selection is implicit, this instruction will always perform integer multiplication regardless of assumptions or the format of the immediate value.
</p>
<h3 id="times-fp">times-fp</h3>
<p>
R-Type<br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs an explicit fixed point multiplication on the last two operands and stores the result in the first operand.<br />
Unlike "times" or any instruction where operation selection is implicit, this instruction will always perform fixed point multiplication regardless of assumptions or the format of the immediate value.
</p>

<h3 id="div">division</h3>
<p>
Pure Pseudo-Instruction <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs integer or fixed point division on the last two operands and stores the result in the first operand.<br />
The Assembler emits a real div-int or div-fp depending on its numeric assumptions. If an immediate is present, than the immediate takes precedence in determining which real instruction is emitted. That is a fixed point immediate will cause a div-fp to be generated, labels/integers will cause a div-int to be emitted. <br />
<strong>Note: </strong>Labels are permissible for use with this instruction, however it is not recommended. It may cause unexpected behavior. I've failed to find a compelling reason to prohibit labels from appearing in this instruction.
</p>

<h3 id="div-int">division-int</h3>
<p>
R-Type<br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs an explicit integer division on the last two operands and stores the result in the first operand.<br />
Unlike "times" or any instruction where operation selection is implicit, this instruction will always perform integer multiplication regardless of assumptions or the format of the immediate value.
</p>
<h3 id="div-fp">division-fp</h3>
<p>
R-Type<br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs an explicit fixed point division on the last two operands and stores the result in the first operand.<br />
Unlike "times" or any instruction where operation selection is implicit, this instruction will always perform fixed point multiplication regardless of assumptions or the format of the immediate value.
</p>

<h3 id="comp">comp</h3>
<p>
Pure Pseudo-Instruction <br />
Parameters: Reg Reg/Immd/Label (Reg/Immd/Label) <br />
Real instruction forms: <ul>
<li>Reg Reg Reg </li>
<li>Reg Reg Immd</li>
</ul>
Performs integer or fixed point comparison on the last two operands and stores the result in the first operand.<br />
The Assembler emits a real comp-int or comp-fp depending on its numeric assumptions. If an immediate is present, than the immediate takes precedence in determining which real instruction is emitted. That is a fixed point immediate will cause a comp-fp to be generated, labels/integers will cause a comp-int to be emitted. <br />
The result is stored as a bit series in the least four significant bits, with all others being set to zero. If a condition holds true, then the bit is set to one, if false it is left as zero. Consider the first operand used as a, and the second operand as b (and result having no letter assigned). These conditions are mapped as follows:</p>

| bits 31-4            | bit 3         | bit 2        | bit 1        | bit 0         |
|----------------------|---------------|--------------|--------------|---------------|
| unconditionally zero | one if a != b | one if a > b | one if a < b | one if a == b |

<p>
<strong>Note: </strong>Labels are permissible for use with this instruction, however it is not recommended. It may cause unexpected behavior. I've failed to find a compelling reason to prohibit labels from appearing in this instruction.
</p>

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

<p>In general assumptions are applied immediately after an assume directive for the rest of the source file. Another assume directive maybe used later to direct the assembler to assume less/more/or something else. These directives cover [numeric assumptions](#numeric-assumptions) and [io related assumptions](#io-assumptions).</p>
