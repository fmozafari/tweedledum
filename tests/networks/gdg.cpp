/*------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
| Author(s): Bruno Schmitt
*-----------------------------------------------------------------------------*/
#include <catch.hpp>
#include <tweedledum/algorithms/optimization/single_qubit_gate_cancellation.hpp>
#include <tweedledum/io/write_qpic.hpp>
#include <tweedledum/networks/gates/gate_kinds.hpp>
#include <tweedledum/networks/gates/qc_gate.hpp>
#include <tweedledum/networks/gdg.hpp>

TEST_CASE("Create GDG network with a few qubits", "[gdg]")
{
	using namespace tweedledum;
	gdg<qc_gate> network;

	CHECK(network.size() == 0);

	auto q0 = network.add_qubit("q0");
	CHECK(network.size() == 2);
	CHECK(network.num_qubits() == 1);

	auto q1 = network.add_qubit();
	CHECK(network.size() == 4);
	CHECK(network.num_qubits() == 2);
}

TEST_CASE("Create GDG network with one qubit and few single-qubit gates",
          "[gdg]")
{
	using namespace tweedledum;
	gdg<qc_gate> network;

	CHECK(network.size() == 0);

	auto q0 = network.add_qubit("q0");
	CHECK(network.size() == 2);
	CHECK(network.num_qubits() == 1);

	network.add_gate(gate_kinds_t::hadamard, "q0");
	network.add_gate(gate_kinds_t::hadamard, 0);
	network.add_gate(gate_kinds_t::t, 0);
	network.add_gate(gate_kinds_t::t_dagger, 0);
	network.add_gate(gate_kinds_t::rotation_z, "q0");
	CHECK(network.size() == 7);
	CHECK(network.num_gates() == 5);
	CHECK(network.num_qubits() == 1);
}

TEST_CASE("Gate cancellation", "[gdg]")
{
	using namespace tweedledum;
	gdg<qc_gate> network;

	CHECK(network.size() == 0);

	auto q0 = network.add_qubit("q0");
	CHECK(network.size() == 2);
	CHECK(network.num_qubits() == 1);

	network.add_gate(gate_kinds_t::hadamard, "q0");
	network.add_gate(gate_kinds_t::hadamard, 0);
	// network.add_gate(gate_kinds_t::t, 0);
	// network.add_gate(gate_kinds_t::t_dagger, 0);
	// network.add_gate(gate_kinds_t::rotation_z, "q0");
	single_qubit_gate_cancellation(network);
	write_qpic(network, "gdg.qpic", true);
	// CHECK(network.size() == 7);
	// CHECK(network.num_gates() == 5);
	// CHECK(network.num_qubits() == 1);
}
