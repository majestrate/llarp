#pragma once
#include "fs.hpp"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <dirent.h>

namespace llarp::util
{
  /// Reads a binary file from disk into a string.  Throws on error.
  std::string
  slurp_file(const fs::path& filename);

  /// Reads a binary file from disk directly into a buffer.  Throws a std::length_error if the
  /// file is bigger than the buffer.  Returns the bytes copied on success.
  size_t
  slurp_file(const fs::path& filename, char* buffer, size_t buffer_size);

  /// Same, but for some non-char but single-byte char type (e.g. byte_t, std::byte, unsigned char).
  template <
      typename Char,
      std::enable_if_t<sizeof(Char) == 1 and not std::is_same_v<Char, char>, int> = 1>
  inline size_t
  slurp_file(const fs::path& filename, Char* buffer, size_t buffer_size)
  {
    return slurp_file(filename, reinterpret_cast<char*>(buffer), buffer_size);
  }

  /// Dumps binary string contents to disk. The file is overwritten if it already exists.  Throws
  /// on error.
  void
  dump_file(const fs::path& filename, std::string_view contents);

  /// Same as above, but works via char-like buffer
  template <typename Char, std::enable_if_t<sizeof(Char) == 1, int> = 0>
  inline void
  dump_file(const fs::path& filename, const Char* buffer, size_t buffer_size)
  {
    return dump_file(
        filename, std::string_view{reinterpret_cast<const char*>(buffer), buffer_size});
  }

  std::string
  filepath(const fs::path& f);

  struct FileHash
  {
    size_t
    operator()(const fs::path& f) const
    {
      std::hash<std::string> h;
      return h(filepath(f));
    }
  };

  using error_code_t = std::error_code;

  /// Ensure that a file exists and has correct permissions
  /// return any error code or success
  error_code_t
  EnsurePrivateFile(const fs::path& pathname);

  /// open a stream to a file and ensure it exists before open
  /// sets any permissions on creation
  template <typename T>
  std::optional<T>
  OpenFileStream(const fs::path& pathname, std::ios::openmode mode)
  {
    if (EnsurePrivateFile(pathname))
      return {};
    return std::make_optional<T>(pathname, mode);
  }

  template <typename PathVisitor>
  static void
  IterDir(const fs::path& _path, PathVisitor visit)
  {
    auto fpath = filepath(_path);
    DIR* d = opendir(fpath.c_str());
    if (d == nullptr)
      return;
    struct dirent* ent = nullptr;
    std::set<fs::path> entries;
    fs::path path{_path};
    do
    {
      ent = readdir(d);
      if (not ent)
        break;
      if (ent->d_name[0] == '.')
        continue;
      entries.emplace(path / fs::path{ent->d_name});
    } while (ent);
    closedir(d);

    for (const auto& p : entries)
    {
      if (not visit(p))
        return;
    }
  }

}  // namespace llarp::util
