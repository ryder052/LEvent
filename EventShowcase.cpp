#include "LEventManager.h"
#include "SimpleDelegate.h"
#include <iostream>
#include <set>

using namespace levent;

enum class EListenerType
{
    Free,
    Member,
    Callable
};

EListenerType FreeListener(std::string_view Str)
{
    std::cout << "[Free] " << Str << std::endl;
    return EListenerType::Free;
}

struct Foo
{
    EListenerType MemberListener(std::string_view Str)
    {
        std::cout << "[Member] " << Str << std::endl;
        return EListenerType::Member;
    }

    static void StaticListener(int& Counter)
    {
        ++Counter;
    }
};

static auto LambdaListener = [](std::string_view Str)
{
    std::cout << "[Lambda] " << Str << std::endl;
    return EListenerType::Callable;
};

bool SingleEventTest()
{
    Foo Bar;

    // Declare event, bind 3 listeners with priority (higher = earlier executed)
    LEvent<SimpleDelegateFactory, EListenerType, std::string_view> StringEvent;
    auto FreeDelegate = StringEvent.AddListener(&FreeListener, 2);
    auto MemberDelegate = StringEvent.AddListener(&Bar, &Foo::MemberListener, 0);
    auto LambdaDelegate = StringEvent.AddListener(LambdaListener, 1);

    // Trigger with all bound
    std::vector<EListenerType> Results = StringEvent.Trigger("Event #1");
    if (Results != std::vector{ EListenerType::Free, EListenerType::Callable, EListenerType::Member })
        return false;

    // Remove the middle listener and test again
    StringEvent.RemoveListener(LambdaDelegate);
    Results = StringEvent.Trigger("Event #2");
    if (Results != std::vector{ EListenerType::Free, EListenerType::Member })
        return false;

    // Remove all and test again
    StringEvent.RemoveListener(FreeDelegate);
    StringEvent.RemoveListener(MemberDelegate);
    auto SetResults = StringEvent.TriggerComplex<std::set<EListenerType>>([](auto& C, auto&& V) { C.insert(V); }, "Event #3");
    if (!SetResults.empty())
        return false;

    return true;
}

bool EventMgrTest()
{
    enum class EEvents
    {
        String,
        Void,
        Test,

        Count // Should not be used, for Event Mgr internal use
    };

    struct Functor
    {
        EListenerType operator()(std::string_view Str) const
        {
            std::cout << "[Functor] " << Str << std::endl;
            return EListenerType::Callable;
        }
    };

    // Get singleton event manager for given enum
    auto&& Mgr = Manager<EEvents, SimpleDelegateFactory>::Get();

    {
        // Enum value -> Event signature binding
        Mgr.DeclareEvent<EEvents::String, EListenerType(std::string_view)>();

        // Adding listeners
        ScopedConnection Conn0 = Mgr.AddEventListener<EEvents::String>(Functor(), 0, true);
        ScopedConnection Conn1 = Mgr.AddEventListener<EEvents::String>(Functor(), 1, true);
        Connection Conn2 = Mgr.AddEventListener<EEvents::String>(Functor(), 2, true);
        if (!(Conn0 && Conn1 && Conn2))
            return false;

        // Trigger event, gather outputs as set
        // WARNING: Parameter types must match declared event signature without conversions or the lookup will fail!
        // Alternatively, you could explicitly specify the argument types
        auto [Results, Error] = Mgr.TriggerEventComplex<EEvents::String, std::set<EListenerType>>([](auto& S, auto&& V) {S.insert(V); }, std::string_view("Managed Event #1"));
        if (Results != std::set{ EListenerType::Callable })
            return false;

        // Manual disconnection
        Conn2.Disconnect();
    }
    // Conn0 and Conn1 Disconnect() using RAII here
    {
        // Check if everything disconnected properly
        auto [Results, Error] = Mgr.TriggerEvent<EEvents::String, EListenerType>(std::string_view("Error"));
        if (Error != EError::OK || !Results.empty())
            return false;
    }

    {
        // Test void outputs with same interface.
        Mgr.DeclareEvent<EEvents::Void, void(int&)>();
        ScopedConnection Conn0 = Mgr.AddEventListener<EEvents::Void>(&Foo::StaticListener, 0, true);
        ScopedConnection Conn1 = Mgr.AddEventListener<EEvents::Void>(&Foo::StaticListener, 0, true);
        if (!(Conn0 && Conn1))
            return false;

        int Counter = 0;

        // Note: Using explicit template parameters to pass-by-ref
        EError Result = Mgr.TriggerEvent<EEvents::Void, void, int&>(Counter);
        if (Result != EError::OK || Counter != 2)
            return false;
    }
    
    return true;
}

int main()
{
    bool SingleGood = SingleEventTest();
    bool MgrGood = EventMgrTest();
    return !SingleGood + !MgrGood;
}