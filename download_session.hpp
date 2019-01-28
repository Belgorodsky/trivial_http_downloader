#pragma once
#include <ostream> 
#include <vector>
#include "aliases.hpp"
#include "params_parser.hpp"
#include "progress_printer.hpp"

class download_session
{
	public:
		download_session(std::unique_ptr<params_parser>&& pz);

		void connect();	
		void send_http_request();
		void recv_http_response();
	private:
		void print_address(std::ostream& os, const shrtr::addri_uptr &addri);
		bool parse_header(std::string_view header);
		void recv_n_flush_rest(
			void (download_session::*flusher)(std::string_view)
		);
		void recv_content(std::string_view first_bytes);
		void recv_content_nommap(std::string_view first_bytes);
		void flush_some(std::string_view first_bytes);
		void flush_some_nommap(std::string_view first_bytes);
		bool init_file();
		bool init_file_nommap();
	private:
		int m_ignore_sigpipe;
		int m_er_status = 0;
		progress_printer m_progr_printer;
		size_t m_content_cur_pos = 0;
		size_t m_content_length;
		std::unique_ptr<params_parser> m_pz;
		shrtr::sock_uptr m_sock;  
		shrtr::file_uptr m_file;
		std::string m_response;
		shrtr::filemem_upr m_filemem;
};
