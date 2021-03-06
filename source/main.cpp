#include <iostream>
using namespace std;

#include "TO_Decoder.h"
#include "Signature.h"
#include "GateStringSparse.h"
#include "GateSigInterface.h"
#include "LCL_Maths.h"
#include "LCL_Bool.h"
#include "BoolMat.h"
#include "BMSparse.h"
#include "Interface_SigBMS.h"
#include "Interface_BMSGSS.h"
#include "LCL_Int.h"
#include "Bool_Signature.h"
#include "LCL_Mat_GF2.h"
#include "GateSynthesisMatrix.h"
#include "SQC_Circuit.h"
#include "LCL_ConsoleOut.h"
using namespace LCL_ConsoleOut;
#include "PhasePolynomial.h"
#include "TO_Maps.h"
#include "WeightedPolynomial.h"
#include "Utils.h"
#include "TO_CircuitGenerators.h"

#include <climits>
#include <vector>
#include <cmath>
#include <ctime>
#include <string>
#include <sstream>
#include <fstream>
#include <ctime>
#include <utility>

int main(int argc, char* argv[]) {
    g_output_filename.clear();
    g_csv_filename.clear();

    srand(time(NULL));

    if(argc>1) {
        string this_command = argv[1];
        if(!this_command.compare("circuit")&&(argc>=3)) {
            cout << "Optimize a Clifford + T circuit." << endl;
            SQC_Circuit* this_circuit = NULL;
            TO_Decoder this_decoder = TODD;
            string circuit_filename = argv[2];
            LOut() << "Input filename = " << circuit_filename << endl;
            string this_input_filetype = "tfc";
            bool this_verify = 0;
            string this_tool_selector="r";
            bool this_tool_feedback=1;

            for(int i = 3; i < argc; i++) {
                string this_option = argv[i];
                if((this_option[0]=='-')&&((i+1)<argc)) {
                    string this_value = argv[i+1];
                    char this_option_char = this_option[1];
                    switch(this_option_char) {
                        case 'f':
                            // filetype
                            this_input_filetype = this_value;
                            break;
                        case 'o':
                            // output filename
                            g_output_filename = this_value;
                            break;
                        case 'a':
                            // algorithm
                            g_algorithm = this_value;
                            break;
                        case 'v':
                            // verify?
                            this_verify = atoi(this_value.c_str());
                            break;
                        case 'h':
                            //no. had. ancs
                            g_Hadamard_ancillas = atoi(this_value.c_str());
                            break;
                        case 's':
                            //TOOL selector
                            this_tool_selector = this_value;
                            break;
                        case 'b':
                            //TOOL feedback param
                            this_tool_feedback = atoi(this_value.c_str());
                            break;
                        case 'r':
                            //n_RM
                            g_Reed_Muller_max = atoi(this_value.c_str());
                            break;
                    }
                    i++;
                }
            }

            // Load circuit depending on filetype
            if(!this_input_filetype.compare("sqc")) {
                this_circuit = new SQC_Circuit();
                this_circuit->Load(circuit_filename.c_str());
            } else if(!this_input_filetype.compare("tfc")) {
                this_circuit = SQC_Circuit::LoadTFCFile(circuit_filename.c_str());
            }

            // Determine algorithm to be used
            if(!g_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::TODD)) {
                this_decoder = TODD;
            } else if(!g_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::TOOL)) {
                if(!this_tool_selector.compare("g")) {
                    this_tool_feedback?this_decoder = TOOL_F_G:this_decoder = TOOL_WF_G;
                } else if(!this_tool_selector.compare("lg")) {
                    this_tool_feedback?this_decoder = TOOL_F_LG:this_decoder = TOOL_WF_LG;
                } else if(!this_tool_selector.compare("r")) {
                    this_tool_feedback?this_decoder = TOOL_F_R:this_decoder = TOOL_WF_R;
                }
            } else if(!g_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::DAFT_GUESS)) {
                this_decoder = RE;
            } else if(!g_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::REED_MULLER)) {
                this_decoder = ReedMullerSynthesis2;
            }

            if(this_circuit) {
                LOut() << "Input circuit:" << endl;
                this_circuit->Print();
                SQC_Circuit result = SQC_Circuit::UniversalOptimize(*this_circuit,this_decoder);
                LOut() << "Output circuit:" << endl;
                result.Print();
                LOut() << "Gate distributions:" << endl;
                LOut() << "Input:" << endl;
                this_circuit->PrintOperatorDistribution();
                LOut() << "Output:" << endl;
                result.PrintOperatorDistribution();
                if(this_verify) {
                    VerifyOptimization2(*this_circuit,result);
                }
                if(!g_output_filename.empty()) {
                    result.Save(g_output_filename.c_str());
                }
            }



            // Delete circuits
            if(this_circuit) {
                delete this_circuit;
                this_circuit = NULL;
            }
        } else if(!this_command.compare("signature")&&(argc>2)) {
            string circuit_filename = argv[2];
            int option_maxRM = 5;

            string option_filename;
            string option_algorithm;
            bool option_feedback = true;
            for(int i = 3; i < argc; i++) {
                string this_option = argv[i];
                if((this_option[0]=='-')&&((i+1)<argc)) {
                    string this_value = argv[i+1];
                    char this_option_char = this_option[1];
                    switch(this_option_char) {
                        case 'r':
                            option_maxRM = atoi(this_value.c_str());
                            break;
                        case 'a':
                            option_algorithm = this_value;
                            g_algorithm = option_algorithm;
                            break;
                        case 'f':
                            if((!this_value.compare("true"))||(!this_value.compare("1"))) {
                                option_feedback = true;
                            } else if((!this_value.compare("false"))||(!this_value.compare("0"))) {
                                option_feedback = false;
                            }
                            break;
                        case 'c':
                            g_csv_filename = this_value;
                            break;
                    }
                    i++;
                }
            }
            g_lempel_feedback = option_feedback;
            g_Reed_Muller_max = option_maxRM;
            g_output_filename = option_filename;
            Signature this_sig;
            char first_char = circuit_filename.c_str()[0];
            if(first_char==':') {
                cout << "Structured signature." << endl;
                this_sig = CircuitGenerator(circuit_filename.substr(1));
            } else {
                cout << "Signature from file: " << circuit_filename << endl;
                this_sig = Signature::SigFromFile(circuit_filename.c_str());
            }
            GateStringSparse out(this_sig.get_n());
            cout << "Input signature polynomial:" << endl;
            this_sig.print();
            cout << endl << "Synthesis algorithm: ";
            if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::LEMPEL_LEAST_GREEDY)) {
                cout << "TOOL least-greedy" << endl;
                GateStringSparse tempout = TOOL(this_sig,option_maxRM,LempelSelector_LeastGreedy,option_feedback);
                out.assign(tempout);
            } else if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::REED_MULLER)) {
                cout << "RM" << endl;
                GateStringSparse tempout = ReedMullerSynthesis2(this_sig);
                out.assign(tempout);
            } else if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::LEMPEL_GREEDY)) {
                cout << "TOOL greedy" << endl;
                GateStringSparse tempout = TOOL(this_sig,option_maxRM,LempelSelector_Greedy,option_feedback);
                out.assign(tempout);
            } else if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::DAFT_GUESS)) {
                cout << "RE" << endl;
                GateStringSparse tempout = GateSigInterface::SigToGSS(this_sig);
                if(!g_csv_filename.empty()) {
                    ofstream my_csv(g_csv_filename.c_str(),iostream::app);
                    int n = this_sig.get_n();
                    my_csv << SYNTHESIS_ALGORITHM_ID::DAFT_GUESS << "," << n << "," << g_random_circuit_seed << "," << tempout.weight(true) << endl;
                    my_csv.close();
                }
                out.assign(tempout);
            } else if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::LEMPEL_RANDOM)) {
                cout << "TOOL random" << endl;
                GateStringSparse tempout = TOOL(this_sig,option_maxRM,LempelSelector_Random,option_feedback);
                out.assign(tempout);
            } else if(!option_algorithm.compare(SYNTHESIS_ALGORITHM_TAG::TODD)) {
                cout << "TODD" << endl;
                GateStringSparse tempout = TODD(this_sig);
                out.assign(tempout);
            }
            result_analysis(this_sig,out);
            cout << "Output phase polynomial:" << endl;
            out.print();
        } else if(!this_command.compare("help")) {
            ifstream myfile("README.txt");
            while(!myfile.eof()) {
                char this_line[1000];
                myfile.getline(this_line,1000);
                cout << this_line << endl;
            }
            myfile.close();
        }
    }
    return 0;
}
