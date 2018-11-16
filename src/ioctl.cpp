/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config.hpp"
#include "dirinfo.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_close.hpp"
#include "fs_base_ioctl.hpp"
#include "fs_base_open.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

#include <fcntl.h>

using std::string;
using std::vector;
using namespace mergerfs;

namespace local
{
  static
  int
  ioctl(const int            fd,
        const unsigned long  cmd,
        void                *data)
  {
    int rv;

    rv = fs::ioctl(fd,cmd,data);

    return ((rv == -1) ? -errno : rv);
  }

  static
  int
  ioctl_file(fuse_file_info      *ffi,
             const unsigned long  cmd,
             void                *data)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

    return local::ioctl(fi->fd,cmd,data);
  }

#ifndef O_NOATIME
# define O_NOATIME 0
#endif

  static
  int
  ioctl_dir_base(Policy::Func::Search  searchFunc,
                 const Branches       &branches_,
                 const uint64_t        minfreespace,
                 const char           *fusepath,
                 const unsigned long   cmd,
                 void                 *data)
  {
    int fd;
    int rv;
    string fullpath;
    vector<const string*> basepaths;

    rv = searchFunc(branches_,fusepath,minfreespace,basepaths);
    if(rv == -1)
      return -errno;

    fs::path::make(basepaths[0],fusepath,fullpath);

    const int flags = O_RDONLY | O_NOATIME | O_NONBLOCK;
    fd = fs::open(fullpath,flags);
    if(fd == -1)
      return -errno;

    rv = local::ioctl(fd,cmd,data);

    fs::close(fd);

    return rv;
  }

  static
  int
  ioctl_dir(fuse_file_info      *ffi,
            const unsigned long  cmd,
            void                *data)
  {
    DirInfo                 *di     = reinterpret_cast<DirInfo*>(ffi->fh);
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return local::ioctl_dir_base(config.getattr,
                                 config.branches,
                                 config.minfreespace,
                                 di->fusepath.c_str(),
                                 cmd,
                                 data);
  }
}

namespace mergerfs
{
  namespace fuse
  {
    int
    ioctl(const char     *fusepath,
          int             cmd,
          void           *arg,
          fuse_file_info *ffi,
          unsigned int    flags,
          void           *data)
    {
      if(flags & FUSE_IOCTL_DIR)
        return local::ioctl_dir(ffi,cmd,data);

      return local::ioctl_file(ffi,cmd,data);
    }
  }
}
