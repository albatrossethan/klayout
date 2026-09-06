
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

#ifndef HDR_dbNetlistVerilogWriter
#define HDR_dbNetlistVerilogWriter

#include "dbCommon.h"
#include "dbNetlistWriter.h"

#include <string>
#include <map>
#include <set>

namespace db
{

class Netlist;
class Net;
class Circuit;
class SubCircuit;
class Pin;

/**
 *  @brief The pin direction hint for the Verilog writer
 *
 *  The netlist model does not provide pin directions as these are not
 *  available from layout extraction. The Verilog writer will use "inout"
 *  by default. Directions can be supplied explicitly - e.g. from a
 *  Liberty file or the design intent.
 */
enum VerilogPinDirection
{
  VerilogInout = 0,
  VerilogInput = 1,
  VerilogOutput = 2
};

/**
 *  @brief A structural Verilog format writer for netlists
 *
 *  This writer produces a structural (gate-level) Verilog netlist from a
 *  db::Netlist object. Circuits become modules, subcircuits become module
 *  instances with named port connections and nets become wires.
 *
 *  Devices are not written as there is no canonical Verilog representation
 *  for them. A netlist with devices is not a structural netlist in the
 *  Verilog sense. Devices are skipped and reported as comments if comments
 *  are enabled.
 */
class DB_PUBLIC NetlistVerilogWriter
  : public NetlistWriter
{
public:
  NetlistVerilogWriter ();
  virtual ~NetlistVerilogWriter ();

  virtual void write (tl::OutputStream &stream, const db::Netlist &netlist, const std::string &description);

  /**
   *  @brief Sets a value indicating whether comments are written
   */
  void set_with_comments (bool f);

  /**
   *  @brief Gets a value indicating whether comments are written
   */
  bool with_comments () const
  {
    return m_with_comments;
  }

  /**
   *  @brief Sets the direction for a pin of a circuit
   *
   *  As the netlist model does not carry pin directions, they can be
   *  supplied here. The default direction is "inout".
   */
  void set_pin_direction (const std::string &circuit_name, const std::string &pin_name, VerilogPinDirection dir);

  /**
   *  @brief Clears the pin directions registered
   */
  void clear_pin_directions ();

private:
  const db::Netlist *mp_netlist;
  tl::OutputStream *mp_stream;
  std::map<const db::Net *, std::string> m_net_to_name;
  std::map<std::pair<std::string, std::string>, VerilogPinDirection> m_pin_directions;
  bool m_with_comments;

  void do_write (const std::string &description);
  void prepare_net_names (const db::Circuit &circuit);

  std::string net_to_string (const db::Net *net) const;
  std::string port_name (const db::Circuit &circuit, const db::Pin &pin) const;
  std::string instance_name (const db::SubCircuit &subcircuit) const;
  VerilogPinDirection pin_direction (const db::Circuit &circuit, const db::Pin &pin) const;

  void emit_line (const std::string &line) const;
  void emit_comment (const std::string &comment) const;

  void write_module_header (const db::Circuit &circuit);
  void write_subcircuit_call (const db::SubCircuit &subcircuit) const;
  void write_module_end (const db::Circuit &circuit) const;
};

}

#endif
