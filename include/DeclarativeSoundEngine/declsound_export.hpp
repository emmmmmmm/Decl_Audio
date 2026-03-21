#pragma once

#ifndef DECLSOUND_API
    #if defined(_WIN32)
        #if defined(DECLSOUND_BUILD_DLL)
            #define DECLSOUND_API __declspec(dllexport)
        #else
            #define DECLSOUND_API __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) && defined(DECLSOUND_BUILD_DLL)
        #define DECLSOUND_API __attribute__((visibility("default")))
    #else
        #define DECLSOUND_API
    #endif
#endif
