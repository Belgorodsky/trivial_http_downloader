#include "download_session.hpp"
#include "dns_resolver.hpp"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <array>
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <limits>
#include <charconv>
#include <memory>

download_session::download_session(std::unique_ptr<params_parser>&& pz) : 
	m_content_length(std::numeric_limits<size_t>::max()),
	m_pz(std::move(pz)),
	m_sock(nullptr,[](auto){}),
	m_file(nullptr,[](auto){}),
	m_filemem(nullptr, [](auto){})
{
#if defined(__APPLE__)
	signal(SIGPIPE, SIG_IGN);
	m_ignore_sigpipe = 0;
#else
	m_ignore_sigpipe = MSG_NOSIGNAL;
#endif
}

void download_session::connect()
{
	auto addris = dns_resolver::get_addinfo_list(
			m_pz->host(),
		        m_pz->scheme()
	);
	if (!addris.empty())
	{
		m_sock = shrtr::sock_uptr(
				new(std::nothrow) int,
				[](int *s)
				{
					if (s)
				       	{
						shutdown(*s, SHUT_RDWR);
						close(*s);
						delete s;
					}
			       	}
		);
	}
	else
	{
		std::cerr << "cannot connect\n";
		m_er_status = -1;
	}

	for (auto && addr : addris)
	{
		if (m_sock)
		{
			*m_sock = socket(
				addr->ai_family,
				addr->ai_socktype,
				addr->ai_protocol
			);

			if (*m_sock < 0)
			{
				m_er_status = errno;
				std::cerr << "cannot create socket\n"
					  << strerror(errno) << '\n';
			}
			else if (
				 ::connect(
					 *m_sock,
					 addr->ai_addr,
					 addr->ai_addrlen
					) < 0
			)
			{
				m_er_status = errno;
				std::cerr << "cannot connect to ";
				print_address(std::cerr, addr);
				std::cerr << strerror(errno) << '\n';
			}
			else
			{
				m_er_status = 0;
				std::cout << "connected to ";
				print_address(std::cout, addr);
				break;
			}
		}
	}
}

void download_session::send_http_request()
{
	if (!m_er_status)
	{
		constexpr char get_slash[]("GET /");
		constexpr char sp_http1_1_rn[](" HTTP/1.1\r\n");
		constexpr char host_colon_sp[]("HOST: ");
		constexpr char endofreq[]("\r\n\r\n");

		constexpr size_t base_size = std::size(get_slash) + 
			std::size(sp_http1_1_rn) + std::size(host_colon_sp) + 
			std::size(endofreq) - 3;

		std::string request;
		request.reserve(
				base_size + 
				m_pz->remote_filename().length() +
				m_pz->host().length()
			       );

		request = get_slash + m_pz->remote_filename() +
			sp_http1_1_rn + host_colon_sp + m_pz->host() +
			endofreq;

		if (	send(
				*m_sock,
				request.data(),
				request.length(),
				m_ignore_sigpipe
			) < 0
		   )
		{
			m_er_status = errno;
			std::cerr << "cannot send http request: " 
				  << strerror(errno) << '\n';
		}
	}
}

void download_session::recv_http_response()
{
	if (!m_er_status)
	{
		bool header_received = false;
		std::string::size_type pos = 0;
		m_response.resize(PIPE_BUF, '\0');
		constexpr char header_end[]("\r\n\r\n");
		std::string::size_type header_end_pos = std::string::npos; 
		while (!header_received)
		{
			auto bytes = recv(
					*m_sock,
					m_response.data() + pos,
					m_response.size() - pos,
					m_ignore_sigpipe
				);
			if (!bytes)
				break;
			if (bytes < 0)
			{
				m_er_status = errno;
				std::cerr 
					<< "cannot recv http request, errno: " 
					<< m_er_status << '\n';
				return;
			}
			header_end_pos = m_response.find(header_end);
			if (!header_received)
			{
				header_received = header_end_pos != std::string::npos;
			}
			pos += bytes;
			m_response.resize(m_response.size() + pos);
		}
		
		std::string_view header = m_response;
		auto content_pos = header_end_pos + std::size(header_end) - 1;
		std::string_view content = 
			content_pos < header.length() ?
		       	header.substr(content_pos, pos - content_pos) :
			std::string_view{};
		header = header.substr(0, header_end_pos);
		if (parse_header(header))
		{
			if (m_content_length != 
					std::numeric_limits<size_t>::max())
			{
				recv_content(content);
			}
			else
			{
				recv_content_nommap(content);
			}
		}

	}
}

bool download_session::parse_header(std::string_view header)
{
	if (header.empty())
	{
		std::cerr << "header is empty\n";
	}
	constexpr char status_subkey[]("HTTP");
	constexpr char status_subval[]("200");
	constexpr char content_length[]("Content-Length:");
	constexpr char header_del[]("\r\n");
	constexpr char wses[](" \t\f\v\n\r");

	std::string_view::size_type next_del = 0;
	auto prev = next_del;

	while(next_del != std::string_view::npos)
	{
		next_del = header.find(header_del, prev);
		auto line = header.substr(prev, next_del - prev);
		auto key_delim_pos = line.find_first_of(wses);
		auto val_start_pos = 
			line.find_first_not_of(
				wses,
				key_delim_pos
			);
		auto key = line.substr(0,key_delim_pos);
		auto val = line.substr(val_start_pos); 
		if (
			status_subkey == key.
			substr(0, std::size(status_subkey) - 1) &&
			status_subval != val.
			substr(0, std::size(status_subval) - 1)
		)
		{
			std::cerr << "status: " << val << '\n';
			m_er_status = -1;
			return false;
		}
		else if(
			content_length == key.
			substr(0, std::size(content_length) - 1)
		)
		{
			size_t res;
			if (auto[p, ec] = std::from_chars(
						val.data(),
					       	val.data() + val.length(),
					       	res
						);
				       	ec == std::errc()
			)
			{
				m_content_length = res;
			}
		}
		prev = next_del + std::size(header_del) - 1;
	}
	return true;
}

void download_session::recv_n_flush_rest(
	void (download_session::*flusher)(std::string_view)
) 
{
	constexpr char header_end[]("\r\n\r\n");
	m_response.clear();
	while (m_content_cur_pos < m_content_length)
	{
		auto bytes = recv(
				*m_sock,
				m_response.data(),
				m_response.size(),
				m_ignore_sigpipe
			);
		if (bytes < 0)
		{
			m_er_status = errno;
			std::cerr 
				<< "cannot recv http request, errno: " 
				<< m_er_status << '\n';
			return;
		}

		std::string_view content(m_response.data(), bytes);

		(this->*flusher)(content);
		m_progr_printer.print(
			std::cout, 
			m_content_cur_pos,
			m_content_length
		);
		m_response.resize(m_content_length, '\0');
	}


}

void download_session::recv_content(std::string_view first_bytes)
{
	if (!m_file && !m_filemem && !init_file())
	{
		 return;
	}

	flush_some(first_bytes);
	m_progr_printer.print(
		std::cout, 
		m_content_cur_pos,
		m_content_length
	);
#if defined(__APPLE__)
	fcntl(*m_sock, F_GLOBAL_NOCACHE, 1);
#endif
	recv_n_flush_rest(&download_session::flush_some);
}


void download_session::recv_content_nommap(std::string_view first_bytes)
{
	if (!m_file && !init_file_nommap())
	{
		 return;
	}

	flush_some_nommap(first_bytes);
	recv_n_flush_rest(&download_session::flush_some_nommap);
}

void download_session::flush_some(std::string_view first_bytes)
{
	char *dst = static_cast<char*>(m_filemem.get()) + 
		m_content_cur_pos;
	std::uninitialized_copy_n(
		first_bytes.data(),
		first_bytes.length(),
		dst
	);

//	std::cout << "download_session::flush_some " <<
//	m_content_cur_pos << '\n';

	m_content_cur_pos += first_bytes.length();
//	std::cout << "download_session::flush_some " <<
//	m_content_cur_pos << '\n';
}

void download_session::flush_some_nommap(std::string_view first_bytes)
{
	auto bytes = write(
				*m_file, 
				first_bytes.data(),
				first_bytes.length()
			); 
	if (bytes < 0
	)
	{
		std::cout << '\n';
		std::cerr << "cannot write: "
			  << strerror(errno)
			  << '\n';
		return;
	}

	std::cout << m_content_cur_pos << std::endl;

	m_content_cur_pos += bytes;
}

bool download_session::init_file()
{
	m_file = shrtr::file_uptr(
		new(std::nothrow) int,
		[](int *f) { if (f) { close(*f); delete f; } }
	);
	
	if (!m_file)
	{
		std::cerr << "cannot alloc file descriptor\n";
	       	return false;
	}

	*m_file = open(
			m_pz->local_filename().c_str(),
			O_RDWR | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR
	);

	if (*m_file < 0)
	{
		m_er_status = errno;
		std::cerr << "cannot open to write'"
			  << m_pz->local_filename()
			  << "' :"
			  << strerror(m_er_status)
			  << '\n';
		return false;
	}

//	if (m_content_length != std::numeric_limits<size_t>::max())
	{

		ftruncate(*m_file, m_content_length);
		size_t pagesize = getpagesize();
		size_t mmap_size = (m_content_length / pagesize + 1) *
			pagesize;

		auto filemem_raw = mmap(
				nullptr,
			       	mmap_size,
			       	PROT_READ | PROT_WRITE,
				MAP_SHARED,
				*m_file,
				0
			);
		
		if (MAP_FAILED == filemem_raw)
		{
			m_er_status = errno;
			std::cerr << "cannot mmap: "
				<< strerror(errno)
				<< '\n';
			return false;
		}

		m_filemem = shrtr::filemem_upr(
			filemem_raw,
			[cl = m_content_length](auto fm)
			{
				if (munmap(fm, cl) < 0)
				{
					std::cerr << "cannot munmap: "
					<< strerror(errno)
					<< '\n';
				}	
			}

		);
	}

	return true;
}

bool download_session::init_file_nommap()
{
	m_file = shrtr::file_uptr(
		new(std::nothrow) int,
		[](int *f) { if (f) { close(*f); delete f; } }
	);
	
	if (!m_file)
	{
		std::cerr << "cannot alloc file descriptor\n";
	       	return false;
	}

	*m_file = open(
			m_pz->local_filename().c_str(),
			O_RDWR | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR
	);

	if (*m_file < 0)
	{
		m_er_status = errno;
		std::cerr << "cannot open to write '"
			  << m_pz->local_filename()
			  << "' :"
			  << strerror(m_er_status)
			  << '\n';
		return false;
	}

	return true;
}


void download_session::print_address(std::ostream& os, const shrtr::addri_uptr &addr)
{
	switch (addr->ai_family)
	{
		case AF_INET:
		{
			auto sai = reinterpret_cast<sockaddr_in*>(addr->ai_addr);
			os << inet_ntoa(sai->sin_addr) << '\n';
			break;
		}
		case AF_INET6:
		{
			std::array<char,INET6_ADDRSTRLEN> buffer = {0};
			auto sai = reinterpret_cast<sockaddr_in6*>(addr->ai_addr);
			os << inet_ntop(
					AF_INET6,
					&sai->sin6_addr,
					buffer.data(),
					buffer.size()
			) << '\n';

			break;
		}
		default:
		{
			os << "neither AF_INET nor AF_INET6\n";
		}
		// TODO print other variants
	}
}
