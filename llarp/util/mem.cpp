#include <llarp/util/alloc.h>
#include "mem.h"
#include <cstdlib>

namespace llarp
{
  void
  Zero(void* ptr, size_t sz)
  {
    auto* p = (uint8_t*)ptr;
    while (sz--)
    {
      *p = 0;
      ++p;
    }
  }
}  // namespace llarp

bool
llarp_eq(const void* a, const void* b, size_t sz)
{
  bool result = true;
  const auto* a_ptr = (const uint8_t*)a;
  const auto* b_ptr = (const uint8_t*)b;
  while (sz--)
  {
    result &= a_ptr[sz] == b_ptr[sz];
  }
  return result;
}
