#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <windows.h>

// Parameters to be modified

//const int MAX_WINDOWS = 30;
bool execute = true;

const int n_kloss = 7;
const int n_kcomb = 7;
int klosses[n_kloss] = {-1000, -800, -600, -400, -200, -100, 0};
int kcombs[n_kcomb] = {0, 100, 200, 400, 600, 800, 1000};

int main() {
    bool error_flag = false;
    int windowCount = 0;

    //iterate over all combinations of kloss and kcomb
    for (int kcomb : kcombs) {
        for (int kloss : klosses) {

            std::cout << "kcomb = " << kcomb << " kloss = " << kloss << std::endl;

            // Open the source file
            std::ifstream inputFile("tetris-Qlearning.cpp");
            if (!inputFile) {
                std::cerr << "Error opening source file." << std::endl;
                return 1;
            }

            // Create a new file to store the modified contents
            std::string filename = "tetris-Qlearning_kloss_"+ std::to_string(kloss) + "_kcomb_" + std::to_string(kcomb) + ".cpp";

            std::ofstream outputFile(filename);
            if (!outputFile) {
                std::cerr << "Error creating output file." << std::endl;
                return 1;
            }

            std::string line;
            int lineNumber = 1;
            while (std::getline(inputFile, line)) {

                // Check if it is line 32
                if (lineNumber == 32) {
                    // Replace the value of kloss
                    size_t pos = line.find("int kloss = -100;");
                    if (pos != std::string::npos) {
                        std::string kloss_line = "int kloss = "+ std::to_string(kloss) + ";";
                        line.replace(pos, 19, kloss_line);
                    }
                }
                // Check if it is line 33
                if (lineNumber == 33) {
                    // Replace the value of kcomb
                    size_t pos = line.find("int kcomb = 600;");
                    if (pos != std::string::npos) {
                        std::string kcomb_line = "int kcomb = "+ std::to_string(kcomb) + ";";
                        line.replace(pos, 19, kcomb_line);
                    }
                }

                // Write the line to the output file
                outputFile << line << std::endl;

                lineNumber++;
            }
            // Close the file streams
            inputFile.close();
            outputFile.close();

            // Compile the modified CPP file using Visual Studio as the compiler
            std::string compileCommand = "cl.exe " + filename;  // Change the command as per your requirements
            int compileResult = system(compileCommand.c_str());

            if (compileResult == 0) {
                std::cout << "Compilation successful." << std::endl;

                if(execute) {
                    std::string exeFilename = "tetris-Qlearning_kloss_"+ std::to_string(kloss) + "_kcomb_" + std::to_string(kcomb) + ".exe";
                    std::string command = "cmd.exe /c start " + exeFilename;
                    system(command.c_str());
                }
                /*
                std::cout << "windowCount: " << windowCount << std::endl;
                
                // Check if the maximum number of windows have been opened
                if (windowCount >= MAX_WINDOWS) {
                    // Wait for a window to close before opening a new one
                    std::cout << "Window count limit reached. Waiting for a window to close." << std::endl;
                    DWORD processId;
                    do {
                        Sleep(1000); // Wait for 1 second
                        HWND hwnd = FindWindow(NULL, "Command Prompt");
                        GetWindowThreadProcessId(hwnd, &processId);
                    } while (processId != 0);

                    std::cout << "A window has closed. Resuming execution." << std::endl;
                }
                
                // Execute the program
                std::string exeFilename = "tetris-Qlearning_kloss_"+ std::to_string(kloss) + "_kcomb_" + std::to_string(kcomb) + ".exe";

                // Create process information structures
                STARTUPINFO si;
                PROCESS_INFORMATION pi;

                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));

                // Build the command to open a new command prompt window and execute the .exe file
                std::string command = "cmd.exe /c start " + exeFilename;

                // Use the CreateProcess function to execute the command
                if (CreateProcess(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
                    std::cout << "Execution successful." << std::endl;

                    windowCount++;
                } else {
                    std::cerr << "Execution failed." << std::endl;
                }
                */
            } else {
                std::cerr << "Compilation failed." << std::endl;
                error_flag = true;
                
            }
        }
    }


    // Close the input file stream
    if (error_flag == false) {
        std::cout << "File modified successfully." << std::endl;
    } else {
        std::cout << "File compilation failed." << std::endl;
    }

    
    //Matlab code generation

    std::ostringstream code;

    int i = 1;
    for (float kcomb : kcombs) {
        for (float kloss : klosses) {
            code << "log_data(:,:," << i << ") = importdata('log_gamma_=0.750000_alpha_=0.150000_kloss_=" << kloss << "_kcomb_=" << kcomb << "_kdens_=0_kbump_=0.txt');\n";
            i++;
        }
    }

    /*
    std::ostringstream code2;
    std::ostringstream code3;

    for (int i = 1; i <= n_kcomb*n_kloss; i++) {
        code2 << "avgHeight" << i << " = str2double(cell2mat(log_data" << i << "(14, 5)));\n";
    }

    code3 << "z = [";
    for (int i = 1; i <= n_kcomb*n_kloss; i++) {
        code3 << "avgHeight" << i;
        if (i !=  n_kcomb*n_kloss) {
            code3 << " ";
            if (i % n_kloss == 0) {
                code3 << "\n    ";
            }
        }
    }
    code3 << "];\n";

    std::cout << code2.str() << std::endl;
    std::cout << code3.str() << std::endl;

    */
    std::cout << code.str() << std::endl;

    return 0;
}
