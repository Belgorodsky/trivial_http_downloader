#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include "aliases.hpp"

struct dns_resolver
{
	using addri_uptr = shrtr::addri_uptr;

	template< class charT>
	static std::vector<addri_uptr> get_addinfo_list(
		const charT *hostname,
		const charT *servname
	)
	{
		return get_addinfo_list(
			std::basic_string<charT>{hostname},
			std::basic_string<charT>{servname}
		);
	}

	template< class charT,
		  class traits = std::char_traits<charT>,    // basic_string::traits_type
		  class Alloc = std::allocator<charT>        // basic_string::allocator_type
		>
	static std::vector<addri_uptr> get_addinfo_list(
		const std::basic_string<charT,traits,Alloc> &hostname,
		const charT *servname
	)
	{
		return get_addinfo_list(
			hostname,
			std::basic_string<charT>{servname}
		);
	}

	template< class charT,
		  class traits = std::char_traits<charT>,    // basic_string::traits_type
		  class Alloc = std::allocator<charT>        // basic_string::allocator_type
		>
	static std::vector<addri_uptr> get_addinfo_list(
		const charT *hostname,
		const std::basic_string<charT,traits,Alloc> &servname
	)
	{
		return get_addinfo_list(
			std::basic_string<charT>{hostname},
			servname
		);
	}

	template< class charT,
		  class traits = std::char_traits<charT>,    // basic_string::traits_type
		  class Alloc = std::allocator<charT>        // basic_string::allocator_type
		>
	static std::vector<addri_uptr> get_addinfo_list(
		const std::basic_string<charT,traits,Alloc> &hostname,
		const std::basic_string<charT,traits,Alloc> &servname
	)
	{
		std::vector<addri_uptr> hosts;
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		addrinfo *res0{nullptr};
		if (auto er = getaddrinfo(
					   hostname.c_str(),
					   servname.c_str(),
					   &hints,
					   &res0
					 );
		    er)
		{
			std::cerr << gai_strerror(er) << '\n';
		}
		else
		{
			auto res = res0;
			hosts.emplace_back(res, &freeaddrinfo);
			for (res = res->ai_next; res; res = res->ai_next)
			{
				hosts.emplace_back(res,[](auto v){});
			}
		}

		return hosts;
	}
};
