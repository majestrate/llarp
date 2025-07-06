#pragma once
#ifdef ENABLE_JEMALLOC
#include <jemalloc/jemalloc.h>
#include <new>

// Override global operator new
void*
operator new(std::size_t size);

// Override global operator new[]
void*
operator new[](std::size_t size);

// Override global operator delete
void
operator delete(void* ptr) noexcept;

// Override global operator delete[]
void
operator delete[](void* ptr) noexcept;

// Override sized delete (C++14)
void
operator delete(void* ptr, std::size_t size) noexcept;

void
operator delete[](void* ptr, std::size_t size) noexcept;

#endif
