#pragma once
#include "LEvent.h"

namespace levent
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Manages events on global level.
    template<typename EnumType, template<typename, typename...> typename DelegateFactory>
    class Manager final : public Singleton<Manager<EnumType, DelegateFactory>>
    {
        // Function pointer aliases
        template<typename ObjectType, typename ReturnType, typename... Args>
        using Ptr2Member = ReturnType(ObjectType::*)(Args...);

        template<typename ObjectType, typename ReturnType, typename... Args>
        using Ptr2MemberC = ReturnType(ObjectType::*)(Args...) const;

        template<typename ReturnType, typename... Args>
        using Ptr2Free = ReturnType(*)(Args...);

        // Function partial specialization workarounds
        template<typename T>
        struct Detail_DeclareEvent;

        template<typename ReturnType, typename... Args>
        struct Detail_DeclareEvent<ReturnType(Args...)>
        {
            template<EnumType ID>
            static bool DeclareEvent(bool CanReplace)
            {
                auto& Slot = Manager::Get().Events[static_cast<int>(ID)];
                using EventType = LEvent<DelegateFactory, ReturnType, Args...>;

                if (CanReplace || !Slot.has_value())
                {
                    Slot = std::make_shared<EventType>();
                    return true;
                }

                return false;
            }
        };

        template<typename Callable>
        struct Detail_AddListener_Callable
        {
            template<typename OperatorType>
            struct Detail_Impl;

            template<typename ObjectType, typename ReturnType, typename... Args>
            struct Detail_Impl<Ptr2MemberC<ObjectType, ReturnType, Args...>>
            {
                template<EnumType ID>
                static auto AddListener(Callable InCallable, int Priority, bool AllowDuplicates)
                {
                    return Manager::Get().AddListener<ID, LEvent<DelegateFactory, ReturnType, Args...>>(InCallable, Priority, AllowDuplicates);
                }
            };

            using Impl = Detail_Impl<decltype(&Callable::operator())>;
        };

    public:
        void BlockEvents(bool b)
        {
            bEventsBlocked = b;
        }

        // Binds signature to ID. 
        // Usage: DeclareEvent<EnumType::ID, ReturnType(Args)>(CanReplace)
        template<EnumType ID, typename T>
        bool DeclareEvent(bool CanReplace = false)
        {
            return Detail_DeclareEvent<T>::DeclareEvent<ID>(CanReplace);
        }

        // Member functions
        template<EnumType ID, typename ObjectType, typename ReturnType, typename... Args>
        Connection AddEventListener(ObjectType* Object, Ptr2Member<ObjectType, ReturnType, Args...> FuncPtr, int Priority = 0, bool AllowDuplicates = false)
        {
            return AddListener<ID, LEvent<DelegateFactory, ReturnType, Args...>>(Object, FuncPtr, Priority, AllowDuplicates);
        }

        // Const member functions
        template<EnumType ID, typename ObjectType, typename ReturnType, typename... Args>
        Connection AddEventListener(ObjectType* Object, Ptr2MemberC<ObjectType, ReturnType, Args...> FuncPtr, int Priority = 0, bool AllowDuplicates = false)
        {
            return AddListener<ID, LEvent<DelegateFactory, ReturnType, Args...>>(Object, FuncPtr, Priority, AllowDuplicates);
        }

        // Free functions
        template<EnumType ID, typename ReturnType, typename... Args>
        Connection AddEventListener(Ptr2Free<ReturnType, Args...> FuncPtr, int Priority = 0, bool AllowDuplicates = false)
        {
            return AddListener<ID, LEvent<DelegateFactory, ReturnType, Args...>>(FuncPtr, Priority, AllowDuplicates);
        }

        // Lambda-like callables <<with const operator()>>
        template<EnumType ID, typename Callable>
        Connection AddEventListener(Callable C, int Priority = 0, bool AllowDuplicates = false)
        {
            return Detail_AddListener_Callable<Callable>::Impl::AddListener<ID>(C, Priority, AllowDuplicates);
        }

        // Calls all delegates bound to an event under id.
        // Will fail if the signatures don't match.
        template<EnumType ID, typename ReturnType = void, typename... Args>
        auto TriggerEvent(Args... InArgs)
        {
            using EventType = LEvent<DelegateFactory, ReturnType, Args...>;
            using SlotType = std::shared_ptr<EventType>;
            auto& Slot = Events[static_cast<int>(ID)];

            if constexpr (std::is_same_v<ReturnType, void>)
            {
                // Here we only return the error.
                if (bEventsBlocked)
                    return EError::EventsBlocked;

                if (!Slot.has_value() || Slot.type() != typeid(SlotType))
                    return EError::FailedToMatchEventType;

                auto& CastedSlot = std::any_cast<SlotType&>(Slot);
                CastedSlot->Trigger(std::forward<Args>(InArgs)...);
                return EError::OK;
            }
            else
            {
                // If explicitly requested, return a vector of results
                // C++23: Replace with std::expected
                struct MaybeResult
                {
                    decltype(std::declval<EventType>().Trigger(InArgs...)) Results;
                    EError Error;
                };

                if (bEventsBlocked)
                    return MaybeResult{ .Error = EError::EventsBlocked };

                if (!Slot.has_value() || Slot.type() != typeid(SlotType))
                    return MaybeResult{ .Error = EError::FailedToMatchEventType };

                auto& CastedSlot = std::any_cast<SlotType&>(Slot);
                return MaybeResult{ CastedSlot->Trigger(std::forward<Args>(InArgs)...), EError::OK };
            }
        }

        // Calls all delegates bound to an event under id.
        // Will fail if the signatures don't match.
        template<EnumType ID, typename ContainerType, typename AdderFunc, typename... Args>
        auto TriggerEventComplex(AdderFunc Adder, Args... InArgs)
        {
            using ValueType = std::decay_t<decltype(*std::declval<ContainerType>().begin())>;
            using EventType = LEvent<DelegateFactory, ValueType, Args...>;
            using SlotType = std::shared_ptr<EventType>;
            auto& Slot = Events[static_cast<int>(ID)];

            // If explicitly requested, return a vector of results
            // C++23: Replace with std::expected
            struct MaybeResult
            {
                ContainerType Results;
                EError Error;
            };

            if (bEventsBlocked)
                return MaybeResult{ .Error = EError::EventsBlocked };

            if (!Slot.has_value() || Slot.type() != typeid(SlotType))
                return MaybeResult{ .Error = EError::FailedToMatchEventType };

            auto& CastedSlot = std::any_cast<SlotType&>(Slot);
            return MaybeResult{ CastedSlot->TriggerComplex<ContainerType, AdderFunc>(Adder, std::forward<Args>(InArgs)...), EError::OK };
        }

        // For memory leak test purposes.
        void DestroyAll()
        {
            for (int i = 0; i < static_cast<int>(EnumType::Count); ++i)
                Events[i].reset();
        }

    private:
        template<EnumType ID, typename EventType, typename... Args>
        Connection AddListener(Args... InArgs)
        {
            auto& Slot = Events[static_cast<int>(ID)];
            using SlotType = std::shared_ptr<EventType>;

            if (!Slot.has_value() || Slot.type() != typeid(SlotType))
                return Connection(EError::FailedToMatchEventType);

            auto& CastedSlot = std::any_cast<SlotType&>(Slot);
            if (CastedSlot->IsBroadcasting())
                return Connection(EError::ModifyingCallbackListDuringBroadcast);

            auto&& AddedDelegate = CastedSlot->AddListener(std::forward<Args>(InArgs)...);
            if (!AddedDelegate)
                return Connection(EError::CallbackAlreadyAdded);

            return Connection(CastedSlot, AddedDelegate);
        }

        std::any Events[static_cast<int>(EnumType::Count)];
        bool bEventsBlocked = false;
    };
}