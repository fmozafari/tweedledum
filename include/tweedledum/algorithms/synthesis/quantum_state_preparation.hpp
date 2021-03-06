#pragma once

#include "../../gates/gate_set.hpp"
#include "../../gates/gate_base.hpp"
#include "../../networks/qubit.hpp"
#include "../generic/rewrite.hpp"
#include "linear_synth.hpp"
#include "tbs.hpp"
#include <array>
#include <iostream>
#include <vector>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/esop.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>
#include <kitty/kitty.hpp>
#include <vector>
#include <math.h>

namespace tweedledum {
namespace detail {
template<class Network>
void decomposition_mcz(Network& net,  std::vector<qubit_id> const& q_map, kitty::dynamic_truth_table tt)//changed by fereshte
{
    
    //unsigned nlines = q_map.size();
    //auto tt = kitty::create<kitty::dynamic_truth_table>(nlines-1);
    //kitty::set_bit(tt,pow(2,nlines-1)-1);
    //kitty::set_bit(tt,5);
        
    const auto num_controls = tt.num_vars();
    //assert(qubit_map.size() == num_controls + 1);
    
    auto g = kitty::extend_to(tt, num_controls + 1);
    auto xt = g.construct();
    kitty::create_nth_var(xt, num_controls);
    g &= xt;
    

    parity_terms parities;
    const float nom = M_PI / (1 << g.num_vars());
    const auto spectrum = kitty::rademacher_walsh_spectrum(g);
    for (auto i = 1u; i < spectrum.size(); ++i) {
        if (spectrum[i] == 0) {
            continue;
        }
        parities.add_term(i, nom * spectrum[i]);
        
    }
    
    detail::linear_synth_gray(net, q_map, parities);

        
}

std::vector<double> gauss(std::vector< std::vector<double> > A) 
{
    auto n = A.size();
    std::cout<<"m size: "<<n<<std::endl;
    for (auto i=0u; i<n; i++) {
        // Search for maximum in this column
        double maxEl = abs(A[i][i]);
        auto maxRow = i;
        for (auto k=i+1; k<n; k++) {
            if (abs(A[k][i]) > maxEl) {
                maxEl = abs(A[k][i]);
                maxRow = k;
            }
        }

        // Swap maximum row with current row (column by column)
        for (auto k=i; k<n+1;k++) {
            double tmp = A[maxRow][k];
            A[maxRow][k] = A[i][k];
            A[i][k] = tmp;
        }

        // Make all rows below this one 0 in current column
        for (auto k=i+1; k<n; k++) {
            double c = -A[k][i]/A[i][i];
            for (int j=i; j<n+1; j++) {
                if (i==j) {
                    A[k][j] = 0;
                } else {
                    A[k][j] += c * A[i][j];
                }
            }
        }
    }
std::cout<<"after m manipulation\n";
    // Solve equation Ax=b for an upper triangular matrix A
    std::vector<double> x(n,0);
    for (int32_t i=n-1; i>=0; i--) {
        std::cout<<"i: "<<i<<std::endl;
        x[i] = A[i][n]/A[i][i];
        std::cout<<"x[i]: "<<x[i]<<"\n";
        for (int32_t k=i-1;k>=0; k--) {
            std::cout<<"k: "<<k<<std::endl;
            A[k][n] -= A[k][i] * x[i];
            std::cout<<"A[k][n]: "<<A[k][n]<<"\n";
        }
    }
    std::cout<<"after equation solve\n";
    return x;
}

template<class Network>
void multiplex_decomposition(Network& net, std::vector<double> mux_angles, uint32_t target_id)
{
    uint32_t n = mux_angles.size();
    auto binarytogray = [](uint32_t num) { return (num>>1) ^ num;};
    //create M matrix
    std::vector<double> line(n+1,0);
    std::vector< std::vector<double> > M(n,line);
    for (auto i=0u; i<n; i++) {
        for (auto j=0u; j<n; j++) {
            M[i][j] = pow(-1, __builtin_popcount(i & (binarytogray(j)) ));
        }
    }
    //solving n equations n unknowns
    for (int i=0; i<n; i++) {
        M[i][n] = mux_angles[i];
    }
    std::cout<<"after M\n";
    std::vector<double> angs(n);
    angs = gauss(M);
    std::cout<<"after gauss\n";
    //add gates to network
    for(auto i=0; i<n;i++){
        net.add_gate(gate_base(gate_set::rotation_y, angs[i]), target_id);
        uint32_t ctrl = log2(binarytogray(i) ^  ( (i==n-1) ? 0 : binarytogray(i+1) )) +1;
        ctrl += target_id;
        std::cout<<"control,target: "<<ctrl<<" "<<target_id<<std::endl;
        net.add_gate(gate::cx,ctrl,target_id);
    }
}

std::vector<uint32_t> extract_angle_idxs(std::vector<uint32_t> idxvec)
{
    std::vector<uint32_t> out_idx;
    auto temp=0;
    out_idx.emplace_back(temp);
    std::cout<<"size: "<<idxvec.size()<<std::endl;
    for(auto i=0u;i<idxvec.size();i++)
    {
        if(idxvec[i]==2)
        {
            for(auto j=0u;j<out_idx.size();j++)
            {
                auto temp1 = out_idx[j] + pow(2,i);
                out_idx.emplace_back(temp1);
            }
        }
        else
        {
            for(auto j=0u;j<out_idx.size();j++)
                out_idx[j] += (pow(2,i) * idxvec[i]);
        }
        
        
    }

    return out_idx;
}

template<typename Network>
void extract_multiplex_gates(Network & net, uint32_t n, std::vector < std::tuple < std::string,double,uint32_t,std::vector<uint32_t> > >
 gates)
{
    auto angle_1 = std::get<1> (gates[0]);
    auto targetid_1 = std::get<2> (gates[0]);
    auto csize_1 = (std::get<3> (gates[0])).size();
    if( (csize_1==0) && (targetid_1==(n-1)) )
    {
        if(angle_1 != 0)
            net.add_gate(gate_base(gate_set::rotation_y, angle_1), targetid_1);
        gates.erase(gates.begin());
    }

    for(int32_t i=n-2;i>=0;i--)
    {
        auto num_angles = pow (2, (n-i-1) );
        std::cout<<"num angles: "<<num_angles<<std::endl;
        std::vector<double> angles(num_angles,0);
        for( auto [name,angle,target_id,controls]: gates)
        { 
            if(target_id==i)
            {
                auto len = n-target_id-1;
                std::vector<uint32_t> idxs (len,2);
                
                std::for_each(begin(controls), end(controls), [&idxs,n] (uint32_t c) {idxs[n-1-(c/2)] = c%2;});
                for(auto id : idxs)
                    std::cout<<"idxs: "<<id<<std::endl;
                std::cout<<"alaki\n";
                std::reverse(idxs.begin(),idxs.end());
                std::vector<uint32_t> angle_idxs = extract_angle_idxs(idxs);
                std::cout<<"alaki2\n";
                for(auto id : angle_idxs)
                    angles[id] = angle;

            }
        }
        for(auto id : angles)
            std::cout<<"angle: "<<id<<std::endl;
        multiplex_decomposition(net, angles, i);
    }
    
}

/*std::vector<std::tuple<std::string,double,uint32_t,std::vector<uint32_t>>>*/ void control_line_cancelling
(std::vector<std::tuple<std::string,double,uint32_t,std::vector<uint32_t>>>& in_gates, uint32_t nqubits)
{
    //std::vector<std::tuple<std::string,double,uint32_t,std::vector<uint32_t>>> out_gates;
    std::vector<uint32_t> line_values (nqubits, 0);
    uint32_t idx=0;
    for(auto & [name,angle,target_id,controls]: in_gates){
        if(angle==0){
            in_gates.erase(in_gates.begin()+idx);
            continue;
        }
        idx++;
        //check controls
        if (controls.size()>0){
            for(auto i=0u;i<controls.size();){
                auto l = controls[i] / 2;
                auto c_val = (controls[i] % 2); // 0:pos 1:neg
                if(line_values[l]==c_val){
                    std::cout<<"erase control\n";
                    controls.erase(controls.begin()+i);
                    continue;
                }
                i++;
            }
        }
        //update line values
        if(angle = M_PI)
            line_values[target_id] = 1;
        else 
            line_values[target_id] = 2;
    }

    return ;//out_gates;
}


} // namespace detail end
//**************************************************************
struct qsp_params {
	enum class strategy : uint32_t {
		allone_first,
		ownfunction,
	} strategy = strategy::ownfunction;
};

void general_qg_generation(std::vector<std::tuple<std::string,double,uint32_t,std::vector<uint32_t>>>& gates,
 kitty::dynamic_truth_table tt, uint32_t var_index, std::vector<uint32_t> controls)
{
    if(var_index==-1)
        return;
    //-----co factors-------
    kitty::dynamic_truth_table tt0(var_index);
    kitty::dynamic_truth_table tt1(var_index);
    tt0 = kitty::shrink_to(kitty::cofactor0(tt,var_index), tt.num_vars() - 1);
    tt1 = kitty::shrink_to(kitty::cofactor1(tt,var_index), tt.num_vars() - 1);
    //--computing probability gate---
    auto c0_ones = kitty::count_ones(tt0);
    
    auto c1_ones = kitty::count_ones(tt1);
    auto tt_ones = kitty::count_ones(tt);
    //if (c0_ones!=tt_ones){ // == --> identity and ignore
        double angle = 2*acos(sqrt(static_cast<double> (c0_ones)/tt_ones));
        //angle *= (180/3.14159265); //in degree
        //----add probability gate----
        gates.emplace_back("RY",angle,var_index,controls);
        std::cout<<"c0 ones: "<<c0_ones<<"tt one: "<<tt_ones<<std::endl;
        std::cout<<"angle: "<<angle<<"control size: "<<controls.size()<<std::endl;
        
    //}
    //-----qc of cofactors-------
    //---check state--- 
    auto c0_allone = (c0_ones==pow(2, tt0.num_vars())) ? true : false ;
    auto c0_allzero = (c0_ones==0) ? true : false ;
    auto c1_allone = (c1_ones==pow(2, tt1.num_vars())) ? true : false ;
    auto c1_allzero = (c1_ones==0) ? true : false ;

    std::vector<uint32_t> controls_new0;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new0)); 
    auto ctrl0 = var_index*2 + 0; //negetive control: /2 ---> index %2 ---> sign
    controls_new0.emplace_back(ctrl0);
    if (c0_allone){
        
        //---add H gates---
        for(auto i=0u;i<var_index;i++)
            gates.emplace_back("RY",M_PI/2,i,controls_new0);
        //--check one cofactor----
        std::vector<uint32_t> controls_new1;
        std::copy(controls.begin(), controls.end(), back_inserter(controls_new1)); 
        auto ctrl1 = var_index*2 + 1; //negetive control: /2 ---> index %2 ---> sign
        controls_new1.emplace_back(ctrl1);
        if(c1_allone){
            //---add H gates---
            for(auto i=0u;i<var_index;i++)
                gates.emplace_back("RY",M_PI/2,i,controls_new1);

        }
        else if(c1_allzero){
            return;
        }
        else{//some 1 some 0
            general_qg_generation(gates,tt1,var_index-1,controls_new1);
        }
    }
    else if(c0_allzero){
        //--check one cofactor----
        std::vector<uint32_t> controls_new1;
        std::copy(controls.begin(), controls.end(), back_inserter(controls_new1)); 
        auto ctrl1 = var_index*2 + 1; //negetive control: /2 ---> index %2 ---> sign
        controls_new1.emplace_back(ctrl1);
        if(c1_allone){
            //---add H gates---
            for(auto i=0u;i<var_index;i++)
                gates.emplace_back("RY",M_PI/2,i,controls_new1);

        }
        else if(c1_allzero){
            return;
        }
        else{//some 1 some 0
            general_qg_generation(gates,tt1,var_index-1,controls_new1);
        }
    }
    else{//some 0 some 1 for c0
        
        std::vector<uint32_t> controls_new1;
        std::copy(controls.begin(), controls.end(), back_inserter(controls_new1)); 
        auto ctrl1 = var_index*2 + 1; //negetive control: /2 ---> index %2 ---> sign
        controls_new1.emplace_back(ctrl1);
        if(c1_allone){
            general_qg_generation(gates,tt0,var_index-1,controls_new0);
            //---add H gates---
            for(auto i=0u;i<var_index;i++)
                gates.emplace_back("RY",M_PI/2,i,controls_new1);

        }
        else if(c1_allzero){
            general_qg_generation(gates,tt0,var_index-1,controls_new0);
            //return;
        }
        else{//some 1 some 0
            general_qg_generation(gates,tt0,var_index-1,controls_new0);
            general_qg_generation(gates,tt1,var_index-1,controls_new1);
        }
    }

}

template<typename Network>
void qc_generation(Network & net, std::vector < std::tuple < std::string,double,uint32_t,std::vector<uint32_t> > >
 gates){
    for(const auto [name,angle,target_id,controls]: gates){
        if(name=="RY"){
            
            if(controls.size()==0){
                auto temp = gate_base(gate_set::rotation_y, angle);
                net.add_gate(temp, target_id);
            }
            else if( (controls.size()==1) && (angle==M_PI) ){
                
                    if(controls[0]%2 == 0){
                        net.add_gate(gate::pauli_x, controls[0]/2);
                        net.add_gate(gate::cx,controls[0]/2,target_id);
                        net.add_gate(gate::pauli_x, controls[0]/2);
                    }
                    else
                    {
                        net.add_gate(gate::cx,controls[0]/2,target_id);
                    }
                           
            }
            else {//we have multi control probability gate
                unsigned nlines = controls.size()+1;
                auto tt = kitty::create<kitty::dynamic_truth_table>(nlines-1);
                uint32_t tt_idx_set=0;
                //kitty::set_bit(tt,pow(2,nlines-1)-1);
            
                std::vector<qubit_id> q_map;
                net.add_gate(gate_base(gate_set::rotation_y, angle/2), target_id);
                auto idx=0;
                for(const auto ctrl:controls){
                    if(ctrl%2 == 1){//negative control
                        //net.add_gate(gate::pauli_x, ctrl/2)
                        tt_idx_set += pow(2,idx);
                    }
                    idx++;
                    q_map.emplace_back(ctrl/2);
                }
                q_map.emplace_back(target_id);
                tt_idx_set += pow(2,nlines-1);
                kitty::set_bit(tt,tt_idx_set);
                detail::decomposition_mcz(net,q_map,tt);
                //for(const auto ctrl:controls){
                    //if(ctrl%2 == 1)//negative control
                        //net.add_gate(gate::pauli_x, ctrl/2);
                //}
                net.add_gate(gate_base(gate_set::rotation_y, -angle/2), target_id);

            }//end multi control
        }//end RY
        else if(name=="H"){
            
            if(controls.size()==0)
                net.add_gate(gate::hadamard, target_id);
            else{//we have multi control probability gate
                unsigned nlines = controls.size()+1;
                auto tt = kitty::create<kitty::dynamic_truth_table>(nlines-1);
                uint32_t tt_idx_set=0;
                std::vector<qubit_id> q_map;
                net.add_gate(gate_base(gate_set::rotation_y, M_PI/4), target_id);
                auto idx=0;
                for(const auto ctrl:controls){
                    if(ctrl%2 == 1){//negative control
                        //net.add_gate(gate::pauli_x, ctrl/2)
                        tt_idx_set += pow(2,idx);
                    }
                    idx++;
                    q_map.emplace_back(ctrl/2);
                }
                q_map.emplace_back(target_id);
                tt_idx_set += pow(2,nlines-1);
                kitty::set_bit(tt,tt_idx_set);
                detail::decomposition_mcz(net,q_map,tt);
                // for(const auto ctrl:controls){
                //     if(ctrl%2 == 1)//negative control
                //         net.add_gate(gate::pauli_x, ctrl/2);
                // }
                net.add_gate(gate_base(gate_set::rotation_y, -M_PI/4), target_id);
            }//end multi control
        }//end H

    }// end gates

}

template<typename Network>
void qsp_ownfunction(Network & net, const std::string &tt_str)
{
    std::vector < std::tuple < std::string,double,uint32_t,std::vector<uint32_t> > > gates; // gate name, angle, target_id, controls:id and sign /2 and %2
    auto tt_vars = int(log2(tt_str.size()));
    
    kitty::dynamic_truth_table tt(tt_vars);
    kitty::create_from_binary_string( tt, tt_str);

    auto var_idx = tt_vars-1;
 
    std::vector<uint32_t> cs;
    general_qg_generation(gates,tt,var_idx,cs);
    //detail::control_line_cancelling(gates,tt_vars);
    for(auto i=0u;i<gates.size();i++){
        std::cout<<"gates: "<<i<<"\n";
        std::cout<<std::get<0> (gates[i]) <<std::endl;
        std::cout<<std::get<1> (gates[i]) <<std::endl;
        std::cout<<std::get<2> (gates[i]) <<std::endl;
        for(auto j=0;j< (std::get<3> (gates[i])).size();j++)
            std::cout<<(std::get<3> (gates[i])) [j] <<std::endl;
    }

    detail::extract_multiplex_gates(net,tt_vars,gates);
    
}

template<typename Network>
void qsp_allone_first(Network & net, const std::string &tt_str)
{
    std::vector < std::tuple < std::string,double,uint32_t,std::vector<uint32_t> > > gates; // gate name, angle, target_id, controls:id and sign /2 and %2
    auto tt_vars = int(log2(tt_str.size()));
    
    //first tt
    kitty::dynamic_truth_table tt(tt_vars);
    kitty::create_from_binary_string(tt, tt_str);
    auto ones = kitty::count_ones(tt);

    auto tt_new = kitty::create<kitty::dynamic_truth_table>(tt_vars);
    for(auto i=0u;i<ones;i++)
        kitty::set_bit(tt_new,i);

    auto var_idx = tt_vars-1;
    std::vector<uint32_t> cs;
    general_qg_generation(gates,tt_new,var_idx,cs);
    detail::control_line_cancelling(gates,tt_vars);
    for(auto i=0u;i<gates.size();i++){
        std::cout<<"gates: "<<i<<"\n";
        std::cout<<std::get<0> (gates[i]) <<std::endl;
        std::cout<<std::get<1> (gates[i]) <<std::endl;
        std::cout<<std::get<2> (gates[i]) <<std::endl;
        for(auto j=0;j< (std::get<3> (gates[i])).size();j++)
            std::cout<<(std::get<3> (gates[i])) [j] <<std::endl;
    }
    qc_generation(net,gates);
    
    std::vector<qubit_id> qubits(tt_vars);
	std::iota(qubits.begin(), qubits.end(), 0u);
    std::vector<uint32_t> perm;
    
    for(auto i=0u;i<tt_str.size();i++)
        if(tt_str[i]=='1')
            perm.emplace_back(i);
    for(auto i=ones;i<tt_str.size();i++)
        perm.emplace_back(i);

    for(auto i=0u;i<perm.size();i++)
    std::cout<<perm[i]<<" ";

    detail::tbs_unidirectional(net, qubits, perm,ones);
                          
}


template<class Network>
void qsp(Network& network, const std::string &tt_str, qsp_params params = {})
{
    assert(tt_str.size() <= pow(2,6));
    const uint32_t num_qubits = std::log2(tt_str.size());
    for (auto i = 0u; i < num_qubits; ++i) {
		network.add_qubit();
	}
	
	switch (params.strategy) {
		case qsp_params::strategy::allone_first:
			qsp_allone_first(network, tt_str);
			break;
		case qsp_params::strategy::ownfunction:
			qsp_ownfunction(network, tt_str);
			break;
	}
}

} // namespace tweedledum end
