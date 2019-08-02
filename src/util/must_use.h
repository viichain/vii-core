#pragma once


#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(warn_unused_result)
#define MUST_USE __attribute__((warn_unused_result))
#else
#define MUST_USE
#endif
