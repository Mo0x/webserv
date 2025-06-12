#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <string>
#include <vector>

// CGIHandler provides a method to execute a CGI script via fork/execve.
// It sets up pipes to pass the request body to the CGI process and to capture its output.
class CGIHandler
{
    public:
        // Executes a CGI script.
        // - cgiPath: Full path to the CGI executable (e.g., /usr/bin/php-cgi)
        // - args: Vector of arguments for the CGI script.
        //         Note: args[0] is typically the script itself.
        // - env: Vector of environment variables (as "KEY=VALUE" strings).
        // - input: The full request body (after unchunking, if needed).
        // Returns the output produced by the CGI script.
        static std::string execute(const std::string &cgiPath,
                                   const std::vector<std::string> &args,
                                   const std::vector<std::string> &env,
                                   const std::string &input);
};

#endif
