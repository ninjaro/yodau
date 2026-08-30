#ifndef YODAU_DEBUG_BUILD_CONFIG_HPP
#define YODAU_DEBUG_BUILD_CONFIG_HPP

// Observability is a developer facility. Release translation units compile out
// all construction, command-line, UI, and transport call sites.
#if !defined(NDEBUG) && !defined(QT_NO_DEBUG)                                  \
    && !defined(YODAU_DISABLE_DEBUG_OBSERVABILITY)
#define YODAU_DEBUG_OBSERVABILITY 1
#else
#define YODAU_DEBUG_OBSERVABILITY 0
#endif

#endif // YODAU_DEBUG_BUILD_CONFIG_HPP
