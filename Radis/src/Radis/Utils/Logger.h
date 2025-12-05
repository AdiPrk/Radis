#pragma once

// Windows-only logger (uses only C++ std lib + minimal Win32 from PCH).
#include <memory>
#include <string>
#include <string_view>
#include <format>
#include <mutex>
#include <chrono>
#include <iostream>
#include <atomic>
#include <fstream>
#include <optional>

namespace Radis
{
	class Logger
	{
	public:
		enum class Level : int { Trace = 0, Info = 1, Warn = 2, Error = 3, Critical = 4 };

		// Color scheme (ANSI sequences). Provide to Init() to override defaults.
		struct ColorScheme
		{
			std::string trace;
			std::string info;
			std::string warn;
			std::string error;
			std::string critical;
			std::string reset;

			// Additional, higher-level semantic slots for console composition:
			std::string dim;      // for timestamps / func:line / name (less prominent)
		};

		// Construct with name and trace-file path. Use Init(...) instead of constructing yourself.
		explicit Logger(std::string traceFile = "dog_trace.log", bool enableColors = true, std::optional<ColorScheme> scheme = std::nullopt) noexcept;

		// Initialize the global logger
		static void Init(const std::string& traceFile = "dog_trace.log", bool enableColors = true, std::optional<ColorScheme> scheme = std::nullopt);

		// Access the global logger.
		inline static std::shared_ptr<Logger>& GetLogger() { return sLogger; }

		// Set / get runtime level (thread-safe).
		void setLevel(Level lvl) noexcept { m_level.store(static_cast<int>(lvl), std::memory_order_relaxed); }
		Level level() const noexcept { return static_cast<Level>(m_level.load(std::memory_order_relaxed)); }

        // Formatted log functions
		template <typename... Args>
		void trace(const char* func, int line, std::format_string<Args...> fmt, Args&&... args)
		{
			log(Level::Trace, func, line, std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void info(const char* func, int line, std::format_string<Args...> fmt, Args&&... args)
		{
			log(Level::Info, func, line, std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void warn(const char* func, int line, std::format_string<Args...> fmt, Args&&... args)
		{
			log(Level::Warn, func, line, std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void error(const char* func, int line, std::format_string<Args...> fmt, Args&&... args)
		{
			log(Level::Error, func, line, std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void critical(const char* func, int line, std::format_string<Args...> fmt, Args&&... args)
		{
			log(Level::Critical, func, line, std::format(fmt, std::forward<Args>(args)...));
		}

		// Runtime-format helper (for dynamic format strings).
		template <typename... Args>
		void trace_runtime(const char* func, int line, std::string_view fmt, Args&&... args)
		{
			log(Level::Trace, func, line, std::vformat(std::string(fmt), std::make_format_args(args...)));
		}

	private:
		// Core logging function
		void log(Level lvl, const char* func, int line, std::string&& message);

		// Global instance
		static std::shared_ptr<Logger> sLogger;

		// State
		std::atomic<int>      m_level{ static_cast<int>(Level::Trace) };
		std::mutex            m_mutex;
		std::ofstream         m_trace_stream;

		// Deduplication (consecutive messages only)
		std::string           m_last_message;
		std::string           m_last_func;
		int                   m_last_line{ 0 };
		Level                 m_last_level{ Level::Trace };
		size_t                m_last_count{ 0 };

		// Reusable buffer
		std::string           m_line_cache;

		// Color / console
		bool                  m_stdout_is_tty{ false };
		bool                  m_stderr_is_tty{ false };
		bool                  m_ansi_supported{ false };
		bool                  m_colors_enabled{ true };
		ColorScheme           m_colors;
	};
}

// Macros: unchanged call-sites — macros inject function & line automatically.
#ifndef _SHIP
#define RADIS_ASSERT(x, ...) { if(!(x)) { RADIS_CRITICAL(__FUNCTION__, __LINE__, "Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define RADIS_STATIC_ASSERT(x, ...) static_assert(x, __VA_ARGS__)

#define RADIS_TRACE(...)    ::Radis::Logger::GetLogger()->trace(__FUNCTION__, __LINE__, __VA_ARGS__)
#define RADIS_INFO(...)     ::Radis::Logger::GetLogger()->info(__FUNCTION__, __LINE__, __VA_ARGS__)
#define RADIS_WARN(...)     ::Radis::Logger::GetLogger()->warn(__FUNCTION__, __LINE__, __VA_ARGS__)
#define RADIS_ERROR(...)    ::Radis::Logger::GetLogger()->error(__FUNCTION__, __LINE__, __VA_ARGS__)
#define RADIS_CRITICAL(...) ::Radis::Logger::GetLogger()->critical(__FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define RADIS_ASSERT(...)
#define RADIS_STATIC_ASSERT(...)
#define RADIS_TRACE(...)
#define RADIS_INFO(...)
#define RADIS_WARN(...)
// Keep ERROR and CRITICAL for release builds as well
#define RADIS_ERROR(...)    ::Radis::Logger::GetLogger()->error(__FUNCTION__, __LINE__, __VA_ARGS__)
#define RADIS_CRITICAL(...) ::Radis::Logger::GetLogger()->critical(__FUNCTION__, __LINE__, __VA_ARGS__)
#endif
