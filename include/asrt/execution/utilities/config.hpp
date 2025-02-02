#ifndef ASRT_EXECUTION_UTILITIES_CONFIG_HPP_
#define ASRT_EXECUTION_UTILITIES_CONFIG_HPP_

// Check if exceptions are enabled
#if defined(__EXCEPTIONS) && !defined(LIBASRT_NO_EXCEPTIONS)
  #define ASRT_EXECUTION_HAS_EXCEPTIONS 1
#else
  #define ASRT_EXECUTION_HAS_EXCEPTIONS 0
#endif

#if __cpp_concepts
#   define ASRT_EXECUTION_HAS_CONCEPTS 1
#   define ASRT_REQUIRES(clause) requires clause
#else
#   define ASRT_REQUIRES(clause)
#endif


#define ASRT_EXEC_ATTR(attr) __attribute__((attr))

#endif //ASRT_EXECUTION_UTILITIES_CONFIG_HPP_