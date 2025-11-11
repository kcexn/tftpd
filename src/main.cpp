#include "tftp/detail/argument_parser.hpp"
#include "tftp/tftp_server.hpp"

#include <spdlog/cfg/helpers.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <thread>

using namespace net::service;
using namespace tftp;

using tftp_server = context_thread<server>;

static constexpr unsigned short PORT = 69;
static constexpr char const *const usage =
    "usage: {} [-m <MAIL_PREFIX>] [-l <LEVEL>] [-p <PORT>]\n"
    "\n"
    "Options:\n"
    "-h, --help                         print this help.\n"
    "-m, --mail-prefix=<MAIL_PREFIX>    set the  mailprefix.\n"
    "-l, --log-level=<LEVEL>            set the log-level (critical, error, "
    "warn, info, debug)\n"
    "-p, --port=<PORT>                  set the port to listen on (default: "
    "69).\n";

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
  using std::tolower;
  auto level = std::string(value);
  std::ranges::transform(level, level.begin(),
                         [](unsigned char chr) { return tolower(chr); });

  auto spdlog_level = spdlog::level::from_str(level);
  if (spdlog_level != spdlog::level::off || level == "off")
  {
    spdlog::set_level(spdlog_level);
    return 0;
  }

  std::cerr << std::format("Unrecognized log level: {}\n", value)
            << "Valid log levels are: ";

  int count = 0;
  for (const auto &level_str : spdlog::level::level_string_views)
  {
    if (count++ > 0)
      std::cerr << ", ";

    std::cerr << std::string(level_str.begin(), level_str.end());
  }
  std::cerr << "\n";
  return -1;
}

// NOLINTNEXTLINE
auto parse_args(int argc, char const *const *argv) -> std::optional<config>
{
  using namespace tftp::detail;

  auto conf = config();
  auto progname = std::filesystem::path(*argv).stem();

  auto error = [&]() -> std::optional<config> {
    std::cerr << std::format(usage, progname.c_str());
    return std::nullopt;
  };

  for (const auto &option : argument_parser::parse(argc, argv))
  {
    const auto &[flag, value] = option;
    if (!flag.empty()) // options with flags.
    {
      if (flag == "-h" || flag == "--help")
      {
        std::cout << std::format(usage, progname.c_str());
        return std::nullopt;
      }

      if (flag == "-m" || flag == "--mail-prefix")
      {
        auto path = std::filesystem::path(value);
        if (setenv("TFTP_MAIL_PREFIX", path.c_str(), 1))
        {
          std::cerr << std::format(
              "Unable to set TFTP_MAIL_PREFIX, error: {}",
              std::error_code(errno, std::system_category()).message());
          return error();
        }
      }
      else if (flag == "-l" || flag == "--log-level")
      {
        if (!set_loglevel(value))
          continue;

        return error();
      }
      else if (flag == "-p" || flag == "--port")
      {
        auto [ptr, err] =
            std::from_chars(value.cbegin(), value.cend(), conf.port);
        if (err != std::errc{})
        {
          std::cerr << std::format("Invalid port number: {}\n", value);
          return error();
        }
      }
      else
      {
        std::cerr << std::format("Unknown flag: {}\n", flag);
        return error();
      }
    }
  }

  return {conf};
}

auto main(int argc, char *argv[]) -> int
{
  using namespace io::socket;

  if (auto conf = parse_args(argc, argv))
  {
    auto address = socket_address<sockaddr_in6>{};
    address->sin6_family = AF_INET6;
    address->sin6_port = htons(conf->port);

    auto server = tftp_server();

    auto sighandler = signal_handler(server);

    spdlog::info("TFTP server starting on UDP port {}.", conf->port);
    server.start(address);
    server.state.wait(server.PENDING);
    server.state.wait(server.STARTED);

    spdlog::info("TFTP server stopped.");
  }
  return 0;
}
