#include <PCH/pch.h>      // assumed to include <windows.h>
#include "Logger.h"

#include <io.h>           // _isatty, _fileno
#include <cstdio>         // stdout/stderr
#include <utility>

using namespace Dog;

// Default ANSI color scheme
static Logger::ColorScheme default_colors()
{
	// ANSI escape sequences. Whole-line color per level.
	return {
		"\x1b[36m",    // trace: bright black / dim
		"\x1b[32m",    // info: green
		"\x1b[33m",    // warn: yellow
		"\x1b[31m",    // error: red
		"\x1b[41;97m", // critical: red bg, white bright
		"\x1b[0m",     // reset
		"\x1b[90m"     // for timestamp/name/func:line (subtle)
	};
}

std::shared_ptr<Logger> Logger::sLogger = nullptr;

Logger::Logger(std::string name, std::string traceFile, bool enableColors, std::optional<ColorScheme> scheme) noexcept
	: m_name(std::move(name)), m_colors(scheme.value_or(default_colors())), m_colors_enabled(enableColors)
{
	// Reserve caches to reduce allocations
	m_line_cache.reserve(256);
	m_last_message.reserve(128);
	m_last_func.reserve(128);

	// Detect console TTY state
	m_stdout_is_tty = (_isatty(_fileno(stdout)) != 0);
	m_stderr_is_tty = (_isatty(_fileno(stderr)) != 0);

	// Try to enable VT processing on Windows consoles (best-effort)
	if (m_stdout_is_tty || m_stderr_is_tty)
	{
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut != INVALID_HANDLE_VALUE)
		{
			DWORD mode = 0;
			if (GetConsoleMode(hOut, &mode))
			{
				mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				if (SetConsoleMode(hOut, mode))
				{
					m_ansi_supported = true;
				}
			}
		}
	}

	// Conservative fallback
	if (!m_ansi_supported)
		m_ansi_supported = (m_stdout_is_tty || m_stderr_is_tty);

	// Open trace file in append mode (best-effort)
	try
	{
		m_trace_stream.open(traceFile, std::ios::out | std::ios::app);
	}
	catch (...)
	{
		// ignore, console logging still works
	}

	// If colors were disabled globally, mark them off
	if (!enableColors)
		m_colors_enabled = false;
}

void Logger::Init(const std::string& name, const std::string& traceFile, bool enableColors, std::optional<ColorScheme> scheme)
{
	if (!sLogger)
	{
		sLogger = std::make_shared<Logger>(name, traceFile, enableColors, scheme);
	}
	else
	{
        sLogger->error(nullptr, 0, "Logger::Init() called multiple times; ignoring.");
	}
}

void Logger::log(Level lvl, const char* func, int line, std::string&& message)
{
    // Level filter (fast path)
    if (static_cast<int>(lvl) < m_level.load(std::memory_order_relaxed))
        return;

    // Build time string (HH:MM:SS.mmm) — no date.
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_s = floor<seconds>(now);
    std::string timestr = std::format("{:%T}", now_s);
    auto ms = duration_cast<milliseconds>(now - now_s).count();
    timestr = std::format("{}.{:03}", timestr, static_cast<int>(ms));

    // Level string for trace file
    const char* level_str = (lvl == Level::Trace) ? "TRACE"
        : (lvl == Level::Info) ? "INFO"
        : (lvl == Level::Warn) ? "WARN"
        : (lvl == Level::Error) ? "ERROR"
        : "CRITICAL";

    // Trace-file line (colorless, includes level and logger name)
    // Format: [HH:MM:SS.mmm] NAME LEVEL func:line: message
    std::string file_line = std::format("[{}] {} {} {}:{}: {}", timestr, m_name, level_str, func ? func : "", line, message);

    std::lock_guard lock(m_mutex);

    // Always append to trace file if possible
    if (m_trace_stream)
    {
        m_trace_stream << file_line << '\n';
        m_trace_stream.flush();
    }

    // Console output helpers
    std::ostream& out = (lvl >= Level::Error) ? std::cerr : std::cout;
    bool out_is_tty = (lvl >= Level::Error) ? m_stderr_is_tty : m_stdout_is_tty;

    const std::string& msg_color = (lvl == Level::Trace) ? m_colors.trace
        : (lvl == Level::Info) ? m_colors.info
        : (lvl == Level::Warn) ? m_colors.warn
        : (lvl == Level::Error) ? m_colors.error
        : m_colors.critical;

    // Console dedupe: require same level + message + func + line
    if (!m_last_message.empty() && lvl == m_last_level && message == m_last_message &&
        (func ? func : "") == m_last_func && line == m_last_line)
    {
        ++m_last_count;

        // When updating the repeated count, print using the stored m_last_message (not the possibly-moved incoming).
        // Build a visible console prefix (dimmed), then colored message, then dimmed "(xN)".
        std::string console_prefix = std::format("[{}] {}:{}: ", timestr, func ? func : "", line);

        if (m_colors_enabled && m_ansi_supported && out_is_tty)
        {
            // Move cursor up, rewrite the line, then clear to end-of-line (restores previous behavior).
            out << "\x1b[1A\r"
                << m_colors.dim << console_prefix << m_colors.reset
                << msg_color << m_last_message << m_colors.reset
                << m_colors.dim << std::format(" (x{})", m_last_count) << m_colors.reset
                << "\x1b[0K" << '\n';
            out.flush();
        }
        else
        {
            out << std::format("[{}] {}:{}: (repeated {} times) {}", timestr, func ? func : "", line, m_last_count, m_last_message) << '\n';
            out.flush();
        }
        return;
    }

    // New message: update dedupe state (move the incoming message into stored slot).
    m_last_message = std::move(message);
    m_last_func = (func ? func : "");
    m_last_line = line;
    m_last_level = lvl;
    m_last_count = 1;

    // Build console prefix and print: dimmed prefix + colored message
    std::string console_prefix = std::format("[{}] {}:{}: ", timestr, func ? func : "", line);

    if (m_colors_enabled && m_ansi_supported && out_is_tty)
    {
        out << m_colors.dim << console_prefix << m_colors.reset
            << msg_color << m_last_message << m_colors.reset
            << '\n';
        out.flush();
    }
    else
    {
        out << console_prefix << m_last_message << '\n';
        out.flush();
    }
}
