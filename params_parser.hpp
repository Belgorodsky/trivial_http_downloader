#pragma once
#include <string>
#include <ostream>

class params_parser
{
	public:
		params_parser(int argc , char *argv[]);

		const std::string &scheme() const { return m_scheme; }
		const std::string &host() const { return m_host; }
		const std::string &remote_filename() const
		{ return m_remote_filename; }
		const std::string &local_filename() const
		{ return m_local_filename; }

	private:
		void parse(int argc , char *argv[]);
		void print_help(std::string_view exec_name, std::ostream &os);

	private:
		std::string m_scheme;
		std::string m_host;
		std::string m_remote_filename;
		std::string m_local_filename;
};
