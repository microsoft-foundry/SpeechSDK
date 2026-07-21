//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <string>
#include "log_utils.h"
#include "speechapi_c_diagnostics.h"

#pragma once

template<class TicketType = size_t>
class MultiStepTicketQueue
{
public:

    MultiStepTicketQueue()
    {
        std::memset(m_ticketStep, startStep, maxTickets * sizeof(m_ticketStep[0]));
    }

    TicketType CreateTicket()
    {
        auto ticket = m_nextTicket.fetch_add(1);
        auto slot = ticket % maxTickets;

        SPX_DBG_ASSERT(m_ticketStep[slot] == startStep);
        m_ticketStep[slot] = startStep + 1;

        return ticket;
    }

    template<size_t size>
    std::array<TicketType, size> CreateTickets()
    {
        auto ticket = m_nextTicket.fetch_add(size);

        std::array<TicketType, size> tickets = {};
        for (size_t i = 0; i < size; i++)
        {
            auto slot = (ticket + i) % maxTickets;

            SPX_DBG_ASSERT(m_ticketStep[slot] == startStep);
            m_ticketStep[slot] = startStep + 1;

            tickets[i] = ticket + i;
        }

        return tickets;
    }

    uint8_t AdvanceStep(TicketType ticket)
    {
        auto slot = ticket % maxTickets;
        return AdvanceStepInternal(ticket, m_ticketStep[slot] + 1);
    }

    uint8_t AdvanceToStep(TicketType ticket, uint8_t step)
    {
        return AdvanceStepInternal(ticket, step);
    }

    void DisposeTicket(TicketType ticket)
    {
        AdvanceStepInternal(ticket, disposedStep);

        auto prevSlot = ticket == 0
            ? maxTickets - 1
            : (ticket - 1) % maxTickets;

        SPX_DBG_ASSERT(m_ticketStep[prevSlot] == disposedStep || ticket == 0);
        m_ticketStep[prevSlot] = startStep;
    }

    class DisposeGuard
    {
    public:

        DisposeGuard(MultiStepTicketQueue& queue, TicketType ticket) : m_tickets(&queue), m_ticket(ticket) {}
        DisposeGuard(DisposeGuard&& other) : m_tickets(other.m_tickets), m_ticket(other.m_ticket) { other.m_tickets = nullptr; }

        ~DisposeGuard() { DisposeTicket(); }

        TicketType Ticket() { return m_ticket; };
        operator TicketType() const { return m_ticket; }

        uint8_t AdvanceStep() { return m_tickets->AdvanceStep(m_ticket); }
        uint8_t AdvanceToStep(uint8_t step) { return m_tickets->AdvanceToStep(m_ticket, step); }

        void DisposeTicket()
        {
            if (m_tickets != nullptr)
            {
                m_tickets->DisposeTicket(m_ticket);
                m_tickets = nullptr;
            }
        }

    private:

        DisposeGuard(const DisposeGuard&) = delete;
        DisposeGuard& operator=(const DisposeGuard&) = delete;
        DisposeGuard& operator=(DisposeGuard&&) = delete;

    private:

        MultiStepTicketQueue* m_tickets;
        TicketType m_ticket;
    };

    DisposeGuard CreateTicketGuard()
    {
        return DisposeGuard(*this, CreateTicket());
    }

    std::array<DisposeGuard, 2> LockStepsInclusive(uint8_t firstStep, uint8_t lastStep)
    {
        // Get two back to back tickets
        auto tickets = CreateTickets<2>();
        SPX_DBG_ASSERT(tickets[0] + 1 == tickets[1]);

        // advance to the appropriate steps
        AdvanceStepInternal(tickets[0], lastStep);
        AdvanceStepInternal(tickets[1], firstStep);

        // return guards, in std::array, reverse order so they're destroyed in the correct order
        return std::array<DisposeGuard, 2>{DisposeGuard(*this, tickets[1]), DisposeGuard(*this, tickets[0])};
    }

    std::array<DisposeGuard, 2> LockAllSteps()
    {
        return LockStepsInclusive(firstLockStep, lastLockStep);
    }

protected:

    uint8_t AdvanceStepInternal(TicketType ticket, uint8_t newStage)
    {
        auto slot = ticket % maxTickets;
        auto prevSlot = ticket == 0
            ? maxTickets - 1
            : (ticket - 1) % maxTickets;

        auto unblocked = (ticket == 0)
            || (m_ticketStep[prevSlot] > newStage)
            || (newStage == disposedStep && m_ticketStep[prevSlot] == disposedStep);

        auto looped = 0;
        for (;;)
        {
            unblocked = (m_ticketStep[prevSlot] > newStage)
                || (newStage == disposedStep && m_ticketStep[prevSlot] == disposedStep)
                || (ticket == 0);

            if (unblocked) break;

            constexpr auto maxSpinCount = 100;
            if (looped++ == maxSpinCount)
            {
                std::this_thread::yield();
                looped = 0;
            }
        }

        m_ticketStep[slot] = newStage;

        SPX_DBG_ASSERT(ticket == 0
            || m_ticketStep[prevSlot] > m_ticketStep[slot]
            || m_ticketStep[prevSlot] == disposedStep);

        return newStage;
    }

private:

    static constexpr uint8_t startStep = 0;
    static constexpr uint8_t disposedStep = UINT8_MAX;

    static constexpr uint8_t firstLockStep = 2;
    static constexpr uint8_t lastLockStep = disposedStep - 1;

    static constexpr size_t maxTickets = 1024;
    uint8_t m_ticketStep[maxTickets];

    std::atomic<TicketType> m_nextTicket{ 0 };
};

class MemoryLogger
{
public:

    static MemoryLogger& Instance();

    MemoryLogger() = default;
    ~MemoryLogger() { Exit(); }
    MemoryLogger(MemoryLogger const&) = delete;
    void operator=(MemoryLogger const&) = delete;

    void Dump(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr);
    void DumpOnExit(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr);
    void Exit();

    void SetFilters(const char* filters);
    void EnableLogging(bool enable);
    bool IsLoggingEnabled();

    void LogToMemory(const char *line);

    size_t GetLineNumOldest() const { return m_numLines >= m_maxLines ? m_numLines - m_maxLines : 0; }
    size_t GetLineNumNewest() const { return m_numLines; }

    const char* GetLine(size_t lineNum) { return m_traceLinePtrs[lineNum % m_maxLines]; }

private:
    LogFilter m_filter;

    using TicketType = size_t;
    size_t m_started = 0;

    static constexpr size_t m_maxLines = 10000;
    static constexpr size_t avgCchPerLine = 200;
    static constexpr size_t bufferCch = m_maxLines * avgCchPerLine;
    char m_buffer[bufferCch + 1] = { };

    char* m_bufferStart = m_buffer;
    char* m_bufferEnd = m_buffer + bufferCch;
    char* m_bufferPtr = m_buffer;

    char* m_traceLinePtrs[m_maxLines] = { nullptr };
    size_t m_numLines = 0;

    MultiStepTicketQueue<TicketType> m_tickets;
    static constexpr TicketType LogTicketStep1_UpdateBuffer = 2;
    static constexpr TicketType LogTicketStep2_UpdateLines = 3;
    static constexpr TicketType LogDumpStep1_FileWrite = 4;

    struct _DumpOnExit
    {
        bool m_enabled;
        std::string m_fileName;
        std::string m_linePrefix;
        bool m_emitToStdOut;
        bool m_emitToStdErr;
    } m_dumpOnExit;
};
