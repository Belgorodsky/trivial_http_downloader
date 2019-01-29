#include "params_parser.hpp"

#include <iostream>

params_parser::params_parser(int argc , char *argv[])
{
	parse(argc,argv);
}

void params_parser::parse(int argc , char *argv[])
{
	if (argc < 2)
	{
		print_help(argv[0], std::cerr);
		return;
	}
	std::string_view url = argv[1];
	std::string_view ofilename = 2 < argc ? argv[2] : std::string_view();
	std::string_view scheme{"http"};
	std::string_view host;
	std::string_view remote_filename;

	constexpr char scheme_delim[] ("://");
	constexpr char end_of_host_delim[] ("/");

	std::string_view::size_type pos = 0;

	auto scheme_delim_pos = url.find(scheme_delim, pos);
	if (scheme_delim_pos != std::string_view::npos)
	{
		scheme = url.substr(pos, scheme_delim_pos - pos);
		pos = scheme_delim_pos + std::size(scheme_delim) - 1;
	}

	auto end_of_host_delim_pos = url.find(end_of_host_delim, pos);
	if (end_of_host_delim_pos != std::string_view::npos)
	{
		host = url.substr(pos, end_of_host_delim_pos - pos); 
		pos = end_of_host_delim_pos + std::size(end_of_host_delim) - 1;
		remote_filename = pos < url.length() ? url.substr(pos) :
			std::string_view{};
	}
	else
	{
		host = url.substr(pos);
	}

	if (ofilename.empty())
	{
		constexpr char delim[] ("/");
		if(auto last_delim = remote_filename.rfind(delim);
			last_delim != std::string_view::npos)
		{
			pos = last_delim + std::size(delim) - 1;
			ofilename = remote_filename.substr(pos);
		}
	}

	if (ofilename.empty())
	{
		std::cerr << "cannot deduce local filename\n";
		print_help(argv[0], std::cerr);
	}

	m_scheme.assign(scheme.data(), scheme.length());
	m_host.assign(host.data(), host.length());

	m_remote_filename.assign(
			remote_filename.data(), 
			remote_filename.length()
	);
	m_local_filename.assign(
		ofilename.data(),
		ofilename.length()
	);
}


void params_parser::print_help(std::string_view exec_name, std::ostream &os)
{
	os << "Usage: " << exec_name
		<< " <url> [<local_filename>]\n";
}
