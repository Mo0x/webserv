#include "CGIHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>

std::string CGIHandler::execute(const std::string &cgiPath,
                                const std::vector<std::string> &args,
                                const std::vector<std::string> &env,
                                const std::string &input)
{
    // Create pipe for sending input to the CGI process (parent writes, child reads)
    int pipeIn[2];
    if (pipe(pipeIn) < 0)
    {
        throw std::runtime_error("Failed to create input pipe.");
    }
    
    // Create pipe for reading the output from the CGI process (child writes, parent reads)
    int pipeOut[2];
    if (pipe(pipeOut) < 0)
    {
        { // Ensure resources are closed before throwing.
            close(pipeIn[0]);
            close(pipeIn[1]);
        }
        throw std::runtime_error("Failed to create output pipe.");
    }
    
    pid_t pid = fork();
    if (pid < 0)
    {
        { // Clean up pipes on fork failure.
            close(pipeIn[0]);
            close(pipeIn[1]);
            close(pipeOut[0]);
            close(pipeOut[1]);
        }
        throw std::runtime_error("Failed to fork process for CGI.");
    }
    
    if (pid == 0)
    {
        // Child process begins here.
        if (dup2(pipeIn[0], STDIN_FILENO) < 0)
        {
            exit(1);
        }
        if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
        {
            exit(1);
        }
        
        // Close unused pipe ends in the child.
        if (close(pipeIn[0]) < 0) { }
        if (close(pipeIn[1]) < 0) { }
        if (close(pipeOut[0]) < 0) { }
        if (close(pipeOut[1]) < 0) { }
        
        // Prepare argument list for execve.
        std::vector<char*> argv;
        { // Add CGI executable path as first argument.
            argv.push_back(const_cast<char*>(cgiPath.c_str()));
        }
        for (size_t i = 0; i < args.size(); ++i)
        {
            argv.push_back(const_cast<char*>(args[i].c_str()));
        }
        argv.push_back(NULL);
        
        // Prepare environment variables.
        std::vector<char*> envp;
        for (size_t i = 0; i < env.size(); ++i)
        {
            envp.push_back(const_cast<char*>(env[i].c_str()));
        }
        envp.push_back(NULL);
        
        // Execute the CGI script.
        execve(cgiPath.c_str(), argv.data(), envp.data());
        // If execve returns, an error occurred.
        exit(1);
    }
    else
    {
        // Parent process begins here.
        if (close(pipeIn[0]) < 0) { }
        if (close(pipeOut[1]) < 0) { }
        
        // Write the provided input to the child process's STDIN.
        ssize_t write_ret = write(pipeIn[1], input.c_str(), input.size());
        if (write_ret < 0)
        {
            { // Clean up and throw error.
                close(pipeIn[1]);
                close(pipeOut[0]);
            }
            throw std::runtime_error("Failed to write to CGI input pipe.");
        }
        if (close(pipeIn[1]) < 0) { } // Signal EOF to child.
        
        // Read the output from the child process.
        std::stringstream output;
        char buffer[1024];
        ssize_t bytesRead;
        while ((bytesRead = read(pipeOut[0], buffer, sizeof(buffer))) > 0)
        {
            output.write(buffer, bytesRead);
        }
        if (bytesRead < 0)
        {
            { // Clean up and throw error.
                close(pipeOut[0]);
            }
            throw std::runtime_error("Failed to read from CGI output pipe.");
        }
        if (close(pipeOut[0]) < 0) { }
        
        // Wait for the child process to finish.
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            throw std::runtime_error("Failed to wait for CGI child process.");
        }
        
        // Return the captured output.
        return output.str();
    }
}
