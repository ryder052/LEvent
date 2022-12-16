#pragma once

template<typename T>
struct Singleton
{
    static inline T& Get()
    {
        static T Instance;
        return Instance;
    }
};