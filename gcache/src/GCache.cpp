/*
 * Copyright (C) 2009 Codership Oy <info@codership.com>
 */

#include <cerrno>
#include <unistd.h>
#include <galerautils.hpp>

#include "BufferHeader.hpp"
#include "GCache.hpp"

namespace gcache
{
    const size_t  GCache::PREAMBLE_LEN = 1024; // reserved for text preamble

    static size_t check_size (size_t megs)
    {
        // overflow check (2^20 == 1Mb)
        if (megs != ((megs << 20) >> 20)) {
            std::ostringstream msg;
            msg << "Requested cache size too high: " << megs << "Mb";
            throw gu::Exception (msg.str().c_str(), ERANGE);
        }
        return (megs << 20);
    }

    void
    GCache::reset_cache()
    {
        first = start;
        next  = start;

        BH_clear (BH (next));

        size_free = size_cache;
        size_used = 0;

        seqno_min = SEQNO_NONE;
        seqno_max = SEQNO_NONE;
    }

    void
    GCache::constructor_common()
    {
        header_write ();
        preamble_write ();
    }

    GCache::GCache (std::string& fname, size_t megs)
        : mtx       (),
          cond      (),
          fd        (fname, check_size(megs)),
          mmap      (fd),
          open      (true),
          preamble  (static_cast<char*>(mmap.ptr)),
          header    (reinterpret_cast<int64_t*>(preamble + PREAMBLE_LEN)),
          header_len(32),
          start     (reinterpret_cast<uint8_t*>(header + header_len)),
          end       (reinterpret_cast<uint8_t*>(preamble + mmap.size)),
          first     (start),
          next      (first),
          size_cache(end - start),
          size_free (size_cache),
          size_used (0),
          mallocs   (0),
          reallocs  (0),
          seqno_locked(SEQNO_NONE),
          seqno_min   (SEQNO_NONE),
          seqno_max   (SEQNO_NONE),
          version   (0),
          seqno2ptr ()
    {
        BH_clear (BH (next));
        constructor_common ();
    }

    GCache::GCache (std::string& fname)
        : mtx       (),
          cond      (),
          fd        (fname),
          mmap      (fd),
          open      (true),
          preamble  (static_cast<char*>(mmap.ptr)),
          header    (reinterpret_cast<int64_t*>(preamble + PREAMBLE_LEN)),
          header_len(gu::convert(header[0], header_len)),
          start     (reinterpret_cast<uint8_t*>(header + header_len)),
          end       (reinterpret_cast<uint8_t*>(preamble + mmap.size)),
          first     (0),
          next      (first),
          size_cache(-1),
          size_free (size_cache),
          size_used (0),
          mallocs   (0),
          reallocs  (0),
          seqno_locked(SEQNO_NONE),
          seqno_min   (SEQNO_NONE),
          seqno_max   (SEQNO_NONE),
          version   (0),
          seqno2ptr ()
    {
        header_read ();
        constructor_common ();
    }

    GCache::~GCache ()
    {
        gu::Lock lock(mtx);

        mmap.sync();

        open = false;
        header_write ();
        preamble_write ();

        mmap.sync();
        mmap.unmap();
    }

    /*! prints object properties */
    void print (std::ostream& os)
    {
    }
}