#pragma once

#include <bits/types/FILE.h>
#include <chrono>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>
#include <thread>

#include "mpmc_queue.hpp"
#include "log/record.hpp"
#include "log/loglevel.hpp"


namespace sheep {

namespace log {

template <std::size_t ThreadNum, std::size_t QCap>
class LoggerImpl
{
    using Global_Que_t = MPMCQueue<Record>;
    static constexpr std::size_t Desired_buffer_size = Record::Size * 8192;
    static constexpr std::size_t Desired_flush_threshold = Desired_buffer_size * 0.8;
    using Buffer = fmt::basic_memory_buffer<char, Desired_buffer_size>;
    static constexpr std::int64_t Desired_sleep_microsec = 10;

public:

    LogLevel getLogLevel() const { return m_logLevel; }

    LoggerImpl& getInstance() {
        static LoggerImpl instance;
        return instance;
    }

    LoggerImpl& setLogLevel(LogLevel level) { 
        m_logLevel = level;
        return *this;
    }

    LoggerImpl& setLogFile(FILE* pf) {
        closeLogFiles();
        m_outputs.clear();
        m_outputs.push_back(pf);        
        return *this;
    }

    LoggerImpl& setLogFile(const char* filename, bool trunc=false) {
        FILE* newfp = fopen(filename, trunc ? "w": "a");
        if (newfp == nullptr) {
            fmt::detail::throw_format_error(
                fmt::format("Unable to open file: {}, {}\n", filename, strerror(errno)).c_str()
            );
        }
        return setLogFile(newfp);
    }

    LoggerImpl& addLogFile(FILE* pf) {
        m_outputs.push_back(pf);
        return *this;
    }

    LoggerImpl& addLogFile(const char* filename, bool trunc=false) {
        FILE* newfp = fopen(filename, trunc ? "w": "a");
        if (newfp == nullptr) {
            fmt::detail::throw_format_error(
                fmt::format("Unable to open file: {}, {}\n", filename, strerror(errno)).c_str()
            );
        }
        return addLogFile(newfp);
    }

    void closeLogFiles() {
        if (m_buf.size() == 0 || m_outputs.empty()) return;
        poll(true);
        for (auto fp: m_outputs) {
            fclose(fp);
        } 
        m_outputs.clear();
    }

    bool checkLogLevel(LogLevel level) { return level>=m_logLevel; }

    template <typename Functor, typename... Args>
    void logToGlobalQueue(Functor& f, Args&&... args) {
        global_q.emplace_with(f, std::forward<Args>(args)...);
    }

    template<typename...Args>
    void log(LogLevel level, Args&&... args) {
        static auto emplace_functor = MakeRecord();
        if (!checkLogLevel(level)) {
            return;
        }

        logToGlobalQueue(emplace_functor, std::forward<Args>(args)...);
    }

    void writeFile() {
        if (m_buf.size() == 0) return;
        auto bufsz = m_buf.size();

        for (auto& fp: m_outputs) {
            fwrite(m_buf.data(), 1, m_buf.size(), fp);
            fflush(fp);
        }
        m_buf.clear();
    }

    void poll(bool flushFile) {
        static auto consume_functor = [this](Record* p) {
            //std::copy(std::begin(p->data), std::end(p->data), std::back_inserter(m_buf));
            //std::copy_if(std::begin(p->data), std::end(p->data), std::back_inserter(m_buf), [](char ch) { return ch != 0; });
            std::copy_n(std::begin(p->data), p->size, std::back_inserter(m_buf));
            //std::strcpy(m_buf, p->data);
        };

        auto cnt = global_q.try_consume_all(consume_functor);
        if (cnt == 0) return;

        if (m_buf.size() >= Desired_flush_threshold) writeFile();

        if (flushFile)
            writeFile();
    }

    void loop(std::int64_t interval=Desired_sleep_microsec) {
        startBackgroundThread(interval);
    }

    void startBackgroundThread(std::int64_t interval) {
        stopBackgroundThread();
        bgt_running = true;
        m_bgt = std::thread(
            [interval, this]() {
                while (bgt_running) {
                    if (interval > 0)
                        std::this_thread::sleep_for(std::chrono::nanoseconds(interval));
                    else
                        std::this_thread::yield();
                    poll(true);
                }
            }
        );
    }

    void stopBackgroundThread() {
        if (!bgt_running) return;
        bgt_running = false;
        if (m_bgt.joinable()) 
            m_bgt.join();
    }

    ~LoggerImpl() {
        stopBackgroundThread();
        poll(true);
        closeLogFiles();
    }

    LoggerImpl()
        : global_q(QCap)
    {
        m_outputs.push_back(stdout);
        // start bg task
        loop();
    }


private:
    LogLevel m_logLevel;
    std::vector<FILE*> m_outputs;

    Buffer m_buf;

    std::thread m_bgt;
    volatile bool bgt_running = false;

    Global_Que_t global_q;
};

} // namespace log

} // namespace sheep