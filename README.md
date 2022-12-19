# LEvent
Event implementation with plug-in Delegate Factory and a handy Manager for enum association.
See full sample in EventShowcase.cpp

```C++
enum class EEvents
{
    String,
    Count // Should not be used, for Event Mgr internal use
};

struct AllInOne
{
    static EListenerType Free(std::string_view Str)
    {
        std::cout << "[Free] " << Str << std::endl;
        return EListenerType::Free;
    }

    EListenerType Member(std::string_view Str)
    {
        std::cout << "[Member] " << Str << std::endl;
        return EListenerType::Member;
    }

    EListenerType operator()(std::string_view Str) const
    {
        std::cout << "[Callable] " << Str << std::endl;
        return EListenerType::Callable;
    }
};

// Get singleton event manager for given enum
auto&& Mgr = Manager<EEvents, SimpleDelegateFactory>::Get();

{
    // Enum value -> Event signature binding
    Mgr.DeclareEvent<EEvents::String, EListenerType(std::string_view)>();

    // Adding listeners
    AllInOne aio;
    ScopedConnection Conn0 = Mgr.AddEventListener<EEvents::String>(&aio, &AllInOne::Member, 1, true);
    ScopedConnection Conn1 = Mgr.AddEventListener<EEvents::String>(&aio, &AllInOne::Member, 1, true);
    ScopedConnection Conn2 = Mgr.AddEventListener<EEvents::String>(&AllInOne::Free);
    Connection Conn3 = Mgr.AddEventListener<EEvents::String>(AllInOne());
    if (!(Conn0 && Conn1 && Conn2 && Conn3))
        return false;

    // Trigger event, gather outputs as set
    // WARNING: Parameter types must match declared event signature without conversions or the lookup will fail!
    // Alternatively, you could explicitly specify the argument types
    auto [Results, Error] = Mgr.TriggerEventComplex<EEvents::String, std::set<EListenerType>>([](auto& S, auto&& V) {S.insert(V); }, std::string_view("Managed Event #1"));
    if (Results.size() != 3)
        return false;

    // Manual disconnection
    Conn3.Disconnect();
}
// Conns 0-2 Disconnect() using RAII here
{
    // Check if everything disconnected properly
    auto [Results, Error] = Mgr.TriggerEvent<EEvents::String, EListenerType>(std::string_view("Error"));
    if (Error != EError::OK || !Results.empty())
        return false;
}
