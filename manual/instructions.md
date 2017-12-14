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
...<br />
This instruction has the same effects regardless of assumptions/deduced data 'type' of the operands. The effects will be semantically correct for both integers and fixed points, though the implementation itself is the same.
</p>

### io
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
