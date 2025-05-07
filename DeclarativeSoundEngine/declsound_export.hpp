#pragma once
#include "pch.h"



#ifndef DECLSOUND_API
#ifdef _WIN32
#ifdef DECLSOUND_BUILD_DLL
#define DECLSOUND_API __declspec(dllexport)
#else
#define DECLSOUND_API __declspec(dllimport)
#endif
#else
#define DECLSOUND_API
#endif
#endif