#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <windows.h>

// Parameters to be modified

const int n_kloss = 7;
const int n_kcomb = 7;
int klosses[n_kloss] = {-1000, -800, -600, -400, -200, -100, 0};
int kcombs[n_kcomb] = {0, 100, 200, 400, 600, 800, 1000};

const int n_kdens = 9;
const int n_kbump = 9;
int kdenses[n_kdens] = {-400, -200, -100, -10, 0, 10, 100, 200, 400};
int kbumps[n_kbump] = {-400, -200, -100, -10, 0, 10, 100, 200, 400};



int main() {
    //Matlab code generation

    std::ostringstream code;

    /*
    int i = 1;
    for (float kcomb : kcombs) {
        for (float kloss : klosses) {
            code << "log_data(:,:," << i << ") = importdata('log_gamma_=0.750000_alpha_=0.150000_kloss_=" << kloss << "_kcomb_=" << kcomb << "_kdens_=0_kbump_=0.txt');\n";
            i++;
        }
    }
    */
    int i = 1;
    for (float kbump : kbumps) {
        for (float kdens : kdenses) {
            code << "log_data(:,:," << i << ") = importdata('log_gamma_=0.750000_alpha_=0.150000_kloss_=-100_kcomb_=600_kdens_=" << kdens << "_kbump_=" << kbump << ".txt');\n";
            i++;
        }
    }

    std::cout << code.str() << std::endl;
}