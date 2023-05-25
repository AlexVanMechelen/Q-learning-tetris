#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <windows.h>

// Parameters to be modified

//const int MAX_WINDOWS = 30;
bool execute = true;

const int n_epsilon = 13;
float epsilons[n_epsilon] = {0.0f, 0.05f, 0.1f, 0.15f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f ,1.0f};

int main() {
    bool error_flag = false;
    int windowCount = 0;

    //iterate over all combinations of epsilon and alpha

    for (float epsilon : epsilons) {

        std::cout << "epsilon = " << epsilon << std::endl;

        // Open the source file
        std::ifstream inputFile("tetris-Qlearning.cpp");
        if (!inputFile) {
            std::cerr << "Error opening source file." << std::endl;
            return 1;
        }

        // Create a new file to store the modified contents
        std::string filename = "tetris-Qlearning_epsilon_"+ std::to_string(epsilon) + ".cpp";

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
                // Replace the value of epsilon
                size_t pos = line.find("double EPSILON = 0;");
                if (pos != std::string::npos) {
                    std::string epsilon_line = "double EPSILON = "+ std::to_string(epsilon) + ";";
                    line.replace(pos, 19, epsilon_line);
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
                std::string exeFilename = "tetris-Qlearning_epsilon_"+ std::to_string(epsilon) + ".exe";
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
            std::string exeFilename = "tetris-Qlearning_epsilon_"+ std::to_string(epsilon) + "_alpha_" + std::to_string(alpha) + ".exe";

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


    // Close the input file stream
    if (error_flag == false) {
        std::cout << "File modified successfully." << std::endl;
    } else {
        std::cout << "File compilation failed." << std::endl;
    }
    //Matlab code to generation

    std::ostringstream code;
    int i = 1;
    for (float epsilon : epsilons) {
        code << "log_data(:,:," << i << ") = importdata('log_epsilon_=" << std::to_string(epsilon) << ".txt');\n";
        i++;
    }

    //std::string filename = "log_epsilon_="+ std::to_string(first_epsilon) + ".txt";

    std::cout << code.str() << std::endl;


    return 0;
}
