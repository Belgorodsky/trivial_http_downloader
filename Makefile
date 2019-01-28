all: build_downloader

build_downloader:
	g++-8 -Ofast -std=c++17 progress_printer.cpp params_parser.cpp download_session.cpp downloader.cpp -o downloader
