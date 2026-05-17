#pragma once
#include "platform.hpp"

#include <llarp/util/fs.hpp>

#include <stdlib.h>

#include <unistd.h>
#include <pwd.h>

namespace llarp
{
  const static inline fs::path our_rc_filename = "self.signed";
  const static inline fs::path our_identity_filename = "identity.key";
  const static inline fs::path our_enc_key_filename = "encryption.key";
  const static inline fs::path our_transport_key_filename = "transport.key";
  const static inline fs::path nodedb_dirname = "nodedb";

  inline fs::path
  GetDefaultDataDir()
  {
    fs::path datadir{"/var/lib/llarpd"};
    if (auto uid = geteuid())
    {
      if (auto* pw = getpwuid(uid))
      {
        datadir = fs::path{pw->pw_dir} / ".llarpd";
      }
    }
    return datadir;
  }

  inline fs::path
  GetDefaultConfigFilename()
  {
    return "llarpd.ini";
  }

  inline fs::path
  GetDefaultConfigPath()
  {
    return GetDefaultDataDir() / GetDefaultConfigFilename();
  }

  inline fs::path
  GetDefaultBootstrap()
  {
    return GetDefaultDataDir() / "bootstrap.signed";
  }

}  // namespace llarp
