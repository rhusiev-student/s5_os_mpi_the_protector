#include <string>
#include <vector>
#include "./args.hpp"

#include <boost/program_options/parsers.hpp>

const char *EMPTY[] = {""};

Args::Args()
    : parsed(po::command_line_parser(1, EMPTY)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run()) {
    opt_descr.add_options()("help,h", "Show help message");
}

Args::Args(int ac, char **av)
    : Args() // Delegate constructor
{
    opt_descr.add_options()("help,h", "Show help message");
    parsed = po::command_line_parser(ac, av)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}

Args::Args(const std::string &command)
    : Args() // Delegate constructor
{
    opt_descr.add_options()("help,h", "Show help message");
    std::vector<std::string> args = boost::program_options::split_unix(command);
    parsed = po::command_line_parser(args)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}

Args::Args(const std::vector<std::string> &command)
    : Args() // Delegate constructor
{
    opt_descr.add_options()("help,h", "Show help message");
    parsed = po::command_line_parser(command)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}

Args::Args(boost::program_options::options_description description)
    : opt_descr(std::move(description)),
      parsed(po::command_line_parser(1, EMPTY)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run()) {}

Args::Args(int ac, char **av,
           boost::program_options::options_description description)
    : Args(description) {
    parsed = po::command_line_parser(ac, av)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}

Args::Args(const std::string &command,
           boost::program_options::options_description description)
    : Args(description) {
    std::vector<std::string> args = boost::program_options::split_unix(command);
    parsed = po::command_line_parser(args)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}

Args::Args(const std::vector<std::string> &command,
           boost::program_options::options_description description)
    : Args(description) {
    parsed = po::command_line_parser(command)
                 .options(opt_descr)
                 .allow_unregistered()
                 .run();
    po::store(parsed, var_map);
}
