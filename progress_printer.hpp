#pragma once
#include <ostream>

class progress_printer
{
	public:
		progress_printer();
		void print(std::ostream &os, size_t pos, size_t len);
	private:
		constexpr char static competed_mess[] = "% completed: ";
		int m_ws_col;
};
