#pragma once
#include <memory>
#include <netdb.h>
#include <functional>

namespace shrtr
{
	using addri_uptr = std::unique_ptr<addrinfo,void (*)(addrinfo*)>;
	using sock_uptr = std::unique_ptr<int, void (*)(int *s)>;
	using file_uptr = std::unique_ptr<int, void (*)(int *f)>;
	using filemem_upr = std::unique_ptr<void, std::function<void (void *)>>;
} // namespace shrtr
