#pragma once


#include "crypto/ByteSlice.h"
#include "crypto/SHA.h"
#include "util/FileSystemException.h"
#include "util/Fs.h"
#include "util/Logging.h"
#include "xdrpp/marshal.h"

#include <fstream>
#include <string>
#include <vector>

namespace viichain
{

class XDRInputFileStream
{
    std::ifstream mIn;
    std::vector<char> mBuf;
    size_t mSizeLimit;
    size_t mSize;

  public:
    XDRInputFileStream(unsigned int sizeLimit = 0) : mSizeLimit{sizeLimit}
    {
    }

    void
    close()
    {
        mIn.close();
    }

    void
    open(std::string const& filename)
    {
        mIn.open(filename, std::ifstream::binary);
        if (!mIn)
        {
            std::string msg("failed to open XDR file: ");
            msg += filename;
            msg += ", reason: ";
            msg += std::to_string(errno);
            CLOG(ERROR, "Fs") << msg;
            throw FileSystemException(msg);
        }

        mSize = fs::size(mIn);
    }

    operator bool() const
    {
        return mIn.good();
    }

    size_t
    size() const
    {
        return mSize;
    }

    size_t
    pos()
    {
        assert(!mIn.fail());

        return mIn.tellg();
    }

    template <typename T>
    bool
    readOne(T& out)
    {
        char szBuf[4];
        if (!mIn.read(szBuf, 4))
        {
            return false;
        }

                        uint32_t sz = 0;
        sz |= static_cast<uint8_t>(szBuf[0] & '\x7f');
        sz <<= 8;
        sz |= static_cast<uint8_t>(szBuf[1]);
        sz <<= 8;
        sz |= static_cast<uint8_t>(szBuf[2]);
        sz <<= 8;
        sz |= static_cast<uint8_t>(szBuf[3]);

        if (mSizeLimit != 0 && sz > mSizeLimit)
        {
            return false;
        }
        if (sz > mBuf.size())
        {
            mBuf.resize(sz);
        }
        if (!mIn.read(mBuf.data(), sz))
        {
            throw xdr::xdr_runtime_error("malformed XDR file");
        }
        xdr::xdr_get g(mBuf.data(), mBuf.data() + sz);
        xdr::xdr_argpack_archive(g, out);
        return true;
    }
};

class XDROutputFileStream
{
    std::ofstream mOut;
    std::vector<char> mBuf;

  public:
    XDROutputFileStream()
    {
        mOut.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    }

    void
    close()
    {
        try
        {
            mOut.close();
        }
        catch (std::ios_base::failure&)
        {
            std::string msg("failed to close XDR file");
            msg += ", reason: ";
            msg += std::to_string(errno);
            throw FileSystemException(msg);
        }
    }

    void
    open(std::string const& filename)
    {
        try
        {
            mOut.open(filename, std::ofstream::binary | std::ofstream::trunc);
        }
        catch (std::ios_base::failure&)
        {
            std::string msg("failed to open XDR file: ");
            msg += filename;
            msg += ", reason: ";
            msg += std::to_string(errno);
            CLOG(FATAL, "Fs") << msg;
            throw FileSystemException(msg);
        }
    }

    operator bool() const
    {
        return mOut.good();
    }

    template <typename T>
    void
    writeOne(T const& t, SHA256* hasher = nullptr, size_t* bytesPut = nullptr)
    {
        uint32_t sz = (uint32_t)xdr::xdr_size(t);
        assert(sz < 0x80000000);

        if (mBuf.size() < sz + 4)
        {
            mBuf.resize(sz + 4);
        }

                        mBuf[0] = static_cast<char>((sz >> 24) & 0xFF) | '\x80';
        mBuf[1] = static_cast<char>((sz >> 16) & 0xFF);
        mBuf[2] = static_cast<char>((sz >> 8) & 0xFF);
        mBuf[3] = static_cast<char>(sz & 0xFF);

        xdr::xdr_put p(mBuf.data() + 4, mBuf.data() + 4 + sz);
        xdr_argpack_archive(p, t);

                                mOut.write(mBuf.data(), sz + 4);

        if (hasher)
        {
            hasher->add(ByteSlice(mBuf.data(), sz + 4));
        }
        if (bytesPut)
        {
            *bytesPut += (sz + 4);
        }
    }
};
}
