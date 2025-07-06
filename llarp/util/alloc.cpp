#include "alloc.hpp"

// Override global operator new
void*
operator new(std::size_t size)
{
  void* ptr = ::malloc(size);
  if (!ptr)
  {
    throw std::bad_alloc();
  }
  return ptr;
}

// Override global operator new[]
void*
operator new[](std::size_t size)
{
  void* ptr = ::malloc(size);
  if (!ptr)
  {
    throw std::bad_alloc();
  }
  return ptr;
}

// Override global operator delete
void
operator delete(void* ptr) noexcept
{
  ::free(ptr);
}

// Override global operator delete[]
void
operator delete[](void* ptr) noexcept
{
  ::free(ptr);
}

// Override sized delete (C++14)
void
operator delete(void* ptr, std::size_t size) noexcept
{
  ::sdallocx(ptr, size, 0);
}

void
operator delete[](void* ptr, std::size_t size) noexcept
{
  ::sdallocx(ptr, size, 0);
}
