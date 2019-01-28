#include "progress_printer.hpp"
#include <sys/ioctl.h>
#include <unistd.h>


progress_printer::progress_printer()
{
	winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	m_ws_col = w.ws_col - 3 -std::size(competed_mess);
}

void progress_printer::print(std::ostream &os, size_t pos, size_t len)
{
	os << "\r" << pos * 100 / len << competed_mess;
	os << std::string(pos * m_ws_col / len, '|');
	os.flush();
	if (pos == len) { os << '\n'; }
}
