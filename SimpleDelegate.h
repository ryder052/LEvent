#pragma once
#include <functional>

namespace levent
{
    template<typename ReturnType, typename... Args>
    class SimpleDelegateFactory
    {
        using CalleeType = std::function<ReturnType(Args...)>;

        // Free functions
        using Ptr2Free = ReturnType(*)(Args...);

        template<std::size_t... I>
        static CalleeType BindFuncImpl(Ptr2Free Func, std::index_sequence<I...>)
        {
            return std::bind(Func, std::_Ph<I + 1>()...);
        }

        template<typename Indices = std::make_index_sequence<sizeof...(Args)>>
        static CalleeType BindFunc(Ptr2Free Func)
        {
            return BindFuncImpl(Func, Indices());
        }

        // Member functions
        template<typename C, typename FuncType, std::size_t... I>
        static CalleeType BindFuncImpl(C* Obj, FuncType Func, std::index_sequence<I...>)
        {
            return std::bind(Func, Obj, std::_Ph<I + 1>()...);
        }

        template<typename C, typename FuncType, typename Indices = std::make_index_sequence<sizeof...(Args)>>
        static CalleeType BindFunc(C* Obj, FuncType Func)
        {
            return BindFuncImpl(Obj, Func, Indices());
        }

    public:
        struct Delegate
        {
            Delegate(CalleeType InCallee, int InPrioriy)
                : Callee(std::move(InCallee))
                , Priority(InPrioriy)
            {}

            CalleeType Callee;
            int Priority;

            ReturnType operator()(Args... InArgs)
            {
                return Callee(std::forward<Args>(InArgs)...);
            }

            bool operator==(const Delegate& Other) const
            {
                // Can't compare std::function's. Assume always different.
                return false;
            }
        };

        static std::shared_ptr<Delegate> MakeDelegate(Ptr2Free InFuncPtr, int InPriority)
        {
            return std::make_shared<Delegate>(BindFunc(InFuncPtr), InPriority);
        }

        template<typename ObjectType, typename FuncType>
        static std::shared_ptr<Delegate> MakeDelegate(ObjectType* InObject, FuncType InFunc, int InPriority)
        {
            Delegate d(BindFunc(InObject, InFunc), InPriority);
            return std::make_shared<Delegate>(d);
        }

        template<typename T>
        static std::shared_ptr<Delegate> MakeDelegate(T Callable, int InPriority)
        {
            return std::make_shared<Delegate>(Callable, InPriority);
        }
    };
}