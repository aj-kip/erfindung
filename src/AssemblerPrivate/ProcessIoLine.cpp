/****************************************************************************

    File: ProcessIoLine.cpp
    Author: Andrew Janke
    License: GPLv3

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#include "ProcessIoLine.hpp"
#include "TextProcessState.hpp"
#include "LineParsingHelpers.hpp"

#include <map>
#include <string>

#include <cassert>

namespace {

using namespace erfin;
using StringCIter = TokensConstIterator;
using LineToInstFunc = StringCIter(*)(TextProcessState &, StringCIter, StringCIter);

StringCIter make_io_read        (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_apu_inst    (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_upload      (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_unload      (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_clear_screen(TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_draw        (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_halt        (TextProcessState &, StringCIter, StringCIter);
StringCIter make_io_wait        (TextProcessState &, StringCIter, StringCIter);

} // end of <anonymous> namespace

namespace erfin {

StringCIter make_sysio
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    // possible io commands (register names can vary)
    // io upload x y z # <- turns into three save instructions
    // io unload x
    // io clear screen
    // io draw x y z
    // io halt
    //
    // io read null       x # <- your "null pointer"
    // io read controller x
    // io read timer      x # <- time since last wait
    // io read random     x # <- rng semantics
    // io read gpu        x # <- read any output from the gpu
    // io read bus-error  x # <- check if a bus error occured
    //
    // io triangle/pulse one/pulse two/noise play x/IMMD
    // io ... tempo x/IMMD # notes per second
    // io ... duty  x      # for entire window

    using LineFuncMap = std::map<std::string, LineToInstFunc>;
    static bool is_initialized = false;
    static LineFuncMap fmap;

    if (is_initialized) {
        ++beg;
        auto itr = fmap.find(*beg);
        if (itr == fmap.end()) {
            throw state.make_error(": io contains no sub operation \"" +
                                   *beg + "\"."                         );
        }
        return (*itr->second)(state, beg, end);
    }
    fmap["read"  ] = make_io_read;
    fmap["upload"] = make_io_upload;
    fmap["unload"] = make_io_unload;
    fmap["clear" ] = make_io_clear_screen;
    fmap["draw"  ] = make_io_draw;
    fmap["halt"  ] = make_io_halt;
    fmap["wait"  ] = make_io_wait;

    fmap["triangle"] = fmap["pulse"] = fmap["noise"] = make_io_apu_inst;
    is_initialized = true;
    return make_sysio(state, beg, end);
}

void run_make_sysio_tests() {
    ;
}

} // end of erfin namespace

namespace {

void emit_set_aside_register_instructions
    (TextProcessState & state, int device_address, int command_identity, Reg scape_goat_reg);

StringCIter make_io_read
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace device_addresses;
    // io read controller x
    // io read timer      x # <- time since last wait
    // io read random     x # <- rng semantics
    // io read gpu        x # <- read any output from the gpu
    // io read bus-error  x # <- check if a bus error occured

    auto eol = get_eol(++beg, end);
    int source_address = 0;
    /**/ if (*beg == "controller") source_address = READ_CONTROLLER        ;
    else if (*beg == "timer"     ) source_address = TIMER_QUERY_SYNC_ET    ;
    else if (*beg == "random"    ) source_address = RANDOM_NUMBER_GENERATOR;
    else if (*beg == "gpu"       ) source_address = GPU_RESPONSE           ;
    else if (*beg == "bus-error" ) source_address = BUS_ERROR              ;
    else throw state.make_error(": \"" + *beg + "\" is not a valid source.");

    if (eol - ++beg < 1) {
        throw state.make_error(": no parameters were given, read expects at "
                               "least one register.");
    }

    for (; beg != eol; ++beg) {
        auto reg = string_to_register_or_throw(state, *beg);
        state.add_instruction(encode(OpCode::LOAD, reg              ,
                                     encode_immd_int(source_address)) );
    }
    return eol;
}

StringCIter make_io_apu_inst
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    auto eol = get_eol(beg, end);
    Channel channel;
    if (*beg == "triangle") {
        channel = Channel::TRIANGLE;
    } else if (*beg == "pulse") {
        ++beg;
        if (*beg == "one") {
            channel = Channel::PULSE_ONE;
        } else if (*beg == "two") {
            channel = Channel::PULSE_TWO;
        } else {
            throw state.make_error(": \"" + *beg + "\" is not a valid pulse "
                                   "channel."                                );
        }
    } else if (*beg == "noise") {
        channel = Channel::NOISE;
    } else {
        throw state.make_error(": \"" + *beg + "\" is not a valid channel.");
    }
    ++beg;
    ApuInstructionType ait;
    if (*beg == "note") {
        ait = ApuInstructionType::NOTE;
    } else if (*beg == "tempo") {
        ait = ApuInstructionType::TEMPO;
    } else if (*beg == "duty-cycle-window") {
        ait = ApuInstructionType::DUTY_CYCLE_WINDOW;
    } else {
        throw state.make_error(": channel 'command' \"" + *beg +
                               "\" is not recognized."          );
    }
    ++beg;
    // next we expect a register
    Reg reg = string_to_register_or_throw(state, *beg++);

    // next (optional) argument is a value to write to the apu
    if (beg != eol) {
        Immd immd;
        NumericParseInfo npi = parse_number(*beg);
        const std::string * label = nullptr;
        switch (npi.type) {
        case INTEGER:
            immd = encode_immd_int(npi.integer);
            break;
        case DECIMAL:
            throw state.make_error(": decimal values not support for apu io. "
                                   "Though you could write directly using a "
                                   "save instruction yourself (not suggested).");
        case NOT_NUMERIC:
            immd = encode_immd_int(0);
            label = &(*beg);
            break;
        }
        state.add_instruction(encode(OpCode::SET, reg, immd), label);
    }
    auto immd = encode_immd_int(device_addresses::APU_INPUT_STREAM);
    (void)ait; (void)channel;
    state.add_instruction(encode(OpCode::SAVE, reg, immd));

    return eol;
}

StringCIter make_io_upload
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    // upload sprite using using three register arguments
    // (note the use of the stack used to acheive this without losing data)
    // we need to use the stack in order to upload the constant classifying
    // the command for the gpu
    // I could just use a: save x GPU_WRITE_ADDRESSES
    auto eol = get_eol(++beg, end);
    if (eol - beg != 3) {
        throw state.make_error(": upload expects exactly three arguments: the "
                               "width, height and address.");
    }
    std::array<Reg, 3> args;
    for (Reg & arg : args) {
        arg = string_to_register_or_throw(state, *beg++);
    }
    assert(beg == eol);
    // what does push do to the SP?
    static constexpr const int GPU_INPUT_STREAM = device_addresses::GPU_INPUT_STREAM;
    emit_set_aside_register_instructions(state, GPU_INPUT_STREAM,
                                         gpu_enum_types::UPLOAD, args[0]);

    for (const Reg & arg : args) {
        state.add_instruction( encode(OpCode::SAVE,                arg,
                                      encode_immd_int(GPU_INPUT_STREAM)) );
    }
    return eol;
}

StringCIter make_io_unload
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    // unload using one argument
    using namespace erfin;
    static constexpr const char * const NEED_EXACTLY_ONE_MSG =
        ": unload excepts exactly one argument, the sprite index to unload.";
    auto eol = get_eol(++beg, end);
    if (eol - beg != 1) throw state.make_error(NEED_EXACTLY_ONE_MSG);
    Reg reg = string_to_register_or_throw(state, *beg);
    static constexpr const int GPU_INPUT_STREAM = device_addresses::GPU_INPUT_STREAM;
    emit_set_aside_register_instructions
        (state, GPU_INPUT_STREAM, gpu_enum_types::UNLOAD, reg);

    state.add_instruction( encode(OpCode::SAVE,                reg,
                                  encode_immd_int(GPU_INPUT_STREAM)) );
    return erfin::get_eol(beg, end);
}

StringCIter make_io_clear_screen
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    // still requires a "scape goat" register
    using namespace erfin;
    static constexpr const char * const NEED_EXACTLY_ONE_MSG =
        ": clear screen need exactly one register argument. This is used for "
        "the emitted save instruction. The previous value will be restored.";
    auto eol = get_eol(++beg, end);
    if (eol - beg != 1) throw state.make_error(NEED_EXACTLY_ONE_MSG);
    Reg reg = string_to_register_or_throw(state, *beg);
    emit_set_aside_register_instructions
        (state, device_addresses::GPU_INPUT_STREAM, gpu_enum_types::CLEAR, reg);
    return eol;
}

StringCIter make_io_draw
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    // three arguments
    using namespace erfin;
    auto eol = get_eol(++beg, end);
    if (eol - beg != 3) {
        state.make_error(": upload expects exactly three arguments: the "
                         "width, height and address.");
    }
    std::array<Reg, 3> args;
    for (Reg & arg : args) {
        arg = string_to_register_or_throw(state, *beg++);
    }
    assert(beg == eol);
    // what does push do to the SP?
    static constexpr const int GPU_INPUT_STREAM = device_addresses::GPU_INPUT_STREAM;
    emit_set_aside_register_instructions
        (state, GPU_INPUT_STREAM, gpu_enum_types::DRAW, args[0]);

    for (const Reg & arg : args) {
        state.add_instruction( encode(OpCode::SAVE,                arg,
                                      encode_immd_int(GPU_INPUT_STREAM)) );
    }
    return eol;
}

StringCIter make_io_halt
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace device_addresses;
    auto eol = get_eol(++beg, end);
    if (beg - eol != 1) {
        throw state.make_error(": halt io command must have exactly one "
                               "register argument.");
    }
    auto reg = string_to_register_or_throw(state, *beg);
    state.add_instruction(encode(OpCode::SET , reg, encode_immd_int(1)));
    state.add_instruction(encode(OpCode::SAVE, reg, encode_immd_int(HALT_SIGNAL)));
    return eol;
}

StringCIter make_io_wait
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace device_addresses;
    auto eol = get_eol(++beg, end);
    if (eol - beg != 1) {
        throw state.make_error(": wait io command must have exactly one "
                               "register argument.");
    }
    auto reg = string_to_register_or_throw(state, *beg);
    state.add_instruction(encode(OpCode::SAVE, reg, encode_immd_int(TIMER_WAIT_AND_SYNC)));
    return eol;
}

// ----------------------------------------------------------------------------

void emit_set_aside_register_instructions
    (TextProcessState & state, int device_address, int command_identity, Reg scape_goat_reg)
{
    using namespace erfin;
    auto reg = scape_goat_reg;
    state.add_instruction(encode(OpCode::PLUS, Reg::SP, Reg::SP, encode_immd_int(1)));
    state.add_instruction(encode(OpCode::SAVE, reg, Reg::SP));
    state.add_instruction(encode(OpCode::SET , reg, encode_immd_int(command_identity)));
    state.add_instruction(encode(OpCode::SAVE, reg, encode_immd_int(device_address  )));
    state.add_instruction(encode(OpCode::LOAD, reg, Reg::SP));
    state.add_instruction(encode(OpCode::MINUS, Reg::SP, Reg::SP, encode_immd_int(1)));
}

} // end of <anonymous> namespace