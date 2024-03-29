
#include "util/asio.h"
#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "main/Application.h"
#include "main/Config.h"
#include "main/VIICoreVersion.h"
#include "overlay/OverlayManager.h"
#include "overlay/TCPPeer.h"
#include "overlay/test/LoopbackPeer.h"
#include "test/test.h"
#include "util/Fs.h"
#include "util/Logging.h"
#include "util/Timer.h"
#include "util/XDRStream.h"

#include "test/fuzz.h"

#include <signal.h>
#include <xdrpp/autocheck.h>
#include <xdrpp/printer.h>

namespace viichain
{

struct CfgDirGuard
{
    Config const& mConfig;
    static void
    clean(std::string const& path)
    {
        if (fs::exists(path))
        {
            fs::deltree(path);
        }
    }
    CfgDirGuard(Config const& c) : mConfig(c)
    {
        clean(mConfig.BUCKET_DIR_PATH);
    }
    ~CfgDirGuard()
    {
        clean(mConfig.BUCKET_DIR_PATH);
    }
};

std::string
msgSummary(VIIMessage const& m)
{
    xdr::detail::Printer p(0);
    xdr::archive(p, m.type(), nullptr);
    return p.buf_.str() + ":" + hexAbbrev(sha256(xdr::xdr_to_msg(m)));
}

bool
tryRead(XDRInputFileStream& in, VIIMessage& m)
{
    try
    {
        return in.readOne(m);
    }
    catch (xdr::xdr_runtime_error& e)
    {
        LOG(INFO) << "Caught XDR error '" << e.what()
                  << "' on input substituting HELLO";
        m.type(HELLO);
        return true;
    }
}

#define PERSIST_MAX 1000
static unsigned int persist_cnt = 0;

void
fuzz(std::string const& filename, el::Level logLevel,
     std::vector<std::string> const& metrics)
{
    Logging::setFmt("<fuzz>", false);
    Logging::setLogLevel(logLevel, nullptr);
    LOG(INFO) << "Fuzzing vii-core " << VII_CORE_VERSION;
    LOG(INFO) << "Fuzz input is in " << filename;

    Config cfg1, cfg2;

    cfg1 = getTestConfig(0);
    cfg2 = getTestConfig(1);

    cfg1.HTTP_PORT = 0;
    cfg1.PUBLIC_HTTP_PORT = false;
    cfg1.ARTIFICIALLY_ACCELERATE_TIME_FOR_TESTING = true;
    cfg1.LOG_FILE_PATH = "fuzz-app-1.log";
    cfg1.BUCKET_DIR_PATH = "fuzz-buckets-1";
    cfg1.QUORUM_SET.threshold = 1;
    cfg1.QUORUM_SET.validators.clear();
    cfg1.QUORUM_SET.validators.push_back(
        SecretKey::fromSeed(sha256("a")).getPublicKey());

    cfg2.HTTP_PORT = 0;
    cfg2.PUBLIC_HTTP_PORT = false;
    cfg2.ARTIFICIALLY_ACCELERATE_TIME_FOR_TESTING = true;
    cfg2.LOG_FILE_PATH = "fuzz-app-2.log";
    cfg2.BUCKET_DIR_PATH = "fuzz-buckets-2";
    cfg2.QUORUM_SET.threshold = 1;
    cfg2.QUORUM_SET.validators.clear();
    cfg2.QUORUM_SET.validators.push_back(
        SecretKey::fromSeed(sha256("b")).getPublicKey());

    CfgDirGuard g1(cfg1);
    CfgDirGuard g2(cfg2);

restart:
{
    VirtualClock clock;
    Application::pointer app1 = Application::create(clock, cfg1);
    Application::pointer app2 = Application::create(clock, cfg2);
    LoopbackPeerConnection loop(*app1, *app2);
    while (!(loop.getInitiator()->isAuthenticated() &&
             loop.getAcceptor()->isAuthenticated()))
    {
        clock.crank(true);
    }

    XDRInputFileStream in(MAX_MESSAGE_SIZE);
    in.open(filename);
    VIIMessage msg;
    size_t i = 0;
    while (tryRead(in, msg))
    {
        ++i;
        LOG(INFO) << "Fuzzer injecting message " << i << ": "
                  << msgSummary(msg);
        auto peer = loop.getInitiator();
        clock.postToCurrentCrank(
            [peer, msg]() { peer->Peer::sendMessage(msg); });
    }
    while (loop.getAcceptor()->isConnected())
    {
        clock.crank(true);
    }
}

    if (getenv("AFL_PERSISTENT") && persist_cnt++ < PERSIST_MAX)
    {
#ifndef _WIN32
        raise(SIGSTOP);
#endif
        goto restart;
    }
}

void
genfuzz(std::string const& filename)
{
    Logging::setFmt("<fuzz>");
    size_t n = 3;
    LOG(INFO) << "Writing " << n << "-message random fuzz file " << filename;
    XDROutputFileStream out;
    out.open(filename);
    autocheck::generator<VIIMessage> gen;
    for (size_t i = 0; i < n; ++i)
    {
        try
        {
            VIIMessage m(gen(10));
            out.writeOne(m);
            LOG(INFO) << "Message " << i << ": " << msgSummary(m);
        }
        catch (xdr::xdr_bad_discriminant const&)
        {
            LOG(INFO) << "Message " << i << ": malformed, omitted";
        }
    }
}
}
