#ifndef FILEZILLA_ENGINE_VISIBILITY_HEADER
#define FILEZILLA_ENGINE_VISIBILITY_HEADER

// For FZ_WINDOWS/MAC/UNIX defines
#include <libfilezilla/libfilezilla.hpp>

// Symbol visibility. There are two main cases: Building FileZilla and using it
#ifdef BUILDING_FILEZILLA

  // Two cases when building: Windows, other platform
  #ifdef FZ_WINDOWS

    // Under Windows we can either use Visual Studio or a proper compiler
    #ifdef _MSC_VER
      #ifdef DLL_EXPORT
        #define FZC_PUBLIC_SYMBOL __declspec(dllexport)
      #endif
    #else
      #ifdef DLL_EXPORT
        #define FZC_PUBLIC_SYMBOL __declspec(dllexport)
      #else
        #define FZC_PUBLIC_SYMBOL __attribute__((visibility("default")))
        #define FZC_PRIVATE_SYMBOL __attribute__((visibility("hidden")))
      #endif
    #endif

  #else

    #define FZC_PUBLIC_SYMBOL __attribute__((visibility("default")))
    #define FZC_PRIVATE_SYMBOL __attribute__((visibility("hidden")))

  #endif

#else

  // Under MSW it makes a difference whether we use a static library or a DLL
  #if defined(FZ_WINDOWS) && defined(FZC_USING_DLL)
    #define FZC_PUBLIC_SYMBOL __declspec(dllimport)
  #endif

#endif


#ifndef FZC_PUBLIC_SYMBOL
#define FZC_PUBLIC_SYMBOL
#endif

#ifndef FZC_PRIVATE_SYMBOL
#define FZC_PRIVATE_SYMBOL
#endif

#endif
