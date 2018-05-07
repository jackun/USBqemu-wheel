#ifndef GLIB_H
#define GLIB_H

#include <cstddef>
#include <cstdint>

#define G_MAXUINT64	0xffffffffffffffffUi64
#define G_MAXUINT32	((uint32_t)0xffffffff)

#include "gmem-size.h"

void* g_malloc0 (size_t n_bytes);
#define g_free free

/* Provide macros to feature the GCC function attribute.
 */
#if    __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define G_GNUC_PURE __attribute__((__pure__))
#define G_GNUC_MALLOC __attribute__((__malloc__))
#else
#define G_GNUC_PURE
#define G_GNUC_MALLOC
#endif

#if defined(__clang__) || defined(__GNUC__)
#if     (!defined(__clang__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))) || \
        (defined(__clang__) && __has_attribute(__alloc_size__))
#define G_GNUC_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
#define G_GNUC_ALLOC_SIZE2(x,y) __attribute__((__alloc_size__(x,y)))
#else
#define G_GNUC_ALLOC_SIZE(x)
#define G_GNUC_ALLOC_SIZE2(x,y)
#endif
#else
#define G_GNUC_ALLOC_SIZE(x)
#define G_GNUC_ALLOC_SIZE2(x,y)
#endif

#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define G_GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define G_GNUC_WARN_UNUSED_RESULT
#endif /* __GNUC__ */

#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define _G_BOOLEAN_EXPR(expr)                   \
 G_GNUC_EXTENSION ({                            \
   int _g_boolean_var_;                         \
   if (expr)                                    \
      _g_boolean_var_ = 1;                      \
   else                                         \
      _g_boolean_var_ = 0;                      \
   _g_boolean_var_;                             \
})
#define G_LIKELY(expr) (__builtin_expect (_G_BOOLEAN_EXPR((expr)), 1))
#define G_UNLIKELY(expr) (__builtin_expect (_G_BOOLEAN_EXPR((expr)), 0))
#else
#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)
#endif

void* g_realloc_n      (void*	 mem,
			   size_t	 n_blocks,
			   size_t	 n_block_bytes) G_GNUC_WARN_UNUSED_RESULT;

void* g_realloc        (void*	 mem,
			   size_t	 n_bytes) G_GNUC_WARN_UNUSED_RESULT;
void* g_malloc_n       (size_t	 n_blocks,
			   size_t	 n_block_bytes) G_GNUC_MALLOC G_GNUC_ALLOC_SIZE2(1,2);

/* Optimise: avoid the call to the (slower) _n function if we can
 * determine at compile-time that no overflow happens.
 */
#if defined (__GNUC__) && (__GNUC__ >= 2) && defined (__OPTIMIZE__)
#  define _G_NEW(struct_type, n_structs, func) \
	(struct_type *) (G_GNUC_EXTENSION ({			\
	  gsize __n = (gsize) (n_structs);			\
	  gsize __s = sizeof (struct_type);			\
	  gpointer __p;						\
	  if (__s == 1)						\
	    __p = g_##func (__n);				\
	  else if (__builtin_constant_p (__n) &&		\
	           (__s == 0 || __n <= G_MAXSIZE / __s))	\
	    __p = g_##func (__n * __s);				\
	  else							\
	    __p = g_##func##_n (__n, __s);			\
	  __p;							\
	}))
#  define _G_RENEW(struct_type, mem, n_structs, func) \
	(struct_type *) (G_GNUC_EXTENSION ({			\
	  gsize __n = (gsize) (n_structs);			\
	  gsize __s = sizeof (struct_type);			\
	  gpointer __p = (gpointer) (mem);			\
	  if (__s == 1)						\
	    __p = g_##func (__p, __n);				\
	  else if (__builtin_constant_p (__n) &&		\
	           (__s == 0 || __n <= G_MAXSIZE / __s))	\
	    __p = g_##func (__p, __n * __s);			\
	  else							\
	    __p = g_##func##_n (__p, __n, __s);			\
	  __p;							\
	}))

#else

/* Unoptimised version: always call the _n() function. */

#define _G_NEW(struct_type, n_structs, func) \
        ((struct_type *) g_##func##_n ((n_structs), sizeof (struct_type)))
#define _G_RENEW(struct_type, mem, n_structs, func) \
        ((struct_type *) g_##func##_n (mem, (n_structs), sizeof (struct_type)))

#endif

/**
 * g_new:
 * @struct_type: the type of the elements to allocate
 * @n_structs: the number of elements to allocate
 * 
 * Allocates @n_structs elements of type @struct_type.
 * The returned pointer is cast to a pointer to the given type.
 * If @n_structs is 0 it returns %NULL.
 * Care is taken to avoid overflow when calculating the size of the allocated block.
 * 
 * Since the returned pointer is already casted to the right type,
 * it is normally unnecessary to cast it explicitly, and doing
 * so might hide memory allocation errors.
 * 
 * Returns: a pointer to the allocated memory, cast to a pointer to @struct_type
 */
#define g_new(struct_type, n_structs)			_G_NEW (struct_type, n_structs, malloc)
/**
 * g_renew:
 * @struct_type: the type of the elements to allocate
 * @mem: the currently allocated memory
 * @n_structs: the number of elements to allocate
 * 
 * Reallocates the memory pointed to by @mem, so that it now has space for
 * @n_structs elements of type @struct_type. It returns the new address of
 * the memory, which may have been moved.
 * Care is taken to avoid overflow when calculating the size of the allocated block.
 * 
 * Returns: a pointer to the new allocated memory, cast to a pointer to @struct_type
 */
#define g_renew(struct_type, mem, n_structs)		_G_RENEW (struct_type, mem, n_structs, realloc)

/**
 * g_strdup:
 * @str: (nullable): the string to duplicate
 *
 * Duplicates a string. If @str is %NULL it returns %NULL.
 * The returned string should be freed with g_free()
 * when no longer needed.
 *
 * Returns: a newly-allocated copy of @str
 */
char*
g_strdup (const char *str);

#endif