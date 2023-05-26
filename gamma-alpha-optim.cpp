#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <windows.h>

// Parameters to be modified

//const int MAX_WINDOWS = 30;
bool execute = true;

//const int n_gamma = 10;
//const int n_alpha = 10;
//float gammas[n_gamma] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
//float alphas[n_alpha] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

const int n_gamma = 9;
const int n_alpha = 7;
float gammas[n_gamma] = {0.6f, 0.625f, 0.65f, 0.675f, 0.7f, 0.725f, 0.75f, 0.775f, 0.8f};
float alphas[n_alpha] = {0.01f, 0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f};

int main() {
    bool error_flag = false;
    int windowCount = 0;

    //iterate over all combinations of gamma and alpha
    for (float alpha : alphas) {
        for (float gamma : gammas) {

            std::cout << "alpha = " << alpha << " gamma = " << gamma << std::endl;

            // Open the source file
            std::ifstream inputFile("tetris-Qlearning.cpp");
            if (!inputFile) {
                std::cerr << "Error opening source file." << std::endl;
                return 1;
            }

            // Create a new file to store the modified contents
            std::string filename = "tetris-Qlearning_gamma_"+ std::to_string(gamma) + "_alpha_" + std::to_string(alpha) + ".cpp";

            std::ofstream outputFile(filename);
            if (!outputFile) {
                std::cerr << "Error creating output file." << std::endl;
                return 1;
            }

            std::string line;
            int lineNumber = 1;
            while (std::getline(inputFile, line)) {

                // Check if it is line 26
                if (lineNumber == 26) {
                    // Replace the value of gamma
                    size_t pos = line.find("float gamma = 0.75f;");
                    if (pos != std::string::npos) {
                        std::string gamma_line = "float gamma = "+ std::to_string(gamma) + "f";
                        line.replace(pos, 19, gamma_line);
                    }
                }
                // Check if it is line 27
                if (lineNumber == 27) {
                    // Replace the value of alpha
                    size_t pos = line.find("float alpha = 0.15f;");
                    if (pos != std::string::npos) {
                        std::string alpha_line = "float alpha = "+ std::to_string(alpha) + "f";
                        line.replace(pos, 19, alpha_line);
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
                    std::string exeFilename = "tetris-Qlearning_gamma_"+ std::to_string(gamma) + "_alpha_" + std::to_string(alpha) + ".exe";
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
                std::string exeFilename = "tetris-Qlearning_gamma_"+ std::to_string(gamma) + "_alpha_" + std::to_string(alpha) + ".exe";

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
    //Matlab code to generation

    std::ostringstream code;
    std::ostringstream code2;
    std::ostringstream code3;

    int i = 1;
    for (float alpha : alphas) {
        for (float gamma : gammas) {
            code << "log_data(:,:," << i << ") = importdata('log_gamma_=" << std::to_string(gamma) << "_alpha_=" << std::to_string(alpha) << "_kloss_=-100_kcomb_=500_kdens_=0_kbump_=0.txt').textdata;\n";
            i++;
        }
    }

    for (int i = 1; i <= n_alpha*n_gamma; i++) {
        code2 << "avgHeight" << i << " = str2double(cell2mat(log_data" << i << "(14, 5)));\n";
    }

    code3 << "z = [";
    for (int i = 1; i <= n_alpha*n_gamma; i++) {
        code3 << "avgHeight" << i;
        if (i !=  n_alpha*n_gamma) {
            code3 << " ";
            if (i % n_gamma == 0) {
                code3 << "\n    ";
            }
        }
    }
    code3 << "];\n";

    std::cout << code.str() << std::endl;
    std::cout << code2.str() << std::endl;
    std::cout << code3.str() << std::endl;

    return 0;
}
