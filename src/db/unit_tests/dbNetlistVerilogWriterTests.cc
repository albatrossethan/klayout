
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
#include "dbNet.h"
#include "dbPin.h"

#include "tlUnitTest.h"
#include "tlStream.h"

//  Builds a small hierarchical netlist:
//
//    module INV (A, Y)      - a leaf cell, no content
//    module TOP (IN, OUT)   - two INV in series
//
static db::Netlist *make_netlist ()
{
  db::Netlist *nl = new db::Netlist ();

  db::Circuit *inv = new db::Circuit ();
  inv->set_name ("INV");
  nl->add_circuit (inv);

  db::Net *ia = new db::Net (); ia->set_name ("A");
  db::Net *iy = new db::Net (); iy->set_name ("Y");
  inv->add_net (ia);
  inv->add_net (iy);
  inv->add_pin ("A");
  inv->add_pin ("Y");
  inv->connect_pin (0, ia);
  inv->connect_pin (1, iy);

  db::Circuit *top = new db::Circuit ();
  top->set_name ("TOP");
  nl->add_circuit (top);

  db::Net *tin = new db::Net (); tin->set_name ("IN");
  db::Net *tmid = new db::Net (); tmid->set_name ("MID");
  db::Net *tout = new db::Net (); tout->set_name ("OUT");
  top->add_net (tin);
  top->add_net (tmid);
  top->add_net (tout);
  top->add_pin ("IN");
  top->add_pin ("OUT");
  top->connect_pin (0, tin);
  top->connect_pin (1, tout);

  db::SubCircuit *sc1 = new db::SubCircuit (inv, "I1");
  top->add_subcircuit (sc1);
  sc1->connect_pin (0, tin);
  sc1->connect_pin (1, tmid);

  db::SubCircuit *sc2 = new db::SubCircuit (inv, "I2");
  top->add_subcircuit (sc2);
  sc2->connect_pin (0, tmid);
  sc2->connect_pin (1, tout);

  return nl;
}

static std::string write_netlist (db::Netlist *nl, db::NetlistVerilogWriter &writer)
{
  std::ostringstream os;
  tl::OutputStringStream oss;
  tl::OutputStream stream (oss);
  writer.write (stream, *nl, std::string ());
  stream.flush ();
  return oss.string ();
}

TEST(1_VerilogWriterBasic)
{
  std::unique_ptr<db::Netlist> nl (make_netlist ());

  db::NetlistVerilogWriter writer;
  writer.set_with_comments (false);

  std::string res = write_netlist (nl.get (), writer);

  //  leaf cells come first (bottom-up), ports are inout by default
  EXPECT_EQ (res.find ("module INV (A, Y);") != std::string::npos, true);
  EXPECT_EQ (res.find ("  inout A;") != std::string::npos, true);
  EXPECT_EQ (res.find ("module TOP (IN, OUT);") != std::string::npos, true);

  //  MID is internal, so it is a wire; IN/OUT are ports and must not be re-declared
  EXPECT_EQ (res.find ("  wire MID;") != std::string::npos, true);
  EXPECT_EQ (res.find ("  wire IN;") == std::string::npos, true);

  //  named port connections on the instances
  EXPECT_EQ (res.find ("  INV I1 (.A(IN), .Y(MID));") != std::string::npos, true);
  EXPECT_EQ (res.find ("  INV I2 (.A(MID), .Y(OUT));") != std::string::npos, true);

  EXPECT_EQ (res.find ("endmodule") != std::string::npos, true);

  //  INV must be written before TOP (definition before use)
  EXPECT_EQ (res.find ("module INV") < res.find ("module TOP"), true);
}

TEST(2_VerilogWriterPinDirections)
{
  std::unique_ptr<db::Netlist> nl (make_netlist ());

  db::NetlistVerilogWriter writer;
  writer.set_with_comments (false);
  writer.set_pin_direction ("INV", "A", db::VerilogInput);
  writer.set_pin_direction ("INV", "Y", db::VerilogOutput);

  std::string res = write_netlist (nl.get (), writer);

  EXPECT_EQ (res.find ("  input A;") != std::string::npos, true);
  EXPECT_EQ (res.find ("  output Y;") != std::string::npos, true);
  //  TOP was not given directions, so it stays inout
  EXPECT_EQ (res.find ("  inout IN;") != std::string::npos, true);
}

TEST(3_VerilogWriterNameEscaping)
{
  std::unique_ptr<db::Netlist> nl (new db::Netlist ());

  db::Circuit *c = new db::Circuit ();
  c->set_name ("sky130_fd_sc_hd__nand2_2");
  nl->add_circuit (c);

  //  a name which is not a valid simple Verilog identifier
  db::Net *n = new db::Net (); n->set_name ("net[3]");
  c->add_net (n);
  //  a Verilog keyword as a net name
  db::Net *n2 = new db::Net (); n2->set_name ("output");
  c->add_net (n2);

  db::NetlistVerilogWriter writer;
  writer.set_with_comments (false);

  std::string res = write_netlist (nl.get (), writer);

  //  cell name is a valid simple identifier and passes through
  EXPECT_EQ (res.find ("module sky130_fd_sc_hd__nand2_2 ();") != std::string::npos, true);
  //  "net[3]" must be escaped
  EXPECT_EQ (res.find ("wire \\net[3] ;") != std::string::npos, true);
  //  a keyword must be escaped too
  EXPECT_EQ (res.find ("wire \\output ;") != std::string::npos, true);
}
