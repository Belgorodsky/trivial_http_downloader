#include <array>
#include <iostream>
#include <tuple>

#include <arpa/inet.h>

#include "params_parser.hpp"
#include "download_session.hpp"

int main(int argc , char *argv[])
{
	// луче использовать boost program_options
	auto params_parser_uptr = std::make_unique<params_parser>(argc,argv);

	std::cout << "host: " << params_parser_uptr->host() << '\n'
		  << "scheme: " << params_parser_uptr->scheme() << '\n'
		  << "remote_filename: " << params_parser_uptr->remote_filename() << '\n'
		  << "local_filename: " << params_parser_uptr->local_filename() << '\n';

	if (params_parser_uptr->scheme() != "http")
	{
		std::cerr << params_parser_uptr->scheme() << " is not supported\n";
		std::cout << "http is only supported\n";
		return 1;
	}

	download_session ds(std::move(params_parser_uptr));
	ds.connect();
	ds.send_http_request();
	ds.recv_http_response();
}
