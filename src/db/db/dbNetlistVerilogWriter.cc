
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2026 Matthias Koefferlein

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "dbNetlistVerilogWriter.h"
#include "dbNetlist.h"
#include "dbCircuit.h"
#include "dbSubCircuit.h"
#include "dbDevice.h"
#include "dbDeviceClass.h"
#include "dbNet.h"
#include "dbPin.h"

#include "tlStream.h"
#include "tlUniqueName.h"
#include "tlString.h"

#include <sstream>
#include <set>
#include <cctype>

namespace db
{

//  Verilog keywords which must not be used as plain identifiers.
//  This is not the full list - it covers the ones likely to appear as
//  net or cell names in an extracted netlist.
static const char *verilog_keywords[] = {
  "always", "and", "assign", "begin", "buf", "case", "cmos", "deassign",
  "default", "defparam", "disable", "else", "end", "endcase", "endmodule",
  "endfunction", "endtask", "event", "for", "force", "forever", "fork",
  "function", "if", "initial", "inout", "input", "integer", "join", "module",
  "nand", "negedge", "nmos", "nor", "not", "or", "output", "parameter",
  "pmos", "posedge", "primitive", "reg", "release", "repeat", "rtran",
  "signed", "supply0", "supply1", "table", "task", "time", "tran", "tri",
  "wait", "wand", "while", "wire", "wor", "xnor", "xor", 0
};

static bool is_verilog_keyword (const std::string &s)
{
  for (const char **k = verilog_keywords; *k; ++k) {
    if (s == *k) {
      return true;
    }
  }
  return false;
}

/**
 *  @brief Formats a name as a Verilog identifier
 *
 *  Simple identifiers are passed through. Everything else is turned into an
 *  escaped identifier (backslash ... whitespace) as per IEEE 1364.
 */
static std::string format_name (const std::string &s)
{
  if (s.empty ()) {
    return "\\  ";
  }

  bool simple = (isalpha ((unsigned char) s[0]) || s[0] == '_');
  for (const char *cp = s.c_str (); simple && *cp; ++cp) {
    if (! isalnum ((unsigned char) *cp) && *cp != '_' && *cp != '$') {
      simple = false;
    }
  }

  if (simple && ! is_verilog_keyword (s)) {
    return s;
  }

  //  escaped identifier: a backslash, the name (without blanks) and a blank
  std::string n;
  n.reserve (s.size () + 2);
  n += "\\";
  for (const char *cp = s.c_str (); *cp; ++cp) {
    if (isspace ((unsigned char) *cp)) {
      n += "_";
    } else {
      n += *cp;
    }
  }
  n += " ";
  return n;
}

NetlistVerilogWriter::NetlistVerilogWriter ()
  : mp_netlist (0), mp_stream (0), m_with_comments (true)
{
  //  .. nothing yet ..
}

NetlistVerilogWriter::~NetlistVerilogWriter ()
{
  //  .. nothing yet ..
}

void NetlistVerilogWriter::set_with_comments (bool f)
{
  m_with_comments = f;
}

void NetlistVerilogWriter::set_pin_direction (const std::string &circuit_name, const std::string &pin_name, VerilogPinDirection dir)
{
  m_pin_directions[std::make_pair (circuit_name, pin_name)] = dir;
}

void NetlistVerilogWriter::clear_pin_directions ()
{
  m_pin_directions.clear ();
}

VerilogPinDirection NetlistVerilogWriter::pin_direction (const db::Circuit &circuit, const db::Pin &pin) const
{
  std::map<std::pair<std::string, std::string>, VerilogPinDirection>::const_iterator d =
    m_pin_directions.find (std::make_pair (circuit.name (), pin.name ()));
  if (d != m_pin_directions.end ()) {
    return d->second;
  } else {
    return VerilogInout;
  }
}

void NetlistVerilogWriter::write (tl::OutputStream &stream, const db::Netlist &netlist, const std::string &description)
{
  mp_netlist = &netlist;
  mp_stream = &stream;

  try {
    do_write (description);
    mp_netlist = 0;
    mp_stream = 0;
  } catch (...) {
    mp_netlist = 0;
    mp_stream = 0;
    throw;
  }
}

void NetlistVerilogWriter::emit_line (const std::string &line) const
{
  *mp_stream << line << "\n";
}

void NetlistVerilogWriter::emit_comment (const std::string &comment) const
{
  *mp_stream << "// " << comment << "\n";
}

std::string NetlistVerilogWriter::net_to_string (const db::Net *net) const
{
  std::map<const db::Net *, std::string>::const_iterator n = m_net_to_name.find (net);
  if (n == m_net_to_name.end ()) {
    //  an unconnected pin - Verilog allows an empty connection
    return std::string ();
  } else {
    return format_name (n->second);
  }
}

std::string NetlistVerilogWriter::port_name (const db::Circuit &circuit, const db::Pin &pin) const
{
  //  A port is named after the net attached to it - this way the port name and
  //  the wire name inside the module are identical, as Verilog requires.
  //  Pins are not necessarily named in an extracted netlist and pin names live
  //  in a different name space than nets, so the pin's own name cannot be used.
  const db::Net *net = circuit.net_for_pin (pin.id ());
  if (net) {
    return format_name (net->expanded_name ());
  } else if (! pin.name ().empty ()) {
    return format_name (pin.name ());
  } else {
    //  an unconnected, unnamed pin - synthesize a name which cannot clash with
    //  a net name (net names are taken from expanded_name which uses "$<n>")
    return format_name ("$pin" + tl::to_string (pin.id ()));
  }
}

void NetlistVerilogWriter::prepare_net_names (const db::Circuit &circuit)
{
  m_net_to_name.clear ();

  for (db::Circuit::const_net_iterator n = circuit.begin_nets (); n != circuit.end_nets (); ++n) {
    m_net_to_name.insert (std::make_pair (n.operator-> (), n->expanded_name ()));
  }
}

void NetlistVerilogWriter::do_write (const std::string &description)
{
  if (! description.empty ()) {
    emit_comment (description);
    emit_line ("");
  }

  for (db::Netlist::const_bottom_up_circuit_iterator c = mp_netlist->begin_bottom_up (); c != mp_netlist->end_bottom_up (); ++c) {

    const db::Circuit &circuit = *c;

    prepare_net_names (circuit);

    write_module_header (circuit);

    for (db::Circuit::const_subcircuit_iterator i = circuit.begin_subcircuits (); i != circuit.end_subcircuits (); ++i) {
      write_subcircuit_call (*i);
    }

    if (m_with_comments) {
      for (db::Circuit::const_device_iterator i = circuit.begin_devices (); i != circuit.end_devices (); ++i) {
        emit_comment ("device not written in Verilog: " + i->expanded_name () + " (" + i->device_class ()->name () + ")");
      }
    }

    write_module_end (circuit);

  }
}

void NetlistVerilogWriter::write_module_header (const db::Circuit &circuit)
{
  emit_line ("");

  if (m_with_comments) {
    emit_comment ("cell " + circuit.name ());
  }

  //  the module statement with the port list

  std::ostringstream os;
  os << "module " << format_name (circuit.name ()) << " (";

  bool first = true;
  for (db::Circuit::const_pin_iterator p = circuit.begin_pins (); p != circuit.end_pins (); ++p) {
    if (! first) {
      os << ", ";
    }
    first = false;
    os << port_name (circuit, *p);
  }

  os << ");";
  emit_line (os.str ());

  //  the port directions

  for (db::Circuit::const_pin_iterator p = circuit.begin_pins (); p != circuit.end_pins (); ++p) {
    VerilogPinDirection dir = pin_direction (circuit, *p);
    const char *dirs = (dir == VerilogInput ? "input" : (dir == VerilogOutput ? "output" : "inout"));
    emit_line (std::string ("  ") + dirs + " " + port_name (circuit, *p) + ";");
  }

  //  the internal wires - nets which are not attached to a pin
  //
  //  Note: a net connected to a pin is already declared through the port
  //  declaration, so it must not be declared again.

  std::set<const db::Net *> pin_nets;
  for (db::Circuit::const_pin_iterator p = circuit.begin_pins (); p != circuit.end_pins (); ++p) {
    const db::Net *net = circuit.net_for_pin (p->id ());
    if (net) {
      pin_nets.insert (net);
    }
  }

  for (db::Circuit::const_net_iterator n = circuit.begin_nets (); n != circuit.end_nets (); ++n) {
    if (pin_nets.find (n.operator-> ()) == pin_nets.end ()) {
      emit_line ("  wire " + net_to_string (n.operator-> ()) + ";");
    }
  }
}

std::string NetlistVerilogWriter::instance_name (const db::SubCircuit &subcircuit) const
{
  //  Instance names and net names share one name space in Verilog, but not in
  //  the netlist model: both are "$<n>" when not named explicitly. Hence a
  //  synthesized instance name needs a prefix to keep it apart from a net.
  const std::string &n = subcircuit.name ();
  if (! n.empty ()) {
    return format_name (n);
  } else {
    return format_name ("$I" + subcircuit.expanded_name ().substr (1));
  }
}

void NetlistVerilogWriter::write_subcircuit_call (const db::SubCircuit &subcircuit) const
{
  if (m_with_comments && ! subcircuit.trans ().is_unity ()) {
    emit_comment ("cell instance " + subcircuit.expanded_name () + " " + subcircuit.trans ().to_string ());
  }

  std::ostringstream os;
  os << "  " << format_name (subcircuit.circuit_ref ()->name ());
  os << " " << instance_name (subcircuit) << " (";

  bool first = true;
  for (db::Circuit::const_pin_iterator p = subcircuit.circuit_ref ()->begin_pins (); p != subcircuit.circuit_ref ()->end_pins (); ++p) {
    if (! first) {
      os << ", ";
    }
    first = false;
    os << "." << port_name (*subcircuit.circuit_ref (), *p);
    os << "(" << net_to_string (subcircuit.net_for_pin (p->id ())) << ")";
  }

  os << ");";
  emit_line (os.str ());
}

void NetlistVerilogWriter::write_module_end (const db::Circuit & /*circuit*/) const
{
  emit_line ("endmodule");
}

}
