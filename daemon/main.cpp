#include <llarp/config/config.hpp>  // for ensure_config
#include <llarp/constants/version.hpp>
#include <llarp.hpp>
#include <llarp/util/lokinet_init.h>
#include <llarp/util/exceptions.hpp>
#include <llarp/util/fs.hpp>
#include <llarp/util/str.hpp>

#include <llarp/util/service_manager.hpp>

#include <csignal>

#include <string>
#include <iostream>
#include <thread>
#include <future>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

namespace
{
  struct command_line_options
  {
    // bool options
    bool help = false;
    bool version = false;
    bool generate = false;
    bool router = false;
    bool config = false;
    bool configOnly = false;
    bool overwrite = false;

    // string options
    // TODO: change this to use a std::filesystem::path once we stop using ghc::filesystem on some
    // platforms
    std::string configPath;
  };

  // operational function definitions
  int
  lokinet_main(int, char**);
  void
  handle_signal(int sig);
  static void
  run_main_context(std::optional<fs::path> confFile, const llarp::RuntimeOptions opts);

  // variable declarations
  static auto logcat = llarp::log::Cat("main");
  std::shared_ptr<llarp::Context> ctx;
  std::promise<int> exit_code;

  // operational function definitions
  void
  handle_signal(int sig)
  {
    llarp::log::info(logcat, "Handling signal {}", sig);
    if (ctx)
      ctx->loop->call([sig] { ctx->HandleSignal(sig); });
    else
      std::cerr << "Received signal " << sig << ", but have no context yet. Ignoring!" << std::endl;
  }

  int
  llarp_main(int argc, char** argv)
  {
    if (auto result = Lokinet_INIT())
      return result;

    llarp::RuntimeOptions opts;
    opts.showBanner = false;

    CLI::App cli{
        "LLARP is a free, open source, private, decentralized "
        "IP "
        "based onion routing network",
        "llarpd"};
    command_line_options options{};

    // flags: boolean values in command_line_options struct
    cli.add_flag("--version", options.version, "llarpd version");
    cli.add_flag("-g,--generate", options.generate, "Generate default configuration and exit");
    cli.add_flag(
        "-r,--router", options.router, "Run llarpd in routing mode instead of client-only mode");
    cli.add_flag("-f,--force", options.overwrite, "Force writing config even if file exists");

    // options: string
    cli.add_option("config,--config", options.configPath, "Path to config.ini configuration file")
        ->capture_default_str();

    try
    {
      cli.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
      return cli.exit(e);
    }

    std::optional<fs::path> configFile;

    try
    {
      if (options.version)
      {
        std::cout << llarp::VERSION_FULL << std::endl;
        return 0;
      }

      opts.isSNode = options.router;

      if (options.generate)
      {
        options.configOnly = true;
      }

      if (not options.configPath.empty())
      {
        configFile = options.configPath;
      }
    }
    catch (const CLI::OptionNotFound& e)
    {
      cli.exit(e);
    }
    catch (const CLI::Error& e)
    {
      cli.exit(e);
    };

    if (configFile.has_value())
    {
      // when we have an explicit filepath
      fs::path basedir = configFile->parent_path();
      if (options.configOnly)
      {
        try
        {
          llarp::ensureConfig(basedir, *configFile, options.overwrite, opts.isSNode);
        }
        catch (std::exception& ex)
        {
          llarp::LogError("cannot generate config at ", *configFile, ": ", ex.what());
          return 1;
        }
      }
      else
      {
        try
        {
          if (!fs::exists(*configFile))
          {
            llarp::LogError("Config file not found ", *configFile);
            return 1;
          }
        }
        catch (std::exception& ex)
        {
          llarp::LogError("cannot check if ", *configFile, " exists: ", ex.what());
          return 1;
        }
      }
    }
    else
    {
      try
      {
        llarp::ensureConfig(
            llarp::GetDefaultDataDir(),
            llarp::GetDefaultConfigPath(),
            options.overwrite,
            opts.isSNode);
      }
      catch (std::exception& ex)
      {
        llarp::LogError("cannot ensure config: ", ex.what());
        return 1;
      }
      configFile = llarp::GetDefaultConfigPath();
    }

    if (options.configOnly)
      return 0;

    std::thread main_thread{[configFile, opts] { run_main_context(configFile, opts); }};
    auto ftr = exit_code.get_future();

    do
    {
      // do periodic non lokinet related tasks here
      if (ctx and ctx->IsUp() and not ctx->LooksAlive())
      {
        auto deadlock_cat = llarp::log::Cat("deadlock");
        for (const auto& wtf :
             {"you have been visited by the mascott of the deadlocked router.",
              "⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⣀⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠄⠄⠄⠄",
              "⠄⠄⠄⠄⠄⢀⣀⣀⡀⠄⠄⠄⡠⢲⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⠄⠄",
              "⠄⠄⠄⠔⣈⣀⠄⢔⡒⠳⡴⠊⠄⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⣿⣿⣧⠄⠄",
              "⠄⢜⡴⢑⠖⠊⢐⣤⠞⣩⡇⠄⠄⠄⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠄⠝⠛⠋⠐",
              "⢸⠏⣷⠈⠄⣱⠃⠄⢠⠃⠐⡀⠄⠄⠄⠄⠙⠻⢿⣿⣿⣿⣿⣿⣿⣿⡿⠛⠸⠄⠄⠄⠄",
              "⠈⣅⠞⢁⣿⢸⠘⡄⡆⠄⠄⠈⠢⡀⠄⠄⠄⠄⠄⠄⠉⠙⠛⠛⠛⠉⠉⡀⠄⠡⢀⠄⣀",
              "⠄⠙⡎⣹⢸⠄⠆⢘⠁⠄⠄⠄⢸⠈⠢⢄⡀⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠃⠄⠄⠄⠄⠄",
              "⠄⠄⠑⢿⠈⢆⠘⢼⠄⠄⠄⠄⠸⢐⢾⠄⡘⡏⠲⠆⠠⣤⢤⢤⡤⠄⣖⡇⠄⠄⠄⠄⠄",
              "⣴⣶⣿⣿⣣⣈⣢⣸⠄⠄⠄⠄⡾⣷⣾⣮⣤⡏⠁⠘⠊⢠⣷⣾⡛⡟⠈⠄⠄⠄⠄⠄⠄",
              "⣿⣿⣿⣿⣿⠉⠒⢽⠄⠄⠄⠄⡇⣿⣟⣿⡇⠄⠄⠄⠄⢸⣻⡿⡇⡇⠄⠄⠄⠄⠄⠄⠄",
              "⠻⣿⣿⣿⣿⣄⠰⢼⠄⠄⠄⡄⠁⢻⣍⣯⠃⠄⠄⠄⠄⠈⢿⣻⠃⠈⡆⡄⠄⠄⠄⠄⠄",
              "⠄⠙⠿⠿⠛⣿⣶⣤⡇⠄⠄⢣⠄⠄⠈⠄⢠⠂⠄⠁⠄⡀⠄⠄⣀⠔⢁⠃⠄⠄⠄⠄⠄",
              "⠄⠄⠄⠄⠄⣿⣿⣿⣿⣾⠢⣖⣶⣦⣤⣤⣬⣤⣤⣤⣴⣶⣶⡏⠠⢃⠌⠄⠄⠄⠄⠄⠄",
              "⠄⠄⠄⠄⠄⠿⠿⠟⠛⡹⠉⠛⠛⠿⠿⣿⣿⣿⣿⣿⡿⠂⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄⠄",
              "⠠⠤⠤⠄⠄⣀⠄⠄⠄⠑⠠⣤⣀⣀⣀⡘⣿⠿⠙⠻⡍⢀⡈⠂⠄⠄⠄⠄⠄⠄⠄⠄⠄",
              "⠄⠄⠄⠄⠄⠄⠑⠠⣠⣴⣾⣿⣿⣿⣿⣿⣿⣇⠉⠄⠻⣿⣷⣄⡀⠄⠄⠄⠄⠄⠄⠄⠄",
              "file a bug report now or be cursed with this "
              "annoying image in your syslog for all time."})
        {
          llarp::log::critical(deadlock_cat, wtf);
          llarp::log::flush();
        }
        llarp::sys::service_manager->failed();
        std::abort();
      }
    } while (ftr.wait_for(std::chrono::seconds(1)) != std::future_status::ready);

    main_thread.join();

    int code = 0;

    try
    {
      code = ftr.get();
    }
    catch (const std::exception& e)
    {
      std::cerr << "main thread threw exception: " << e.what() << std::endl;
      code = 1;
    }
    catch (...)
    {
      std::cerr << "main thread threw non-standard exception" << std::endl;
      code = 2;
    }

    llarp::log::flush();
    llarp::sys::service_manager->stopped();
    if (ctx)
    {
      ctx.reset();
    }
    return code;
  }

  // this sets up, configures and runs the main context
  static void
  run_main_context(std::optional<fs::path> confFile, const llarp::RuntimeOptions opts)
  {
    llarp::LogInfo(fmt::format("starting up {} {}", llarp::VERSION_FULL, llarp::RELEASE_MOTTO));
    try
    {
      std::shared_ptr<llarp::Config> conf;
      if (confFile)
      {
        llarp::LogInfo("Using config file: ", *confFile);
        conf = std::make_shared<llarp::Config>(confFile->parent_path());
      }
      else
      {
        conf = std::make_shared<llarp::Config>(llarp::GetDefaultDataDir());
      }
      if (not conf->Load(confFile, opts.isSNode))
      {
        llarp::LogError("failed to parse configuration");
        exit_code.set_value(1);
        return;
      }

      // change cwd to dataDir to support relative paths in config
      fs::current_path(conf->router.m_dataDir);

      ctx = std::make_shared<llarp::Context>();
      ctx->Configure(std::move(conf));

      signal(SIGINT, handle_signal);
      signal(SIGTERM, handle_signal);

      signal(SIGHUP, handle_signal);
      signal(SIGUSR1, handle_signal);

      try
      {
        ctx->Setup(opts);
      }
      catch (llarp::util::bind_socket_error& ex)
      {
        llarp::LogError(fmt::format("{}, is llarpd already running? 🤔", ex.what()));
        exit_code.set_value(1);
        return;
      }
      catch (std::exception& ex)
      {
        llarp::LogError(fmt::format("failed to start up llarpd: {}", ex.what()));
        exit_code.set_value(1);
        return;
      }
      llarp::util::SetThreadName("llarpd-mainloop");

      auto result = ctx->Run(opts);
      exit_code.set_value(result);
    }
    catch (std::exception& e)
    {
      llarp::LogError("Fatal: caught exception while running: ", e.what());
      exit_code.set_exception(std::current_exception());
    }
    catch (...)
    {
      llarp::LogError("Fatal: caught non-standard exception while running");
      exit_code.set_exception(std::current_exception());
    }
  }

}  // namespace

int
main(int argc, char* argv[])
{
  // Set up a default, stderr logging for very early logging; we'll replace this later once we read
  // the desired log info from config.
  llarp::log::add_sink(llarp::log::Type::Print, "stderr");
  llarp::log::reset_level(llarp::log::Level::info);

  llarp::logRingBuffer = std::make_shared<llarp::log::RingBufferSink>(100);
  llarp::log::add_sink(llarp::logRingBuffer, llarp::log::DEFAULT_PATTERN_MONO);

  return llarp_main(argc, argv);
}
