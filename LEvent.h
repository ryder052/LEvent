// ###############################################################################################################
// Copyright 2022, Jakub Mrowinski, All rights reserved.
// ###############################################################################################################
#pragma once
#include <vector>
#include <any>
#include <ranges>
#include "Singleton.h"

namespace levent
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Interface for cleaner Connection
    class ILEvent
    {
    public:
        virtual void RemoveListener(class Connection&) = 0;
    };

    enum class EError
    {
        OK,
        EventAlreadyDefined,
        FailedToMatchEventType,
        ModifyingCallbackListDuringBroadcast,
        CallbackAlreadyAdded,
        EventsBlocked
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Use this to unbind from an event.
    class [[nodiscard]] Connection
    {
    public:
        Connection(EError InError)
            : Error(InError)
        {}

        virtual ~Connection() = default;

        template<typename T>
        Connection(std::shared_ptr<ILEvent> InEvent, std::shared_ptr<T> InDelegate)
            : Event(InEvent)
            , Delegate(InDelegate)
        {}

        // Copying connections is forbidden
        Connection(const Connection&) = delete;

        // Moving is fine
        Connection(Connection&& Other) noexcept
            : Event(std::move(Other.Event))
            , Delegate(std::move(Other.Delegate))
            , bActive(Other.bActive)
            , Error(Other.Error)
        {}

        // Move assignment is also fine
        Connection& operator=(Connection&& Other) noexcept
        {
            Event = std::move(Other.Event);
            Delegate = std::move(Other.Delegate);
            bActive = Other.bActive;
            Error = Other.Error;
            return *this;
        }

        // Disable the connection.
        void Disconnect()
        {
            if (Event && bActive)
                Event->RemoveListener(*this);

            bActive = false;
        }

        // Easy checking
        explicit operator bool() const
        {
            return static_cast<bool>(Event) && bActive;
        }

        // State checking
        const auto IsActive() const { return bActive && Error == EError::OK; }
        const auto GetError() const { return Error; }

    protected:
        std::shared_ptr<ILEvent> Event;
        std::any Delegate;
        bool bActive = true;
        EError Error = EError::OK;

        template<template<typename, typename...> typename DelegateFactory, typename ReturnType, typename... Args>
        friend class LEvent;
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // RAII version for convenience.
    class [[nodiscard]] ScopedConnection : public Connection
    {
    public:
        ScopedConnection() = default;

        ScopedConnection(Connection&& conn)
            : Connection(std::move(conn))
        {}

        ScopedConnection(ScopedConnection&& conn) noexcept
            : Connection(std::move(conn))
        {}

        ScopedConnection& operator=(ScopedConnection&& other) noexcept
        {
            Event = std::move(other.Event);
            Delegate = other.Delegate;
            return *this;
        }

        ~ScopedConnection()
        {
            Connection::Disconnect();
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Broadcasts data to bound delegates.
    template<template<typename, typename...> typename DelegateFactory, typename ReturnType, typename... Args>
    class LEvent : public ILEvent
    {
        using DelegateType = typename DelegateFactory<ReturnType, Args...>::Delegate;

    public:
        LEvent() = default;

        // Registers new delegate: Object + member function ptr
        template<typename ObjectType, typename MemberFuncPtrType>
        std::shared_ptr<DelegateType> AddListener(ObjectType* Object, MemberFuncPtrType FuncPtr, int Priority = 0, bool AllowDuplicates = false)
        {
            if (bIsBroadcasting)
                return nullptr;

            auto DelegatePtr = DelegateFactory<ReturnType, Args...>::MakeDelegate(Object, FuncPtr, Priority);

            bool Success = InsertDelegate(DelegatePtr, AllowDuplicates);
            return Success ? DelegatePtr : nullptr;
        }

        // Registers new delegate: Callable or free function ptr
        template<typename FuncType>
        std::shared_ptr<DelegateType> AddListener(FuncType FuncPtr, int Priority = 0, bool AllowDuplicates = false)
        {
            if (bIsBroadcasting)
                return nullptr;

            auto DelegatePtr = DelegateFactory<ReturnType, Args...>::MakeDelegate(FuncPtr, Priority);

            bool Success = InsertDelegate(DelegatePtr, AllowDuplicates);
            return Success ? DelegatePtr : nullptr;
        }

        // Unbinds a delegate managed by a Connection.
        virtual void RemoveListener(Connection& Conn) override
        {
            if (bIsBroadcasting)
                return;

            auto&& DelegatePtr = std::any_cast<std::shared_ptr<DelegateType>>(Conn.Delegate);
            auto FoundIt = std::ranges::find_if(Delegates, [&DelegatePtr](const auto& Data)
                {
                    return Data == DelegatePtr;
                });

            if (FoundIt != Delegates.end())
                Delegates.erase(FoundIt);
        }

        // Calls all delegates.
        // While broadcasting, disable adding/removing listeners.
        auto Trigger(Args... InArgs) const
        {
            bIsBroadcasting = true;

            if constexpr (std::is_same_v<ReturnType, void>)
            {
                for (auto&& D : Delegates)
                    (*D)(InArgs...);

                bIsBroadcasting = false;
            }
            else
            {
                std::vector<ReturnType> Results;
                Results.reserve(Delegates.size());
                for (auto&& D : Delegates)
                    Results.push_back((*D)(InArgs...));

                bIsBroadcasting = false;
                return Results;
            }
        }

        bool IsBroadcasting() const { return bIsBroadcasting; }

    private:
        inline bool InsertDelegate(const std::shared_ptr<DelegateType>& DelegatePtr, bool AllowDuplicates)
        {
            if (!AllowDuplicates)
            {
                // Do not add duplicates.
                auto FoundIt = std::ranges::find_if(Delegates, [&](const auto& Data)
                    {
                        return *Data == *DelegatePtr;
                    });

                if (FoundIt != Delegates.end())
                    return false;
            }

            // Maintain order by priority.
            auto InsertIt = std::ranges::find_if(Delegates, [&](const auto& Data)
                {
                    return Data->Priority < DelegatePtr->Priority;
                });

            Delegates.insert(InsertIt, DelegatePtr);
            return true;
        }

        std::vector<std::shared_ptr<DelegateType>> Delegates;
        mutable bool bIsBroadcasting = false;;
    };
}

