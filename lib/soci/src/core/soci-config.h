
#ifndef SOCI_CONFIG_H_INCLUDED
#define SOCI_CONFIG_H_INCLUDED


#ifdef _WIN32
# ifdef SOCI_DLL
#  ifdef SOCI_SOURCE
#   define SOCI_DECL __declspec(dllexport)
#  else
#   define SOCI_DECL __declspec(dllimport)
#  endif // SOCI_SOURCE
# endif // SOCI_DLL
#endif // _WIN32
#ifndef SOCI_DECL
# define SOCI_DECL
#endif

#ifdef _MSC_VER
#pragma warning(disable:4251 4275)
#endif // _MSC_VER

#endif // SOCI_CONFIG_H_INCLUDED
