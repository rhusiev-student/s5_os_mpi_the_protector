#ifndef INCLUDE_ARGS_HPP_
#define INCLUDE_ARGS_HPP_

#include <stdexcept>
#include <string>
#include <vector>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

class ArgsException : public std::runtime_error {
  public:
    using runtime_error::runtime_error;
};

void assert_file_exist(const std::string &f_name);

class Args {
  public:
    Args();
    Args(int ac, char **av);
    explicit Args(const std::string &command);
    explicit Args(const std::vector<std::string> &command);
    explicit Args(boost::program_options::options_description description);
    Args(int ac, char **av,
         boost::program_options::options_description description);
    Args(const std::string &command,
         boost::program_options::options_description description);
    Args(const std::vector<std::string> &command,
         boost::program_options::options_description description);

    Args(const Args &) = default;
    Args &operator=(const Args &) = delete;
    Args(Args &&) = default;
    Args &operator=(Args &&) = delete;
    ~Args() = default;

    po::parsed_options parsed;
    boost::program_options::variables_map var_map{};
    boost::program_options::options_description opt_descr;
};

#endif // INCLUDE_ARGS_HPP_
