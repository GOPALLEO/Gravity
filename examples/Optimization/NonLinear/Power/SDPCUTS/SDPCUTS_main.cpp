//
// Created by kbestuzheva on 12/11/17.
//
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstring>
#include <fstream>
#include "../PowerNet.h"
#include <gravity/solver.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef USE_OPT_PARSER
#include <optionParser.hpp>
#endif

using namespace std;
using namespace gravity;


/* main */
int main (int argc, char * argv[]) {
    int output = 0;
    bool sdp_cuts = true;
    
    bool current_from = true, llnc=true, current_to=true, loss=true, loss_bounds=true, current;
    
    size_t num_bags = 0;
    string num_bags_s = "100";
    string solver_str = "ipopt";
    string sdp_cuts_s = "yes";
    string current_from_s = "yes";
    string orig_s = "yes";
    string current_to_s="yes";
    string lazy_s = "no";
    bool lazy_bool = false;
    SolverType solv_type = ipopt;
    double tol = 1e-6;
    string mehrotra = "no";
    
    
    string fname = string(prj_dir)+"/data_sets/Power/nesta_case5_pjm.m";
    
    // create a OptionParser with options
    
#ifdef USE_OPT_PARSER
    op::OptionParser opt;
    opt.add_option("h", "help",
                   "shows option help"); // no default value means boolean options, which default value is false
    opt.add_option("f", "file", "Input file name", fname);
    opt.add_option("s", "solver", "Solvers: ipopt/cplex/gurobi, default = ipopt", solver_str);
    opt.add_option("b", "numbags", "Number of bags per iteration", num_bags_s);
    opt.add_option("If", "current_from", "add from current constraints", current_from_s);
    opt.add_option("o", "original", "add original variables and linking constraints", orig_s);
    opt.add_option("It", "current_to", "add to current constraints", current_to_s);
    opt.add_option("lz", "lazy", "Generate 3d SDP cuts in a lazy fashion, default = no", lazy_s);
    // parse the options and verify that all went well. If not, errors and help will be shown
    bool correct_parsing = opt.parse_options(argc, argv);
    
    if (!correct_parsing) {
        return EXIT_FAILURE;
    }
    
    fname = opt["f"];
    bool has_help = op::str2bool(opt["h"]);
    if (has_help) {
        opt.show_help();
        exit(0);
    }
    solver_str = opt["s"];
    if (solver_str.compare("gurobi")==0) {
        solv_type = gurobi;
    }
    else if(solver_str.compare("cplex")==0) {
        solv_type = cplex;
    }else if(solver_str.compare("Mosek")==0) {
        solv_type = _mosek;
    }
    lazy_s = opt["lz"];
    if (lazy_s.compare("no")==0) {
        lazy_bool = false;
    }
    else if(lazy_s.compare("yes")==0) {
        lazy_bool = true;
    }
    
    current_from_s = opt["If"];
    if (current_from_s.compare("no")==0) {
        current_from = false;
    }
    else {
        current_from = true;
    }
    bool add_original = true;
    
    orig_s = opt["o"];
    if (orig_s.compare("no")==0) {
        add_original = false;
    }
    else {
        add_original = true;
    }
    
    
    
    current_to_s = opt["It"];
    if (current_to_s.compare("no")==0) {
        current_to = false;
    }
    else {
        current_to = true;
    }
    num_bags = atoi(opt["b"].c_str());
    
    current_from=true;
    current_to=true;
    loss=true;
#else
    if(argc==2){
        fname=argv[1];    }
    else{
        fname=string(prj_dir)+"/data_sets/Power/nesta_case5_pjm.m";
    }
    
    
    
    
    
#endif
    
    
    cout << "\nnum bags = " << num_bags << endl;
    
    // double total_time_start = get_wall_time();
    PowerNet grid;
    grid.readgrid(fname);
    grid.update_ref_bus();
    
    grid.get_tree_decomp_bags();
    auto bags_3d=grid.decompose_bags_3d();
    
    
    /* Grid Stats */
    auto nb_gen = grid.get_nb_active_gens();
    auto nb_lines = grid.get_nb_active_arcs();
    auto nb_buses = grid.get_nb_active_nodes();
    DebugOn("nb active gens = " << nb_gen << endl);
    DebugOn("nb active lines = " << nb_lines << endl);
    DebugOn("nb active buses = " << nb_buses << endl);
    
    /** Sets */
    auto node_pairs = grid.get_node_pairs();
    auto node_pairs_chord = grid.get_node_pairs_chord(bags_3d);
    if (grid._tree || !grid.add_3d_nlin || !sdp_cuts) {
        node_pairs_chord = node_pairs;
    }
    auto nodes = indices(grid.nodes);
    auto arcs = indices(grid.arcs);
    auto gens = indices(grid.gens);
    auto gen_nodes = grid.gens_per_node();
    auto out_arcs = grid.out_arcs_per_node();
    auto in_arcs = grid.in_arcs_per_node();
    
    /* Grid Parameters */
    auto pg_min = grid.pg_min.in(gens);
    auto pg_max = grid.pg_max.in(gens);
    auto qg_min = grid.qg_min.in(gens);
    auto qg_max = grid.qg_max.in(gens);
    auto c1 = grid.c1.in(gens);
    auto c2 = grid.c2.in(gens);
    auto c0 = grid.c0.in(gens);
    auto pl = grid.pl.in(nodes);
    auto ql = grid.ql.in(nodes);
    auto gs = grid.gs.in(nodes);
    auto bs = grid.bs.in(nodes);
    auto b = grid.b.in(arcs);
    auto g = grid.g.in(arcs);
    auto as = grid.as.in(arcs);
    auto ch = grid.ch.in(arcs);
    auto tr = grid.tr.in(arcs);
    auto th_min = grid.th_min.in(node_pairs);
    auto th_max = grid.th_max.in(node_pairs);
    auto g_ft = grid.g_ft.in(arcs);
    auto g_ff = grid.g_ff.in(arcs);
    auto g_tt = grid.g_tt.in(arcs);
    auto g_tf = grid.g_tf.in(arcs);
    auto b_ft = grid.b_ft.in(arcs);
    auto b_ff = grid.b_ff.in(arcs);
    auto b_tf = grid.b_tf.in(arcs);
    auto b_tt = grid.b_tt.in(arcs);
    auto S_max = grid.S_max.in(arcs);
    auto v_max = grid.v_max.in(nodes);
    auto v_min = grid.v_min.in(nodes);
    auto w_max = grid.w_max.in(nodes);
    auto w_min = grid.w_min.in(nodes);
    auto tan_th_min = grid.tan_th_min.in(node_pairs);
    auto tan_th_max = grid.tan_th_max.in(node_pairs);
    auto wr_min = grid.wr_min.in(node_pairs_chord);
    auto wr_max = grid.wr_max.in(node_pairs_chord);
    auto wi_min = grid.wi_min.in(node_pairs_chord);
    auto wi_max = grid.wi_max.in(node_pairs_chord);
    auto lij_min=grid.lij_min.in(arcs);
    auto lij_max=grid.lij_max.in(arcs);
    auto cc=grid.cc.in(arcs);
    auto dd=grid.dd.in(arcs);
    auto ch_half=grid.ch_half.in(arcs);
    auto arcs_inductive=grid.arcs_inductive_only();
    auto lji_min=grid.lji_min.in(arcs);
    auto lji_max=grid.lji_max.in(arcs);
    
    
    double upper_bound = grid.solve_acopf(ACRECT);
    //    auto OPF=build_ACOPF(grid, ACRECT);
    //    solver<> OPFUB(OPF, solv_type);
    //    OPFUB.run(output = 5, tol);
    //    auto upper_bound=OPF->get_obj_val();
    
    /** Build model */
    Model<> SDP("SDP Model");
    // Model<> SDPOA("SDP-OA Model");
    
    /** Variables */
    /* Power generation variables */
    var<> Pg("Pg", pg_min, pg_max);
    var<> Qg ("Qg", qg_min, qg_max);
    //  var<> eta("eta", 0, 1);
    SDP.add(Pg.in(gens),Qg.in(gens));
    // SDP.add(eta.in(range(0,0)));
    //    SDPOA.add(Pg.in(gens),Qg.in(gens));
    //    SDPOA.add(eta.in(range(0,0)));
    
    /* Power flow variables */
    var<> Pf_from("Pf_from", -1.*S_max,S_max);
    var<> Qf_from("Qf_from", -1.*S_max,S_max);
    var<> Pf_to("Pf_to", -1.*S_max,S_max);
    var<> Qf_to("Qf_to", -1.*S_max,S_max);
    
    SDP.add(Pf_from.in(arcs), Qf_from.in(arcs),Pf_to.in(arcs),Qf_to.in(arcs));
    
    
    
    /* Real part of Wij = ViVj */
    var<>  R_Wij("R_Wij", wr_min, wr_max);
    /* Imaginary part of Wij = ViVj */
    var<>  Im_Wij("Im_Wij", wi_min, wi_max);
    /* Magnitude of Wii = Vi^2 */
    var<>  Wii("Wii", w_min, w_max);
    SDP.add(Wii.in(nodes),R_Wij.in(node_pairs_chord),Im_Wij.in(node_pairs_chord));
    
    
    
    /* Initialize variables */
    R_Wij.initialize_all(1.0);
    Wii.initialize_all(1.00);
    
    current = true;
    var<> lij("lij", lij_min,lij_max);
    var<> lji("lji", lji_min,lji_max);
    
    if(current){
        SDP.add(lij.in(arcs),lji.in(arcs));
    }
    
    
    
    //SDPOA.min(eta(0));
    
    //    Constraint<> obj_UB("obj_UB");
    //    obj_UB  = (product(c1,Pg) + product(c2,pow(Pg,2)) + sum(c0))-eta(0)*upper_bound;
    //    SDP.add(obj_UB.in(range(0,0)) <= 0);
    auto obj=(product(c1,Pg) + product(c2,pow(Pg,2)) + sum(c0))/upper_bound;
    SDP.min(obj);
    
    
    
    
    /** Constraints */
    auto bag_size = bags_3d.size();
    Constraint<> SDP3("SDP_3D");
    //      Constraint<> SDPD("SDPD");
    if(!grid._tree && grid.add_3d_nlin && sdp_cuts)
    {
        DebugOn("\nNum of bags = " << bag_size << endl);
        DebugOn("Adding 3d determinant polynomial cuts\n");
        auto R_Wij_ = R_Wij.pairs_in_bags(bags_3d, 3);
        auto Im_Wij_ = Im_Wij.pairs_in_bags(bags_3d, 3);
        auto Wii_ = Wii.in_bags(bags_3d, 3);
        
        
        
        SDP3 = 2 * R_Wij_[0] * (R_Wij_[1] * R_Wij_[2] + Im_Wij_[1] * Im_Wij_[2]);
        SDP3 -= 2 * Im_Wij_[0] * (R_Wij_[2] * Im_Wij_[1] - Im_Wij_[2] * R_Wij_[1]);
        SDP3 -= (pow(R_Wij_[0], 2) + pow(Im_Wij_[0], 2)) * Wii_[2];
        SDP3 -= (pow(R_Wij_[1], 2) + pow(Im_Wij_[1], 2)) * Wii_[0];
        SDP3 -= (pow(R_Wij_[2], 2) + pow(Im_Wij_[2], 2)) * Wii_[1];
        SDP3 += Wii_[0] * Wii_[1] * Wii_[2];
        if (lazy_bool) {
            SDP.add_lazy(SDP3.in(range(0, bag_size-1)) >= 0);
        }
        else {
            SDP.add(SDP3.in(range(0, bag_size-1)) >= 0);
            DebugOn("Number of 3d determinant cuts = " << SDP3.get_nb_instances() << endl);
        }
        
    }
    
    /** Constraints */
    /* Second-order cone constraints */
    Constraint<> SOC("SOC");
    SOC = pow(R_Wij, 2) + pow(Im_Wij, 2) - Wii.from(node_pairs_chord)*Wii.to(node_pairs_chord);
    //SDP.add(SOC.in(node_pairs_chord) == 0, true, "on/off", true);
    SDP.add(SOC.in(node_pairs_chord) <= 0);
    
    
    /* Flow conservation */
    Constraint<> KCL_P("KCL_P");
    KCL_P  = sum(Pf_from, out_arcs) + sum(Pf_to, in_arcs) + pl - sum(Pg, gen_nodes) + gs*Wii;
    SDP.add(KCL_P.in(nodes) == 0);
    
    
    Constraint<> KCL_Q("KCL_Q");
    KCL_Q  = sum(Qf_from, out_arcs) + sum(Qf_to, in_arcs) + ql - sum(Qg, gen_nodes) - bs*Wii;
    SDP.add(KCL_Q.in(nodes) == 0);
    
    
    /* AC Power Flow */
    Constraint<> Flow_P_From("Flow_P_From");
    Flow_P_From = Pf_from - (g_ff*Wii.from(arcs) + g_ft*R_Wij.in_pairs(arcs) + b_ft*Im_Wij.in_pairs(arcs));
    SDP.add(Flow_P_From.in(arcs) == 0);
    
    
    Constraint<> Flow_P_To("Flow_P_To");
    Flow_P_To = Pf_to - (g_tt*Wii.to(arcs) + g_tf*R_Wij.in_pairs(arcs) - b_tf*Im_Wij.in_pairs(arcs));
    SDP.add(Flow_P_To.in(arcs) == 0);
    
    
    Constraint<> Flow_Q_From("Flow_Q_From");
    Flow_Q_From = Qf_from - (g_ft*Im_Wij.in_pairs(arcs) - b_ff*Wii.from(arcs) - b_ft*R_Wij.in_pairs(arcs));
    SDP.add(Flow_Q_From.in(arcs) == 0);
    
    
    Constraint<> Flow_Q_To("Flow_Q_To");
    Flow_Q_To = Qf_to + b_tt*Wii.to(arcs) + b_tf*R_Wij.in_pairs(arcs) + g_tf*Im_Wij.in_pairs(arcs);
    SDP.add(Flow_Q_To.in(arcs) == 0);
    
    /* Phase Angle Bounds constraints */
    Constraint<> PAD_UB("PAD_UB");
    PAD_UB = Im_Wij.in(node_pairs);
    PAD_UB <= tan_th_max*R_Wij.in(node_pairs);
    SDP.add(PAD_UB.in(node_pairs));
    
    
    Constraint<> PAD_LB("PAD_LB");
    PAD_LB =  Im_Wij.in(node_pairs);
    PAD_LB >= tan_th_min*R_Wij.in(node_pairs);
    SDP.add(PAD_LB.in(node_pairs));
    
    
    /* Thermal Limit Constraints */
    Constraint<> Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from = pow(Pf_from, 2) + pow(Qf_from, 2);
    Thermal_Limit_from <= pow(S_max,2);
    //SDP.add(Thermal_Limit_from.in(arcs));
    SDP.add(Thermal_Limit_from.in(arcs));
    
    
    
    
    Constraint<> Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to = pow(Pf_to, 2) + pow(Qf_to, 2);
    Thermal_Limit_to <= pow(S_max,2);
    SDP.add(Thermal_Limit_to.in(arcs));
    
    //    func<> theta_L = atan(min(Im_Wij.get_lb().in(node_pairs)/R_Wij.get_ub().in(node_pairs),Im_Wij.get_lb().in(node_pairs)/R_Wij.get_lb().in(node_pairs)));
    //    func<> theta_U = atan(max(Im_Wij.get_ub().in(node_pairs)/R_Wij.get_lb().in(node_pairs),Im_Wij.get_ub().in(node_pairs)/R_Wij.get_ub().in(node_pairs)));
    //    func<> phi=(theta_U.in(node_pairs)+theta_L.in(node_pairs))/2.0;
    //    func<> del=(theta_U.in(node_pairs)-theta_L.in(node_pairs))/2.0;
    
    
    
    //    Constraint<> LNC1("LNC1");
    //    LNC1 += (sqrt(Wii.get_lb().from(node_pairs))+sqrt(Wii.get_ub().from(node_pairs)))*(sqrt(Wii.get_lb().to(node_pairs))+sqrt(Wii.get_ub().to(node_pairs)))*(Im_Wij.in(node_pairs)*sin(phi.in(node_pairs)) + R_Wij.in(node_pairs)*cos(phi.in(node_pairs)));
    //
    //    LNC1 -=sqrt(Wii.get_ub().to(node_pairs))*cos(del.in(node_pairs))*(sqrt(Wii.get_lb().to(node_pairs))+sqrt(Wii.get_ub().to(node_pairs)))*Wii.from(node_pairs);
    //
    //    LNC1 -=sqrt(Wii.get_ub().from(node_pairs))*cos(del.in(node_pairs))*(sqrt(Wii.get_lb().from(node_pairs))+sqrt(Wii.get_ub().from(node_pairs)))*Wii.to(node_pairs);
    //
    //    LNC1-=sqrt(Wii.get_ub().from(node_pairs))*sqrt(Wii.get_ub().to(node_pairs))*cos(del)*(sqrt(Wii.get_lb().from(node_pairs))*
    //                                                                                        sqrt(Wii.get_lb().to(node_pairs)) - sqrt(Wii.get_ub().from(node_pairs))*sqrt(Wii.get_ub().to(node_pairs)));
    //   // SDP.add(LNC1.in(node_pairs) >= 0);
    //
    //
    //    Constraint<> LNC2("LNC2");
    //    LNC2 += (sqrt(Wii.get_lb().from(node_pairs))+sqrt(Wii.get_ub().from(node_pairs)))*(sqrt(Wii.get_lb().to(node_pairs))+sqrt(Wii.get_ub().to(node_pairs)))*(sin(phi.in(node_pairs))*Im_Wij.in(node_pairs) + cos(phi.in(node_pairs))*R_Wij.in(node_pairs));
    //    LNC2 -=sqrt(Wii.get_lb().to(node_pairs))*cos(del.in(node_pairs))*(sqrt(Wii.get_lb().to(node_pairs))+sqrt(Wii.get_ub().to(node_pairs)))*Wii.from(node_pairs);
    //    LNC2 -=sqrt(Wii.get_lb().from(node_pairs))*cos(del.in(node_pairs))*(sqrt(Wii.get_lb().from(node_pairs))+sqrt(Wii.get_ub().from(node_pairs)))*Wii.to(node_pairs);
    //    LNC2 -=sqrt(Wii.get_lb().from(node_pairs))*sqrt(Wii.get_lb().to(node_pairs))*cos(del.in(node_pairs))*(sqrt(Wii.get_ub().from(node_pairs))*
    //                                                                                                       sqrt(Wii.get_ub().to(node_pairs))-sqrt(Wii.get_lb().from(node_pairs))*sqrt(Wii.get_lb().to(node_pairs)));
    //   // SDP.add(LNC2.in(node_pairs) >= 0);
    //
    
    /* Lifted Nonlinear Cuts */
    Constraint<> LNC1("LNC1");
    LNC1 += (grid.v_min.from(node_pairs)+grid.v_max.from(node_pairs))*(grid.v_min.to(node_pairs)+grid.v_max.to(node_pairs))*(grid.sphi*Im_Wij + grid.cphi*R_Wij);
    LNC1 -= grid.v_max.to(node_pairs)*grid.cos_d*(grid.v_min.to(node_pairs)+grid.v_max.to(node_pairs))*Wii.from(node_pairs);
    LNC1 -= grid.v_max.from(node_pairs)*grid.cos_d*(grid.v_min.from(node_pairs)+grid.v_max.from(node_pairs))*Wii.to(node_pairs);
    LNC1 -= grid.v_max.from(node_pairs)*grid.v_max.to(node_pairs)*grid.cos_d*(grid.v_min.from(node_pairs)*grid.v_min.to(node_pairs) - grid.v_max.from(node_pairs)*grid.v_max.to(node_pairs));
    SDP.add(LNC1.in(node_pairs) >= 0);
    
    Constraint<> LNC2("LNC2");
    LNC2 += (grid.v_min.from(node_pairs)+grid.v_max.from(node_pairs))*(grid.v_min.to(node_pairs)+grid.v_max.to(node_pairs))*(grid.sphi*Im_Wij + grid.cphi*R_Wij);
    LNC2 -= grid.v_min.to(node_pairs)*grid.cos_d*(grid.v_min.to(node_pairs)+grid.v_max.to(node_pairs))*Wii.from(node_pairs);
    LNC2 -= grid.v_min.from(node_pairs)*grid.cos_d*(grid.v_min.from(node_pairs)+grid.v_max.from(node_pairs))*Wii.to(node_pairs);
    LNC2 += grid.v_min.from(node_pairs)*grid.v_min.to(node_pairs)*grid.cos_d*(grid.v_min.from(node_pairs)*grid.v_min.to(node_pairs) - grid.v_max.from(node_pairs)*grid.v_max.to(node_pairs));
    SDP.add(LNC2.in(node_pairs) >= 0);
    
    
    if(current){
        param<Cpx> T("T"), Y("Y"), Ych("Ych");
        var<Cpx> L_from("L_from"), Wij("Wij");
        T.real_imag(cc.in(arcs), dd.in(arcs));
        Y.real_imag(g.in(arcs), b.in(arcs));
        Ych.set_imag(ch_half.in(arcs));
        
        
        L_from.set_real(lij.in(arcs));
        Wij.real_imag(R_Wij.in_pairs(arcs), Im_Wij.in_pairs(arcs));
        var<Cpx> Sij("Sij"), Sji("Sji");
        Sij.real_imag(Pf_from.in(arcs), Qf_from.in(arcs));
        Sji.real_imag(Pf_to.in(arcs), Qf_to.in(arcs));
        
        
        Constraint<Cpx> I_from("I_from");
        
        I_from=(Y+Ych)*(conj(Y)+conj(Ych))*Wii.from(arcs)-T*Y*(conj(Y)+conj(Ych))*conj(Wij)-conj(T)*conj(Y)*(Y+Ych)*Wij+pow(tr,2)*Y*conj(Y)*Wii.to(arcs)-pow(tr,2)*L_from;
        SDP.add_real(I_from.in(arcs)==0);
        
        var<Cpx> L_to("L_to");
        L_to.set_real(lji.in(arcs));
        
        Constraint<Cpx> I_to("I_to");
        I_to=pow(tr,2)*(Y+Ych)*(conj(Y)+conj(Ych))*Wii.to(arcs)-conj(T)*Y*(conj(Y)+conj(Ych))*Wij-T*conj(Y)*(Y+Ych)*conj(Wij)+Y*conj(Y)*Wii.from(arcs)-pow(tr,2)*L_to;
        SDP.add_real(I_to.in(arcs)==0);
        
        Constraint<> I_from_Pf("I_from_Pf");
        I_from_Pf=lij*Wii.from(arcs)-pow(tr,2)*(pow(Pf_from,2) + pow(Qf_from,2));
        SDP.add(I_from_Pf.in(arcs)==0, true);
        
        
        
        Constraint<> I_to_Pf("I_to_Pf");
        I_to_Pf=lji*Wii.to(arcs)-(pow(Pf_to,2) + pow(Qf_to, 2));
        SDP.add(I_to_Pf.in(arcs)==0, true);
        
    }
    
    
    double solver_time_start;
    
    solver<> SDPOPF(SDP,solv_type);
    solver_time_start = get_wall_time();
    
    SDPOPF.run(output = 5, tol = 1e-6);
    double solver_time_end = get_wall_time();
    double solver_time=solver_time_end-solver_time_start;
    double gap=999, lower_bound=999;
    // SDP.print_solution();
    // SDP.print();
    SDP.print_constraints_stats(tol);
    SDP.print_nonzero_constraints(tol,true);
    if(SDP._status==0)
    {
        lower_bound = SDP.get_obj_val()*upper_bound;
        
        gap = 100*(upper_bound - lower_bound)/upper_bound;
    }
    
    //    auto solve_time = solver_time_end - solver_time_start;
    //
    //    string out = "\nDATA_OPF, " + grid._name + ", " + to_string(nb_buses) + ", " + to_string(nb_lines) +", " + to_string(lower_bound) + ", " + to_string(-numeric_limits<double>::infinity()) + ", " + to_string(solve_time) + ", LocalOptimal, " + to_string(total_time);
    //    DebugOn(out <<endl);
    DebugOn("Final Gap = " << to_string(gap) << "%."<<endl);
    DebugOn("Upper bound = " << to_string(upper_bound) << "."<<endl);
    DebugOn("Lower bound = " << to_string(lower_bound) << "."<<endl);
    SDP.print();
    string result_name=string(prj_dir)+"/results_SDP/"+grid._name+".txt";
    
    ofstream fout(result_name.c_str());
    fout<<grid._name<<"\t"<<std::fixed<<std::setprecision(5)<<gap<<"\t"<<std::setprecision(5)<<upper_bound<<"\t"<<std::setprecision(5)<<lower_bound<<"\t"<<std::setprecision(5)<<solver_time<<endl;
    fout.close();
    
    //    SDP.run_obbt();
    //    SDP.reset_constrs();
    //    solver<> SDPLB1(SDP,solv_type);
    //
    //    auto status = SDPLB1.run(output = 5, tol);
    //    SDP.print_constraints_stats(tol);
    //    bool print_only_relaxed;
    //    SDP.print_nonzero_constraints(tol,print_only_relaxed=true);
    //
    //    //        SDP.print_solution();
    //
    //    //        SDP.print();
    //
    //    if(status==0)
    //    {
    //        total_time_end = get_wall_time();
    //        total_time = total_time_end - total_time_start;
    //        DebugOn("\nResults: " << grid._name << " " << to_string(SDP.get_obj_val()) << " " <<endl);
    //        SDP.print_constraints_stats(tol);
    //
    //        DebugOn("Initial Gap Nonlinear = " << to_string(gap) << "%."<<endl);
    //        lower_bound=SDP.get_obj_val()*upper_bound;
    //        gap = 100*(upper_bound - lower_bound)/upper_bound;
    //        DebugOn("Final Gap = " << to_string(gap) << "%."<<endl);
    //        DebugOn("Upper bound = " << to_string(upper_bound) << "."<<endl);
    //        DebugOn("Lower bound = " << to_string(lower_bound) << "."<<endl);
    //        DebugOn("Time\t"<<total_time<<endl);
    //    }
    //    else {
    //        DebugOn("WARNING: Relaxation did not converge!"<<endl);
    //    }
    

}
