#include "tftp/detail/argument_parser.hpp"
#include "tftp/tftp_server.hpp"

#include <spdlog/common-inl.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <format>
#include <iostream>
#include <thread>

using namespace net::service;
using namespace tftp;

using tftp_server = context_thread<server>;

static constexpr unsigned short PORT = 7;
static constexpr char const *const usage =
    "usage: {} [--log-level <LEVEL>] [<PORT>]\n";

static auto signal_mask() -> sigset_t *
{
  static auto set = sigset_t{};
  static sigset_t *setp = nullptr;
  static auto mtx = std::mutex{};

  if (auto lock = std::lock_guard{mtx}; !setp)
  {
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    setp = &set;
  }
  return setp;
}

static auto signal_handler(tftp_server &server) -> std::jthread
{
  static const sigset_t *sigmask = nullptr;
  static auto mtx = std::mutex();

  if (auto lock = std::lock_guard{mtx}; !sigmask)
  {
    sigmask = signal_mask();
    pthread_sigmask(SIG_BLOCK, sigmask, nullptr);

    return std::jthread([&](const std::stop_token &token) noexcept {
      static const auto timeout = timespec{.tv_sec = 0, .tv_nsec = 50000000};

      while (!token.stop_requested())
      {
        using enum tftp_server::signals;
        switch (sigtimedwait(sigmask, nullptr, &timeout))
        {
          case SIGTERM:
          case SIGHUP:
          case SIGINT:
            server.signal(terminate);
            break;

          default:
            break;
        }
      }
    });
  }

  return {};
}

struct config {
  unsigned short port = PORT;
};

static auto set_loglevel(std::string_view value) -> int
{
  auto level = std::string(value);
  std::ranges::transform(level, level.begin(),
                         // NOLINTNEXTLINE(readability-identifier-length)
                         [](unsigned char ch) { return std::tolower(ch); });

  auto spdlog_level = spdlog::level::from_str(level);
  if (spdlog_level != spdlog::level::off || level == "off")
  {
    spdlog::set_level(spdlog_level);
    return 0;
  }

  std::cerr << std::format("Unrecognized log level: {}\n", value)
            << "Valid log levels are: ";
  for (int i = 0; i < spdlog::level::n_levels; ++i)
  {
    if (i > 0)
      std::cerr << ", ";

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    const auto &level_str = spdlog::level::level_string_views[i];
    std::cerr << std::string(level_str.begin(), level_str.end());
  }
  std::cerr << "\n";
  return -1;
}

auto parse_args(int argc, char const *const *argv) -> std::optional<config>
{
  using namespace tftp::detail;

  auto conf = config();
  char const *const progname = std::filesystem::path(*argv).stem().c_str();

  auto error = [&]() -> std::optional<config> {
    std::cerr << std::format(usage, progname);
    return std::nullopt;
  };

  for (const auto &option : argument_parser::parse(argc, argv))
  {
    const auto &[flag, value] = option;
    if (!flag.empty()) // options with flags.
    {
      if (flag == "-h" || flag == "--help")
      {
        std::cout << std::format(usage, progname);
        return std::nullopt;
      }

      if (flag == "--log-level")
      {
        if (!set_loglevel(value))
          continue;

        return error();
      }

      std::cerr << std::format("Unknown flag: {}\n", flag);
      return error();
    }

    // positional options.
    auto [ptr, err] = std::from_chars(value.cbegin(), value.cend(), conf.port);
    if (err != std::errc{})
    {
      std::cerr << std::format("Invalid port number: {}\n", value);
      return error();
    }
  }

  return {conf};
}

auto main(int argc, char *argv[]) -> int
{
  using namespace io::socket;

  if (auto conf = parse_args(argc, argv))
  {
    auto mtx = std::mutex{};
    auto cvar = std::condition_variable{};

    auto address = socket_address<sockaddr_in6>{};
    address->sin6_family = AF_INET6;
    address->sin6_port = htons(conf->port);

    auto server = tftp_server();

    auto sighandler = signal_handler(server);

    spdlog::info("TFTP server starting on UDP port {}.", conf->port);
    server.start(mtx, cvar, address);

    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return server.stopped.load(); });

    spdlog::info("TFTP server stopped.");
  }
  return 0;
}
