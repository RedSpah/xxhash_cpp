#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <type_traits>
#include <vector>
#include <string>
#include <iostream>

/*
xxHash - Extremely Fast Hash algorithm
Header File
Copyright (C) 2012-Present, Yann Collet.
Copyright (C) 2017-Present, Red Gavin.
All rights reserved.

BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
You can contact the author at :
- xxHash source repository : https://github.com/Cyan4973/xxHash
- xxHash C++ port repository : https://github.com/RedSpah/xxhash_cpp
*/

/* Intrinsics
* Sadly has to be included in the global namespace or literally everything breaks
*/
#include <immintrin.h>

namespace xxh
{
	/* *************************************
	*  Version
	***************************************/
	constexpr int cpp_version_major = 0;
	constexpr int cpp_version_minor = 7;
	constexpr int cpp_version_release = 2;
	constexpr uint32_t version_number() { return cpp_version_major * 10000 + cpp_version_minor * 100 + cpp_version_release; }

	/* *************************************
	*  Compiler / Platform Specific Features
	***************************************/
	namespace intrin
	{
		/*!XXH_FORCE_ALIGN_CHECK :
		* This is a minor performance trick, only useful with lots of very small keys.
		* It means : check for aligned/unaligned input.
		* The check costs one initial branch per hash;
		* set it to 0 when the input is guaranteed to be aligned,
		* or when alignment doesn't matter for performance.
		*/
#ifndef XXH_FORCE_ALIGN_CHECK /* can be defined externally */
#	if defined(__i386) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#		define XXH_FORCE_ALIGN_CHECK 0
#	else
#		define XXH_FORCE_ALIGN_CHECK 1
#	endif
#endif


		/*!XXH_CPU_LITTLE_ENDIAN :
		* This is a CPU endian detection macro, will be
		* automatically set to 1 (little endian) if it is left undefined.
		* If compiling for a big endian system (why), XXH_CPU_LITTLE_ENDIAN has to be explicitly defined as 0.
		*/
#ifndef XXH_CPU_LITTLE_ENDIAN
#	define XXH_CPU_LITTLE_ENDIAN 1
#endif


		/* Vectorization Detection
		* NOTE: XXH_NEON and XXH_VSX aren't supported in this C++ port.
		* The primary reason is that I don't have access to an ARM and PowerPC
		* machines to test them, and the secondary reason is that I even doubt anyone writing
		* code for such machines would bother using a C++ port rather than the original C version.
		*/
#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__AVX2__)
		constexpr int vector_mode = 2;
		constexpr int acc_align = 32;
		using _vec128_underlying = __m128i;
		using _vec256_underlying = __m256i;
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
		constexpr int vector_mode = 1;
		constexpr int acc_align = 16;
		using _vec128_underlying = __m128i;
		using _vec256_underlying = __m256i;
		//using _vec256_underlying = std::array<uint8_t, 32>;
#  else /* what in tarnation are you using this library on*/
		constexpr int vector_mode = 0;
		constexpr int acc_align = 8;
		using _vec128_underlying = __m128i;
		using _vec256_underlying = __m256i;
		//using _vec128_underlying = std::array<uint8_t, 16>;
		//using _vec256_underlying = std::array<uint8_t, 32>;
#  endif
#endif

		/* Compiler Specifics
		* Defines inline macros and includes specific compiler's instrinsics.
		* */
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)    
#  define XXH_FORCE_INLINE static __forceinline
#  define XXH_NO_INLINE static __declspec(noinline)
#  include <intrin.h>
#elif defined(__GNUC__)  /* Clang / GCC */
#  define XXH_FORCE_INLINE static inline __attribute__((always_inline))
#  define XXH_NO_INLINE static __attribute__((noinline))
#  include <mmintrin.h>
#else
#  define XXH_FORCE_INLINE static inline
#  define XXH_NO_INLINE static
#endif


		/* Prefetch
		* Can be disabled by defining XXH_NO_PREFETCH
		*/
#if defined(XXH_NO_PREFETCH)
		void prefetch(const void* ptr) {}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_I86))  
		XXH_FORCE_INLINE void prefetch(const void* ptr) { _mm_prefetch((const char*)(ptr), _MM_HINT_T0); }
#elif defined(__GNUC__) 
		XXH_FORCE_INLINE void prefetch(const void* ptr) { __builtin_prefetch((ptr), 0, 3); }
#else
		void prefetch(const void* ptr) {}
#endif


		/* Restrict
		* Defines macro for restrict, which in C++ is sadly just a compiler extension (for now).
		* Can be disabled by defining XXH_NO_RESTRICT
		*/
#if (defined(__GNUC__) || defined(_MSC_VER)) && defined(__cplusplus) && !defined(XXH_NO_RESTRICT)
#  define XXH_RESTRICT  __restrict
#else
#  define XXH_RESTRICT 
#endif

		/* Has to be predeclared here for use with mul64to128 */
		struct alignas(16) _uint128 {
			uint64_t low64 = 0;
			uint64_t high64 = 0;
		};

		namespace bit_ops
		{
#if defined(_MSC_VER)
			inline uint32_t rotl32(uint32_t x, int32_t r) { return _rotl(x, r); }
			inline uint64_t rotl64(uint64_t x, int32_t r) { return _rotl64(x, r); }
			inline uint32_t rotr32(uint32_t x, int32_t r) { return _rotr(x, r); }
			inline uint64_t rotr64(uint64_t x, int32_t r) { return _rotr64(x, r); }
#else
			inline uint32_t rotl32(uint32_t x, int32_t r) { return ((x << r) | (x >> (32 - r))); }
			inline uint64_t rotl64(uint64_t x, int32_t r) { return ((x << r) | (x >> (64 - r))); }
			inline uint32_t rotr32(uint32_t x, int32_t r) { return ((x >> r) | (x << (32 - r))); }
			inline uint64_t rotr64(uint64_t x, int32_t r) { return ((x >> r) | (x << (64 - r))); }
#endif

#if defined(_MSC_VER)     /* Visual Studio */
			inline uint32_t swap32(uint32_t x) { return _byteswap_ulong(x); }
			inline uint64_t swap64(uint64_t x) { return _byteswap_uint64(x); }
#elif defined(__GNUC__)
			inline uint32_t swap32(uint32_t x) { return __builtin_bswap32(x); }
			inline uint64_t swap64(uint64_t x) { return __builtin_bswap64(x); }
#else
			inline uint32_t swap32(uint32_t x) { return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) | ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff); }
			inline uint64_t swap64(uint64_t x) { return ((x << 56) & 0xff00000000000000ULL) | ((x << 40) & 0x00ff000000000000ULL) | ((x << 24) & 0x0000ff0000000000ULL) | ((x << 8) & 0x000000ff00000000ULL) | ((x >> 8) & 0x00000000ff000000ULL) | ((x >> 24) & 0x0000000000ff0000ULL) | ((x >> 40) & 0x000000000000ff00ULL) | ((x >> 56) & 0x00000000000000ffULL); }
#endif

#if defined(_MSC_VER) && defined(_M_IX86) // Only for 32-bit MSVC.
			inline uint64_t mult32to64(uint32_t x, uint32_t y) { return __emulu(x, y); }
#else
			inline uint64_t mult32to64(uint32_t x, uint32_t y) { return (uint64_t)((x) & 0xFFFFFFFF) * (uint64_t)((y) & 0xFFFFFFFF); }
#endif




#if defined(__GNUC__) && !defined(__clang__) && defined(__i386__)
			__attribute__((__target__("no-sse")))
#endif
			inline _uint128 mult64to128(uint64_t lhs, uint64_t rhs)
			{

#if defined(__GNUC__) && !defined(__wasm__) \
    && defined(__SIZEOF_INT128__) \
    || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

				__uint128_t product = (__uint128_t)lhs * (__uint128_t)rhs;
				_uint128 const r128 = { (uint64_t)(product), (uint64_t)(product >> 64) };
				return r128;

#elif defined(_M_X64) || defined(_M_IA64)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
				uint64_t product_high;
				uint64_t const product_low = _umul128(lhs, rhs, &product_high);
				return _uint128{ product_low, product_high };

#else

				uint64_t  const lo_lo = bit_ops::mult32to64(lhs & 0xFFFFFFFF, rhs & 0xFFFFFFFF);
				uint64_t  const hi_lo = bit_ops::mult32to64(lhs >> 32, rhs & 0xFFFFFFFF);
				uint64_t  const lo_hi = bit_ops::mult32to64(lhs & 0xFFFFFFFF, rhs >> 32);
				uint64_t  const hi_hi = bit_ops::mult32to64(lhs >> 32, rhs >> 32);

				/* Now add the products together. These will never overflow. */
				uint64_t  const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
				uint64_t  const upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;
				uint64_t  const lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);

				_uint128 r128 = { lower, upper };
				return r128;
#endif
			}

		}

	}




	namespace hash_t_impl
	{
		/* *************************************
		*  Basic Types - Detail
		***************************************/

		template <size_t N>
		struct hash_type { using type = void; };
		template <>
		struct hash_type<32> { using type = uint32_t; };
		template <>
		struct hash_type<64> { using type = uint64_t; };
		template <>
		struct hash_type<128> { using type = intrin::_uint128; };

		template <size_t N>
		struct vec_type { using type = void; };
		template <>
		struct vec_type<64> { using type = uint64_t; };
		template <>
		struct vec_type<128> { using type = intrin::_vec128_underlying; };
		template <>
		struct vec_type<256> { using type = intrin::_vec256_underlying; };

		/* Rationale
		* On the surface level uint_type appears to be pointless, 
		* as it is just a copy of hash_type. They do use the same types,
		* that is true, but the reasoning for the difference is aimed at humans,
		* not the compiler, as a difference between values that are 'just' numbers,
		* and those that represent actual hash values.
		*/
		template <size_t N>
		struct uint_type { using type = void; };
		template <>
		struct uint_type<32> { using type = uint32_t; };
		template <>
		struct uint_type<64> { using type = uint64_t; };
		template <>
		struct uint_type<128> { using type = intrin::_uint128; };
	}

	/* *************************************
	*  Basic Types - Public
	***************************************/

	template <size_t N>
	using hash_t = typename hash_t_impl::hash_type<N>::type;
	using hash32_t = hash_t<32>;
	using hash64_t = hash_t<64>;
	using hash128_t = hash_t<128>;

	template <size_t N>
	using vec_t = typename hash_t_impl::vec_type<N>::type;
	using vec64_t = vec_t<64>;
	using vec128_t = vec_t<128>;
	using vec256_t = vec_t<256>;

	template <size_t N>
	using uint_t = typename hash_t_impl::uint_type<N>::type;
	using uint128_t = intrin::_uint128;

	/* *************************************
	*  Bit Functions - Public
	***************************************/

	namespace bit_ops
	{
		/* ****************************************
		*  Intrinsics and Bit Operations
		******************************************/
		template <size_t N>
		inline uint_t<N> rotl(uint_t<N> n, int32_t r)
		{
			if constexpr (N == 32)
			{
				return intrin::bit_ops::rotl32(n, r);
			}

			if constexpr (N == 64)
			{
				return intrin::bit_ops::rotl64(n, r);
			}
		};

		template <size_t N>
		inline uint_t<N> rotr(uint_t<N> n, int32_t r)
		{
			if constexpr (N == 32)
			{
				return intrin::bit_ops::rotr32(n, r);
			}

			if constexpr (N == 64)
			{
				return intrin::bit_ops::rotr64(n, r);
			}
		};

		template <size_t N>
		inline uint_t<N> swap(hash_t<N> n) 
		{
			if constexpr (N == 32)
			{
				return intrin::bit_ops::swap32(n);
			}

			if constexpr (N == 64)
			{
				return intrin::bit_ops::swap64(n);
			}
		};

		inline uint64_t mul32to64(uint32_t x, uint32_t y) 
		{ 
			return intrin::bit_ops::mult32to64(x, y); 
		}

		inline uint128_t mul64to128(uint64_t x, uint64_t y) 
		{ 
			return intrin::bit_ops::mult64to128(x, y); 
		}

		inline uint64_t mul128fold64(uint64_t lhs, uint64_t rhs)
		{
			uint128_t product = mul64to128(lhs, rhs);
			return product.low64 ^ product.high64;
		}
	}

	/* *************************************
	*  Memory Functions - Public
	***************************************/

	enum class alignment : uint8_t { aligned, unaligned };
	enum class endianness : uint8_t { big_endian = 0, little_endian = 1, unspecified = 2 };

	namespace mem_ops
	{
		/* *************************************
		*  Memory Access
		***************************************/
		template <size_t N>
		inline uint_t<N> read_unaligned(const void* memPtr)
		{
			uint_t<N> val;
			memcpy(&val, memPtr, sizeof(val));
			return val;
		}


		inline uint32_t read32(const void* memPtr) { return read_unaligned<32>(memPtr); }
		inline uint64_t read64(const void* memPtr) { return read_unaligned<64>(memPtr); }

		/* *************************************
		*  Endianness
		***************************************/

		constexpr endianness get_endian(endianness endian = endianness::unspecified)
		{
			constexpr std::array<endianness, 3> endian_lookup = { endianness::big_endian, endianness::little_endian, (XXH_CPU_LITTLE_ENDIAN) ? endianness::little_endian : endianness::big_endian };
			return endian_lookup[static_cast<uint8_t>(endian)];
		}

		constexpr bool is_little_endian()
		{
			return get_endian(endianness::unspecified) == endianness::little_endian;
		}

		/* ***************************
		*  Memory reads
		*****************************/


		template <size_t N>
		inline uint_t<N> readLE_align(const void* ptr, endianness endian, alignment align)
		{
			if (align == alignment::unaligned)
			{
				return ((endian == endianness::little_endian) ? read_unaligned<N>(ptr) : bit_ops::swap<N>(read_unaligned<N>(ptr)));
			}
			else
			{
				return ((endian == endianness::little_endian) ? *reinterpret_cast<const uint_t<N>*>(ptr) : bit_ops::swap<N>(*reinterpret_cast<const uint_t<N>*>(ptr)));
			}
		}

		template <size_t N>
		inline uint_t<N> readLE(const void* ptr, endianness endian = endianness::unspecified)
		{
			return readLE_align<N>(ptr, get_endian(endian), alignment::unaligned);
		}

		template <size_t N>
		inline uint_t<N> readBE(const void* ptr)
		{
			return is_little_endian() ? bit_ops::swap<N>(read_unaligned<N>(ptr)) : read_unaligned<N>(ptr);
		}

		template <size_t N>
		inline alignment get_alignment(const void* input)
		{
			return ((XXH_FORCE_ALIGN_CHECK) && ((reinterpret_cast<uintptr_t>(input)& ((N / 8) - 1)) == 0)) ? xxh::alignment::aligned : xxh::alignment::unaligned;
		}

		template <size_t N>
		void writeLE(void* dst, uint_t<N> v)
		{
			if (!is_little_endian())
			{
				v = bit_ops::swap<N>(v);
			}
			memcpy(dst, &v, sizeof(v));
		}


	}

	/* *************************************
	*  Vector Functions - Public
	***************************************/

	namespace vec_ops
	{

		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> loadu(const vec_t<N>* input)
		{ 
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid template argument passed to xxh::vec_ops::loadu"); 

			if constexpr (N == 128)
			{
				return _mm_loadu_si128(input);
			}

			if constexpr (N == 256)
			{
				return _mm256_loadu_si256(input);
			}

			if constexpr (N == 64)
			{
				return mem_ops::read_unaligned<64>(input);
			}
		}


		// 'xorv' instead of 'xor' because 'xor' is a weird wacky alternate operator expression thing. 
		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> xorv(vec_t<N> a, vec_t<N> b)
		{ 
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::xor");
		
			if constexpr (N == 128)
			{
				return _mm_xor_si128(a, b);
			}

			if constexpr (N == 256)
			{
				return _mm256_xor_si256(a, b);
			}

			if constexpr (N == 64)
			{
				return a ^ b;
			}
		};
		

		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> mul(vec_t<N> a, vec_t<N> b)
		{
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::mul");

			if constexpr (N == 128)
			{
				return _mm_mul_epu32(a, b);
			}

			if constexpr (N == 256)
			{
				return _mm256_mul_epu32(a, b);
			}

			if constexpr (N == 64)
			{
				return a * b;
			}
		};

		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> add(vec_t<N> a, vec_t<N> b)
		{
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::add");

			if constexpr (N == 128)
			{
				return _mm_add_epi64(a, b);
			}

			if constexpr (N == 256)
			{
				return _mm256_add_epi64(a, b);
			}

			if constexpr (N == 64)
			{
				return a + b;
			}
		};


		template <size_t N, uint8_t S1, uint8_t S2, uint8_t S3, uint8_t S4>
		XXH_FORCE_INLINE vec_t<N> shuffle(vec_t<N> a)
		{ 
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::shuffle");

			if constexpr (N == 128)
			{
				return _mm_shuffle_epi32(a, _MM_SHUFFLE(S1, S2, S3, S4));
			}

			if constexpr (N == 256)
			{
				return _mm256_shuffle_epi32(a, _MM_SHUFFLE(S1, S2, S3, S4));
			}

			if constexpr (N == 64)
			{
				return a;
			}
		};


		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> set1(int a)
		{
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::set1");

			if constexpr (N == 128)
			{
				return _mm_set1_epi32(a);
			}

			if constexpr (N == 256)
			{
				return _mm256_set1_epi32(a);
			}

			if constexpr (N == 64)
			{
				return a;
			}
		};


		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> srli(vec_t<N> n, int a)
		{
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::srli");

			if constexpr (N == 128)
			{
				return _mm_srli_epi64(n, a);
			}

			if constexpr (N == 256)
			{
				return _mm256_srli_epi64(n, a);
			}

			if constexpr (N == 64)
			{
				return bit_ops::rotr<64>(n, a);
			}
		};


		template <size_t N>
		XXH_FORCE_INLINE vec_t<N> slli(vec_t<N> n, int a)
		{
			static_assert(!(N != 128 && N != 256 && N != 64), "Invalid argument passed to xxh::vec_ops::slli");

			if constexpr (N == 128)
			{
				return _mm_slli_epi64(n, a);
			}

			if constexpr (N == 256)
			{
				return _mm256_slli_epi64(n, a);
			}

			if constexpr (N == 64)
			{
				return bit_ops::rotl<64>(n, a);
			}
		};
	}

	/* *******************************************************************
	*  Hash functions
	*********************************************************************/

	namespace detail
	{
		/* *******************************************************************
		*  Hash functions - Implementation
		*********************************************************************/

		constexpr static std::array<hash32_t, 5> primes32 = { 2654435761U, 2246822519U, 3266489917U, 668265263U, 374761393U };
		constexpr static std::array<hash64_t, 5> primes64 = { 11400714785074694791ULL, 14029467366897019727ULL, 1609587929392839161ULL, 9650029242287828579ULL, 2870177450012600261ULL };

		template <size_t N>
		constexpr hash_t<N> PRIME(int64_t n) {};

		template <>
		constexpr hash32_t PRIME<32>(int64_t n)
		{
			return primes32[n - 1];
		}

		template <>
		constexpr hash64_t PRIME<64>(int64_t n)
		{
			return primes64[n - 1];
		}

		template <size_t N>
		inline hash_t<N> round(hash_t<N> seed, hash_t<N> input)
		{
			seed += input * PRIME<N>(2);
			seed = bit_ops::rotl<N>(seed, ((N == 32) ? 13 : 31));
			seed *= PRIME<N>(1);
			return seed;
		}

		inline hash64_t mergeRound64(hash64_t acc, hash64_t val)
		{
			val = round<64>(0, val);
			acc ^= val;
			acc = acc * PRIME<64>(1) + PRIME<64>(4);
			return acc;
		}

		template <size_t N>
		inline void endian_align_sub_mergeround([[maybe_unused]] hash_t<N>& hash_ret, hash_t<N> v1, hash_t<N> v2, hash_t<N> v3, hash_t<N> v4) {};

		template <>
		inline void endian_align_sub_mergeround<64>(hash_t<64>& hash_ret, hash_t<64> v1, hash_t<64> v2, hash_t<64> v3, hash_t<64> v4)
		{
			hash_ret = mergeRound64(hash_ret, v1);
			hash_ret = mergeRound64(hash_ret, v2);
			hash_ret = mergeRound64(hash_ret, v3);
			hash_ret = mergeRound64(hash_ret, v4);
		}

		template <size_t N>
		inline hash_t<N> endian_align_sub_ending(hash_t<N> hash_ret, const uint8_t* p, const uint8_t* bEnd, xxh::endianness endian, xxh::alignment align) {};

		template <>
		inline hash_t<32> endian_align_sub_ending<32>(hash_t<32> hash_ret, const uint8_t* p, const uint8_t* bEnd, xxh::endianness endian, xxh::alignment align)
		{
			while ((p + 4) <= bEnd)
			{
				hash_ret += mem_ops::readLE_align<32>(p, endian, align) * PRIME<32>(3);
				hash_ret = bit_ops::rotl<32>(hash_ret, 17) * PRIME<32>(4);
				p += 4;
			}

			while (p < bEnd)
			{
				hash_ret += (*p) * PRIME<32>(5);
				hash_ret = bit_ops::rotl<32>(hash_ret, 11) * PRIME<32>(1);
				p++;
			}

			hash_ret ^= hash_ret >> 15;
			hash_ret *= PRIME<32>(2);
			hash_ret ^= hash_ret >> 13;
			hash_ret *= PRIME<32>(3);
			hash_ret ^= hash_ret >> 16;

			return hash_ret;
		}

		template <>
		inline hash_t<64> endian_align_sub_ending<64>(hash_t<64> hash_ret, const uint8_t* p, const uint8_t* bEnd, xxh::endianness endian, xxh::alignment align)
		{
			while (p + 8 <= bEnd)
			{
				const hash64_t k1 = round<64>(0, mem_ops::readLE_align<64>(p, endian, align));
				hash_ret ^= k1;
				hash_ret = bit_ops::rotl<64>(hash_ret, 27) * PRIME<64>(1) + PRIME<64>(4);
				p += 8;
			}

			if (p + 4 <= bEnd)
			{
				hash_ret ^= static_cast<hash64_t>(mem_ops::readLE_align<32>(p, endian, align))* PRIME<64>(1);
				hash_ret = bit_ops::rotl<64>(hash_ret, 23) * PRIME<64>(2) + PRIME<64>(3);
				p += 4;
			}

			while (p < bEnd)
			{
				hash_ret ^= (*p) * PRIME<64>(5);
				hash_ret = bit_ops::rotl<64>(hash_ret, 11) * PRIME<64>(1);
				p++;
			}

			hash_ret ^= hash_ret >> 33;
			hash_ret *= PRIME<64>(2);
			hash_ret ^= hash_ret >> 29;
			hash_ret *= PRIME<64>(3);
			hash_ret ^= hash_ret >> 32;

			return hash_ret;
		}

		template <size_t N>
		inline hash_t<N> endian_align(const void* input, size_t len, hash_t<N> seed, xxh::endianness endian, xxh::alignment align)
		{
			static_assert(!(N != 32 && N != 64), "You can only call endian_align in 32 or 64 bit mode.");

			const uint8_t* p = static_cast<const uint8_t*>(input);
			const uint8_t* bEnd = p + len;
			hash_t<N> hash_ret;

			if (len >= (N / 2))
			{
				const uint8_t* const limit = bEnd - (N / 2);
				hash_t<N> v1 = seed + PRIME<N>(1) + PRIME<N>(2);
				hash_t<N> v2 = seed + PRIME<N>(2);
				hash_t<N> v3 = seed + 0;
				hash_t<N> v4 = seed - PRIME<N>(1);

				do
				{
					v1 = round<N>(v1, mem_ops::readLE_align<N>(p, endian, align)); p += (N / 8);
					v2 = round<N>(v2, mem_ops::readLE_align<N>(p, endian, align)); p += (N / 8);
					v3 = round<N>(v3, mem_ops::readLE_align<N>(p, endian, align)); p += (N / 8);
					v4 = round<N>(v4, mem_ops::readLE_align<N>(p, endian, align)); p += (N / 8);
				} while (p <= limit);

				hash_ret = bit_ops::rotl<N>(v1, 1) + bit_ops::rotl<N>(v2, 7) + bit_ops::rotl<N>(v3, 12) + bit_ops::rotl<N>(v4, 18);

				endian_align_sub_mergeround<N>(hash_ret, v1, v2, v3, v4);
			}
			else { hash_ret = seed + PRIME<N>(5); }

			hash_ret += static_cast<hash_t<N>>(len);

			return endian_align_sub_ending<N>(hash_ret, p, bEnd, endian, align);
		}
	}

	/* *******************************************************************
	*  Hash functions - XXH3
	*********************************************************************/

	enum class vec_mode : uint8_t { scalar = 0, sse2 = 1, avx2 = 2 };

	namespace detail3
	{
		using namespace vec_ops;
		using namespace detail;
		using namespace mem_ops;
		using namespace bit_ops;

		/* Constants */
		constexpr int secret_default_size = 192;
		constexpr int secret_size_min = 136;
		constexpr int secret_consume_rate = 8;
		constexpr int stripe_len = 64;
		constexpr int acc_nb = 8;
		constexpr int prefetch_distance = 384;
		constexpr int secret_lastacc_start = 7;
		constexpr int secret_mergeaccs_start = 11;
		constexpr int midsize_max = 240;
		constexpr int midsize_startoffset = 3;
		constexpr int midsize_lastoffset = 17;

		/* Compiler Constants */
		constexpr vec_mode vector_mode = static_cast<vec_mode>(intrin::vector_mode);
		constexpr int acc_align = intrin::acc_align;
		
		/* Defaults */
		alignas(64) constexpr uint8_t default_secret[secret_default_size] = {
	0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
	0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
	0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
	0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
	0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
	0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
	0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
	0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
	0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
	0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
	0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
	0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
		};

		constexpr std::array<uint64_t, 8> init_acc = { PRIME<32>(3), PRIME<64>(1), PRIME<64>(2), PRIME<64>(3), PRIME<64>(4), PRIME<32>(2), PRIME<64>(5), PRIME<32>(1) };

		constexpr std::array<int, 3> vec_bit_width = { 64, 128, 256 };
		constexpr std::array<int, 3> a512_i_xor = { 1, 0, 0 };

		enum class acc_width : uint8_t { acc_64bits, acc_128bits };
		


		static hash_t<64> avalanche(hash_t<64> h64)
		{
			h64 ^= h64 >> 37;
			h64 *= PRIME<64>(3);
			h64 ^= h64 >> 32;
			return h64;
		}

		template <vec_mode V>
		void accumulate_512(void* XXH_RESTRICT acc, const void* XXH_RESTRICT input, const void* XXH_RESTRICT secret, acc_width width)
		{
			constexpr int bits = vec_bit_width[static_cast<uint8_t>(V)];

			using vec_t = vec_t<bits>;
			
			alignas(sizeof(vec_t)) vec_t* const xacc = static_cast<vec_t*>(acc);
			const vec_t* const xinput = static_cast<const vec_t*>(input);
			const vec_t* const xsecret = static_cast<const vec_t*>(secret);

			for (size_t i = 0; i < stripe_len / sizeof(vec_t); i++)
			{
				vec_t const data_vec = loadu<bits>(xinput + i);
				vec_t const key_vec = loadu<bits>(xsecret + i);
				vec_t const data_key = xorv<bits>(data_vec, key_vec);
				vec_t product = set1<bits>(0);

				if constexpr (bits > 64)
				{
					product = mul<bits>(data_key, vec_ops::shuffle<bits, 0, 3, 0, 1>(data_key));
				}
				else 
				{
					product = mul32to64(data_key & 0xFFFFFFFF, data_key >> 32);
				}

				if (width == acc_width::acc_128bits)
				{
					vec_t const data_swap = shuffle<bits, 1, 0, 3, 2>(data_vec);
					vec_t const sum = add<bits>(xacc[i], data_swap);
					
					xacc[i ^ a512_i_xor[static_cast<uint8_t>(V)]] = add<bits>(sum, product);
				}	
				else
				{
					vec_t const sum = add<bits>(xacc[i], data_vec);
					xacc[i] = add<bits>(sum, product);
				}
			}
		}

		template <vec_mode V>
		void scramble_acc(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
		{
			constexpr int bits = vec_bit_width[static_cast<uint8_t>(V)];
			using vec_t = vec_t<bits>;

			alignas(sizeof(vec_t)) vec_t* const xacc = (vec_t*)acc;
			const vec_t* const xsecret = (const vec_t*)secret;  
			const vec_t prime32 = set1<bits>(PRIME<32>(1));

			for (size_t i = 0; i < stripe_len / sizeof(vec_t); i++)
			{
				vec_t const acc_vec = xacc[i];
				vec_t const shifted = srli<bits>(acc_vec, 47);
				vec_t const data_vec = xorv<bits>(acc_vec, shifted);

				vec_t const key_vec = loadu<bits>(xsecret + i);
				vec_t const data_key = xorv<bits>(data_vec, key_vec);

				if constexpr (bits > 64)
				{
					vec_t const data_key_hi = shuffle<bits, 0, 3, 0, 1>(data_key);
					vec_t const prod_lo = mul<bits>(data_key, prime32);
					vec_t const prod_hi = mul<bits>(data_key_hi, prime32);

					xacc[i] = add<bits>(prod_lo, vec_ops::slli<bits>(prod_hi, 32));
				}
				else 
				{
					xacc[i] = mul<bits>(prime32, data_key);
				}
			}
		}

		static void accumulate(uint64_t* XXH_RESTRICT acc, const uint8_t* XXH_RESTRICT input, const uint8_t* XXH_RESTRICT secret, size_t nbStripes, acc_width accWidth)
		{
			for (size_t n = 0; n < nbStripes; n++) {
				const uint8_t* const in = input + n * stripe_len;
				intrin::prefetch(in + prefetch_distance);
				accumulate_512<vector_mode>(acc, in, secret + n * secret_consume_rate, accWidth);
			}
		}

		static void hash_long_internal_loop(uint64_t* XXH_RESTRICT acc, const uint8_t* XXH_RESTRICT input, size_t len, const uint8_t* XXH_RESTRICT secret, size_t secretSize, acc_width accWidth)
		{
			size_t const nb_rounds = (secretSize - stripe_len) / secret_consume_rate;
			size_t const block_len = stripe_len * nb_rounds;
			size_t const nb_blocks = len / block_len;

			for (size_t n = 0; n < nb_blocks; n++) {
				accumulate(acc, input + n * block_len, secret, nb_rounds, accWidth);
				scramble_acc<vector_mode>(acc, secret + secretSize - stripe_len);
			}

			/* last partial block */
			size_t const nbStripes = (len - (block_len * nb_blocks)) / stripe_len;
			accumulate(acc, input + nb_blocks * block_len, secret, nbStripes, accWidth);

			/* last stripe */
			if (len & (stripe_len - 1)) {
				const uint8_t* const p = input + len - stripe_len;
				accumulate_512<vector_mode>(acc, p, secret + secretSize - stripe_len - secret_lastacc_start, accWidth);
			}
		}

		static uint64_t mix_2_accs(const uint64_t* XXH_RESTRICT acc, const uint8_t* XXH_RESTRICT secret)
		{
			return mul128fold64(acc[0] ^ readLE<64>(secret), acc[1] ^ readLE<64>(secret + 8));
		}

		static uint64_t merge_accs(const uint64_t* XXH_RESTRICT acc, const uint8_t* XXH_RESTRICT secret, uint64_t start)
		{
			uint64_t result64 = start;

			result64 += mix_2_accs(acc + 0, secret + 0);
			result64 += mix_2_accs(acc + 2, secret + 16);
			result64 += mix_2_accs(acc + 4, secret + 32);
			result64 += mix_2_accs(acc + 6, secret + 48);

			return avalanche(result64);
		}

		static void init_custom_secret(uint8_t* customSecret, uint64_t seed64)
		{
			for (uint64_t i = 0; i < secret_default_size / 16; i++) {
				writeLE<64>(customSecret + i * 16, readLE<64>(default_secret + i * 16) + seed64);
				writeLE<64>(customSecret + i * 16 + 8, readLE<64>(default_secret + i * 16 + 8) - seed64);
			}
		}

		template <size_t N>
		hash_t<N> len_1to3(const uint8_t* input, size_t len, const uint8_t* secret, hash64_t seed)
		{
			if constexpr (N == 64)
			{
				uint8_t const c1 = input[0];
				uint8_t const c2 = input[len >> 1];
				uint8_t const c3 = input[len - 1];
				uint32_t const combined = ((uint32_t)c1) | (((uint32_t)c2) << 8) | (((uint32_t)c3) << 16) | (((uint32_t)len) << 24);
				uint64_t const keyed = (uint64_t)combined ^ (readLE<32>(secret) + seed);
				uint64_t const mixed = keyed * PRIME<64>(1);
				return avalanche(mixed);
			}
			else
			{
				uint8_t const c1 = input[0];
				uint8_t const c2 = input[len >> 1];
				uint8_t const c3 = input[len - 1];
				uint32_t  const combinedl = ((uint32_t)c1) + (((uint32_t)c2) << 8) + (((uint32_t)c3) << 16) + (((uint32_t)len) << 24);
				uint32_t  const combinedh = swap<32>(combinedl);
				uint64_t  const keyed_lo = (uint64_t)combinedl ^ (readLE<32>(secret) + seed);
				uint64_t  const keyed_hi = (uint64_t)combinedh ^ (readLE<32>(secret + 4) - seed);
				uint64_t  const mixedl = keyed_lo * PRIME<64>(1);
				uint64_t  const mixedh = keyed_hi * PRIME<64>(5);
				hash128_t const h128 = { avalanche(mixedl) /*low64*/, avalanche(mixedh) /*high64*/ };
				return h128;
			}
		}

		template <size_t N>
		hash_t<N> len_4to8(const uint8_t* input, size_t len, const uint8_t* secret, hash64_t seed)
		{
			if constexpr (N == 64)
			{
				uint32_t const input_lo = readLE<32>(input);
				uint32_t const input_hi = readLE<32>(input + len - 4);
				uint64_t const input_64 = input_lo | ((uint64_t)input_hi << 32);
				uint64_t const keyed = input_64 ^ (readLE<64>(secret) + seed);
				uint64_t const mix64 = len + ((keyed ^ (keyed >> 51))* PRIME<32>(1));
				return avalanche((mix64 ^ (mix64 >> 47))* PRIME<64>(2));
			}
			else
			{
				uint32_t const input_lo = readLE<32>(input);
				uint32_t const input_hi = readLE<32>(input + len - 4);
				uint64_t const input_64_lo = input_lo + ((uint64_t)input_hi << 32);
				uint64_t const input_64_hi = swap<64>(input_64_lo);
				uint64_t const keyed_lo = input_64_lo ^ (readLE<64>(secret) + seed);
				uint64_t const keyed_hi = input_64_hi ^ (readLE<64>(secret + 8) - seed);
				uint64_t const mix64l1 = len + ((keyed_lo ^ (keyed_lo >> 51))* PRIME<32>(1));
				uint64_t const mix64l2 = (mix64l1 ^ (mix64l1 >> 47))* PRIME<64>(2);
				uint64_t const mix64h1 = ((keyed_hi ^ (keyed_hi >> 47))* PRIME<64>(1)) - len;
				uint64_t const mix64h2 = (mix64h1 ^ (mix64h1 >> 43))* PRIME<64>(4);
				hash128_t const h128 = { avalanche(mix64l2) /*low64*/, avalanche(mix64h2) /*high64*/ };
				return h128;
			}
		}

		template <size_t N>
		hash_t<N> len_9to16(const uint8_t* input, size_t len, const uint8_t* secret, hash64_t seed)
		{
			if constexpr (N == 64)
			{
				uint64_t const input_lo = readLE<64>(input) ^ (readLE<64>(secret) + seed);
				uint64_t const input_hi = readLE<64>(input + len - 8) ^ (readLE<64>(secret + 8) - seed);
				uint64_t const acc = len + (input_lo + input_hi) + mul128fold64(input_lo, input_hi);
				return avalanche(acc);
			}
			else
			{
				uint64_t const input_lo = readLE<64>(input) ^ (readLE<64>(secret) + seed);
				uint64_t const input_hi = readLE<64>(input + len - 8) ^ (readLE<64>(secret + 8) - seed);
				hash128_t m128 = mul64to128(input_lo ^ input_hi, PRIME<64>(1));
				uint64_t const lenContrib = ((uint64_t)len * (uint64_t)PRIME<32>(5));
				m128.low64 += lenContrib;
				m128.high64 += input_hi * PRIME<64>(1);
				m128.low64 ^= (m128.high64 >> 32);
				hash128_t h128 = mul64to128(m128.low64, PRIME<64>(2));
				h128.high64 += m128.high64 * PRIME<64>(2);
				h128.low64 = avalanche(h128.low64);
				h128.high64 = avalanche(h128.high64);
				return h128;
			}
		}

		template <size_t N>
		hash_t<N> len_0to16(const uint8_t* input, size_t len, const uint8_t* secret, hash64_t seed)
		{
			if (len > 8)
			{
				return len_9to16<N>(input, len, secret, seed);
			}
			else if (len >= 4)
			{
				return len_4to8<N>(input, len, secret, seed);
			}
			else if (len)
			{
				return len_1to3<N>(input, len, secret, seed);
			}
			else
			{
				return hash_t<N>();
			}
		}

		template <size_t N>
		hash_t<N> hash_long_internal(const uint8_t* XXH_RESTRICT input, size_t len, const uint8_t* XXH_RESTRICT secret = default_secret, size_t secretSize = sizeof(default_secret))
		{
			alignas(acc_align) std::array<uint64_t, acc_nb> acc = init_acc;

			if constexpr (N == 64)
			{				
				hash_long_internal_loop(acc.data(), input, len, secret, secretSize, acc_width::acc_64bits);

				/* converge into final hash */
				return merge_accs(acc.data(), secret + secret_mergeaccs_start, (uint64_t)len * PRIME<64>(1));
			}
			else
			{
				hash_long_internal_loop(acc.data(), input, len, secret, secretSize, acc_width::acc_128bits);

				/* converge into final hash */
				uint64_t const low64 = merge_accs(acc.data(), secret + secret_mergeaccs_start, (uint64_t)len * PRIME<64>(1));
				uint64_t const high64 = merge_accs(acc.data(), secret + secretSize - sizeof(acc) - secret_mergeaccs_start, ~((uint64_t)len * PRIME<64>(2)));
				hash128_t const h128 = { low64, high64 };
				return h128;
			}
		}

		static uint64_t mix_16b(const uint8_t* XXH_RESTRICT input, const uint8_t* XXH_RESTRICT secret, hash64_t seed64)
		{
			uint64_t const input_lo = readLE<64>(input);
			uint64_t const input_hi = readLE<64>(input + 8);
			return mul128fold64(input_lo ^ (readLE<64>(secret) + seed64), input_hi ^ (readLE<64>(secret + 8) - seed64));
		}

		static uint128_t mix_32b(hash128_t acc, const uint8_t* input_1, const uint8_t* input_2, const uint8_t* secret, hash64_t seed)
		{
			acc.low64 += mix_16b(input_1, secret + 0, seed);
			acc.low64 ^= readLE<64>(input_2) + readLE<64>(input_2 + 8);
			acc.high64 += mix_16b(input_2, secret + 16, seed);
			acc.high64 ^= readLE<64>(input_1) + readLE<64>(input_1 + 8);
			return acc;
		}

		template <size_t N>
		hash_t<N> len_17to128(const uint8_t* XXH_RESTRICT input, size_t len, const uint8_t* XXH_RESTRICT secret, size_t secretSize, hash64_t seed)
		{
			if constexpr (N == 64)
			{
				uint64_t acc = len * PRIME<64>(1);

				if (len > 32) {
					if (len > 64) {
						if (len > 96) {
							acc += mix_16b(input + 48, secret + 96, seed);
							acc += mix_16b(input + len - 64, secret + 112, seed);
						}
						acc += mix_16b(input + 32, secret + 64, seed);
						acc += mix_16b(input + len - 48, secret + 80, seed);
					}
					acc += mix_16b(input + 16, secret + 32, seed);
					acc += mix_16b(input + len - 32, secret + 48, seed);
				}
				acc += mix_16b(input + 0, secret + 0, seed);
				acc += mix_16b(input + len - 16, secret + 16, seed);

				return avalanche(acc);
			}
			else
			{
				hash128_t acc;
				acc.low64 = len * PRIME<64>(1);
				acc.high64 = 0;
				if (len > 32) {
					if (len > 64) {
						if (len > 96) {
							acc = mix_32b(acc, input + 48, input + len - 64, secret + 96, seed);
						}
						acc = mix_32b(acc, input + 32, input + len - 48, secret + 64, seed);
					}
					acc = mix_32b(acc, input + 16, input + len - 32, secret + 32, seed);
				}
				acc = mix_32b(acc, input, input + len - 16, secret, seed);
				uint64_t const low64 = acc.low64 + acc.high64;
				uint64_t const high64 = (acc.low64 * PRIME<64>(1)) + (acc.high64 * PRIME<64>(4)) + ((len - seed) * PRIME<64>(2));
				hash128_t const h128 = { avalanche(low64), (hash64_t)0 - avalanche(high64) };
				return h128;
			}
		}

		template <size_t N>
		hash_t<N> len_129to240(const uint8_t* XXH_RESTRICT input, size_t len, const uint8_t* XXH_RESTRICT secret, size_t secretSize, hash64_t seed)
		{
			if constexpr (N == 64)
			{
				uint64_t acc = len * PRIME<64>(1);
				int const nbRounds = (int)len / 16;

				for (uint64_t i = 0; i < 8; i++) {
					acc += mix_16b(input + (i * 16), secret + (i * 16), seed);
				}

				acc = avalanche(acc);

				for (uint64_t i = 8; i < nbRounds; i++) {
					acc += mix_16b(input + (i * 16), secret + ((i - 8) * 16) + midsize_startoffset, seed);
				}

				/* last bytes */
				acc += mix_16b(input + len - 16, secret + secret_size_min - midsize_lastoffset, seed);
				return avalanche(acc);
			}
			else
			{
				hash128_t acc;
				int const nbRounds = (int)len / 32;
				acc.low64 = len * PRIME<64>(1);
				acc.high64 = 0;

				for (int i = 0; i < 4; i++) {
					acc = mix_32b(acc, input + (32 * i), input + (32 * i) + 16, secret + (32 * i), seed);
				}

				acc.low64 = avalanche(acc.low64);
				acc.high64 = avalanche(acc.high64);

				for (int i = 4; i < nbRounds; i++) {
					acc = mix_32b(acc, input + (32 * i), input + (32 * i) + 16, secret + midsize_startoffset + (32 * (i - 4)), seed);
				}

				/* last bytes */
				acc = mix_32b(acc, input + len - 16, input + len - 32, secret + secret_size_min - midsize_lastoffset - 16, 0ULL - seed);

				uint64_t const low64 = acc.low64 + acc.high64;
				uint64_t const high64 = (acc.low64 * PRIME<64>(1)) + (acc.high64 * PRIME<64>(4)) + ((len - seed) * PRIME<64>(2));
				hash128_t const h128 = { avalanche(low64), (hash64_t)0 - avalanche(high64) };
				return h128;
			}

		}

		template <size_t N>
		hash_t<N> hash_long(const uint8_t* input, size_t len, hash64_t seed, const uint8_t* XXH_RESTRICT secret, size_t secretSize)
		{
			alignas(8) uint8_t custom_secret[secret_default_size];

			if (seed == 0)
			{
				return hash_long_internal<N>(input, len, secret, secretSize);
			}
			else
			{
				init_custom_secret(custom_secret, seed);
				return hash_long_internal<N>(input, len, custom_secret, secret_default_size);
			}
		}

		template <size_t N>
		hash_t<N> xxhash3(const void* input, size_t len, hash64_t seed, const void* secret, size_t secretSize)
		{
			if (seed)
			{
				secret = default_secret;
				secretSize = sizeof(default_secret);
			}

			if (len <= 16)
			{
				return len_0to16<N>(static_cast<const uint8_t*>(input), len, static_cast<const uint8_t*>(secret), seed);
			}
			else if (len <= 128)
			{
				return len_17to128<N>(static_cast<const uint8_t*>(input), len, static_cast<const uint8_t*>(secret), secretSize, seed);
			}
			else if (len <= midsize_max)
			{
				return len_129to240<N>(static_cast<const uint8_t*>(input), len, static_cast<const uint8_t*>(secret), secretSize, seed);
			}
			else
			{
				return hash_long<N>(static_cast<const uint8_t*>(input), len, seed, static_cast<const uint8_t*>(secret), secretSize);
			}
		}

	}

	template <size_t N>
	hash_t<N> xxhash(const void* input, size_t len, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		return detail::endian_align<N>(input, len, seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(input));
	}

	template <size_t N, typename T>
	hash_t<N> xxhash(const std::basic_string<T>& input, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		return detail::endian_align<N>(static_cast<const void*>(input.data()), input.length() * sizeof(T), seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(static_cast<const void*>(input.data())));
	}

	template <size_t N, typename ContiguousIterator>
	hash_t<N> xxhash(ContiguousIterator begin, ContiguousIterator end, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		using T = typename std::decay_t<decltype(*end)>;
		return detail::endian_align<N>(static_cast<const void*>(&*begin), (end - begin) * sizeof(T), seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(static_cast<const void*>(&*begin)));
	}

	template <size_t N, typename T>
	hash_t<N> xxhash(const std::vector<T>& input, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		return detail::endian_align<N>(static_cast<const void*>(input.data()), input.size() * sizeof(T), seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(static_cast<const void*>(input.data())));
	}

	template <size_t N, typename T, size_t AN>
	hash_t<N> xxhash(const std::array<T, AN>& input, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		return detail::endian_align<N>(static_cast<const void*>(input.data()), AN * sizeof(T), seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(static_cast<const void*>(input.data())));
	}

	template <size_t N, typename T>
	hash_t<N> xxhash(const std::initializer_list<T>& input, hash_t<N> seed = 0, endianness endian = endianness::unspecified)
	{
		static_assert(!(N != 32 && N != 64), "You can only call xxhash in 32 or 64 bit mode.");
		return detail::endian_align<N>(static_cast<const void*>(input.begin()), input.size() * sizeof(T), seed, mem_ops::get_endian(endian), mem_ops::get_alignment<N>(static_cast<const void*>(input.begin())));
	}


	template <size_t N>
	hash_t<N> xxhash3(const void* input, size_t len, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		return detail3::xxhash3<N>(input, len, seed, secret, secretSize);
	}

	template <size_t N, typename T>
	hash_t<N> xxhash3(const std::basic_string<T>& input, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		return detail3::xxhash3<N>(static_cast<const void*>(input.data()), input.length() * sizeof(T), seed, secret, secretSize);
	}

	template <size_t N, typename ContiguousIterator>
	hash_t<N> xxhash3(ContiguousIterator begin, ContiguousIterator end, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		using T = typename std::decay_t<decltype(*end)>;
		return detail3::xxhash3<N>(static_cast<const void*>(&*begin), (end - begin) * sizeof(T), seed, secret, secretSize);
	}

	template <size_t N, typename T>
	hash_t<N> xxhash3(const std::vector<T>& input, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		return detail3::xxhash3<N>(static_cast<const void*>(input.data()), input.size() * sizeof(T), seed, secret, secretSize);
	}

	template <size_t N, typename T, size_t AN>
	hash_t<N> xxhash3(const std::array<T, AN>& input, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		return detail3::xxhash3<N>(static_cast<const void*>(input.data()), AN * sizeof(T), seed, secret, secretSize);
	}

	template <size_t N, typename T>
	hash_t<N> xxhash3(const std::initializer_list<T>& input, hash64_t seed = 0, const void* secret = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
	{
		static_assert(!(N != 128 && N != 64), "You can only call xxhash3 in 64 or 128 bit mode.");
		return detail3::xxhash3<N>(static_cast<const void*>(input.begin()), input.size() * sizeof(T), seed, secret, secretSize);
	}



	/* *******************************************************************
	*  Hash streaming
	*********************************************************************/
	enum class error_code : uint8_t { ok = 0, error };

	template <size_t N>
	class hash_state_t {

		uint64_t total_len = 0;
		hash_t<N> v1 = 0, v2 = 0, v3 = 0, v4 = 0;
		std::array<hash_t<N>, 4> mem = { { 0,0,0,0 } };
		uint32_t memsize = 0;

		inline error_code _update_impl(const void* input, size_t length, endianness endian)
		{
			const uint8_t* p = reinterpret_cast<const uint8_t*>(input);
			const uint8_t* const bEnd = p + length;

			if (!input) { return xxh::error_code::error; }

			total_len += length;

			if (memsize + length < (N / 2))
			{   /* fill in tmp buffer */
				memcpy(reinterpret_cast<uint8_t*>(mem.data()) + memsize, input, length);
				memsize += static_cast<uint32_t>(length);
				return error_code::ok;
			}

			if (memsize)
			{   /* some data left from previous update */
				memcpy(reinterpret_cast<uint8_t*>(mem.data()) + memsize, input, (N / 2) - memsize);

				const hash_t<N>* ptr = mem.data();
				v1 = detail::round<N>(v1, mem_ops::readLE<N>(ptr, endian)); ptr++;
				v2 = detail::round<N>(v2, mem_ops::readLE<N>(ptr, endian)); ptr++;
				v3 = detail::round<N>(v3, mem_ops::readLE<N>(ptr, endian)); ptr++;
				v4 = detail::round<N>(v4, mem_ops::readLE<N>(ptr, endian));

				p += (N / 2) - memsize;
				memsize = 0;
			}

			if (p <= bEnd - (N / 2))
			{
				const uint8_t* const limit = bEnd - (N / 2);

				do
				{
					v1 = detail::round<N>(v1, mem_ops::readLE<N>(p, endian)); p += (N / 8);
					v2 = detail::round<N>(v2, mem_ops::readLE<N>(p, endian)); p += (N / 8);
					v3 = detail::round<N>(v3, mem_ops::readLE<N>(p, endian)); p += (N / 8);
					v4 = detail::round<N>(v4, mem_ops::readLE<N>(p, endian)); p += (N / 8);
				} while (p <= limit);
			}

			if (p < bEnd)
			{
				memcpy(mem.data(), p, static_cast<size_t>(bEnd - p));
				memsize = static_cast<uint32_t>(bEnd - p);
			}

			return error_code::ok;
		}

		inline hash_t<N> _digest_impl(endianness endian) const
		{
			const uint8_t* p = reinterpret_cast<const uint8_t*>(mem.data());
			const uint8_t* const bEnd = reinterpret_cast<const uint8_t*>(mem.data()) + memsize;
			hash_t<N> hash_ret;

			if (total_len > (N / 2))
			{
				hash_ret = bit_ops::rotl<N>(v1, 1) + bit_ops::rotl<N>(v2, 7) + bit_ops::rotl<N>(v3, 12) + bit_ops::rotl<N>(v4, 18);

				detail::endian_align_sub_mergeround<N>(hash_ret, v1, v2, v3, v4);
			}
			else { hash_ret = v3 + detail::PRIME<N>(5); }

			hash_ret += static_cast<hash_t<N>>(total_len);

			return detail::endian_align_sub_ending<N>(hash_ret, p, bEnd, endian, alignment::unaligned);
		}

	public:
		hash_state_t(hash_t<N> seed = 0)
		{
			static_assert(!(N != 32 && N != 64), "You can only stream hashing in 32 or 64 bit mode.");
			v1 = seed + detail::PRIME<N>(1) + detail::PRIME<N>(2);
			v2 = seed + detail::PRIME<N>(2);
			v3 = seed + 0;
			v4 = seed - detail::PRIME<N>(1);
		};

		hash_state_t operator=(hash_state_t<N>& other)
		{
			memcpy(this, &other, sizeof(hash_state_t<N>));
		}

		error_code reset(hash_t<N> seed = 0)
		{
			memset(this, 0, sizeof(hash_state_t<N>));
			v1 = seed + detail::PRIME<N>(1) + detail::PRIME<N>(2);
			v2 = seed + detail::PRIME<N>(2);
			v3 = seed + 0;
			v4 = seed - detail::PRIME<N>(1);
			return error_code::ok;
		}

		error_code update(const void* input, size_t length, endianness endian = endianness::unspecified)
		{
			return _update_impl(input, length, mem_ops::get_endian(endian));
		}

		template <typename T>
		error_code update(const std::basic_string<T>& input, endianness endian = endianness::unspecified)
		{
			return _update_impl(static_cast<const void*>(input.data()), input.length() * sizeof(T), mem_ops::get_endian(endian));
		}

		template <typename ContiguousIterator>
		error_code update(ContiguousIterator begin, ContiguousIterator end, endianness endian = endianness::unspecified)
		{
			using T = typename std::decay_t<decltype(*end)>;
			return _update_impl(static_cast<const void*>(&*begin), (end - begin) * sizeof(T), mem_ops::get_endian(endian));
		}

		template <typename T>
		error_code update(const std::vector<T>& input, endianness endian = endianness::unspecified)
		{
			return _update_impl(static_cast<const void*>(input.data()), input.size() * sizeof(T), mem_ops::get_endian(endian));
		}

		template <typename T, size_t AN>
		error_code update(const std::array<T, AN>& input, endianness endian = endianness::unspecified)
		{
			return _update_impl(static_cast<const void*>(input.data()), AN * sizeof(T), mem_ops::get_endian(endian));
		}

		template <typename T>
		error_code update(const std::initializer_list<T>& input, endianness endian = endianness::unspecified)
		{
			return _update_impl(static_cast<const void*>(input.begin()), input.size() * sizeof(T), mem_ops::get_endian(endian));
		}

		hash_t<N> digest(endianness endian = endianness::unspecified)
		{
			return _digest_impl(mem_ops::get_endian(endian));
		}
	};

	using hash_state32_t = hash_state_t<32>;
	using hash_state64_t = hash_state_t<64>;

	template <size_t N>
	struct hash3_state_t 
	{   
		constexpr static int internal_buffer_size = 256;
		constexpr static int internal_buffer_stripes = (internal_buffer_size / detail3::stripe_len);
	
		alignas(64) hash_t<64> acc[8];
		alignas(64) uint8_t customSecret[detail3::secret_default_size];  /* used to store a custom secret generated from the seed. Makes state larger. Design might change */
		alignas(64) uint8_t buffer[internal_buffer_size];
		uint32_t bufferedSize = 0;
		uint32_t nbStripesPerBlock = 0;
		uint32_t nbStripesSoFar = 0;
		uint32_t secretLimit = 0;
		uint32_t reserved32 = 0;
		uint32_t reserved32_2 = 0;
		uint64_t totalLen = 0;
		uint64_t seed = 0;
		uint64_t reserved64 = 0;
		const uint8_t* secret = nullptr;    /* note : there is some padding after, due to alignment on 64 bytes */

		void consume_stripes(const uint8_t* input, detail3::acc_width accWidth)
		{
			if (nbStripesPerBlock - nbStripesSoFar <= internal_buffer_stripes) {
				/* need a scrambling operation */
				size_t const nbStripes = nbStripesPerBlock - nbStripesSoFar;
				detail3::accumulate(acc, input, secret + nbStripesSoFar * detail3::secret_consume_rate, nbStripes, accWidth);
				detail3::scramble_acc<detail3::vector_mode>(acc, secret + secretLimit);
				detail3::accumulate(acc, input + nbStripes * detail3::stripe_len, secret, internal_buffer_stripes - nbStripes, accWidth);
				nbStripesSoFar = (uint32_t)(internal_buffer_stripes - nbStripes);
			}
			else {
				detail3::accumulate(acc, input, secret + nbStripesSoFar * detail3::secret_consume_rate, internal_buffer_stripes, accWidth);
				nbStripesSoFar += (uint32_t)internal_buffer_stripes;
			}
		}

		error_code update_impl(const void* input_, size_t len)
		{
			detail3::acc_width accWidth = (N == 64) ? detail3::acc_width::acc_64bits : detail3::acc_width::acc_128bits;

			if (input_ == NULL)
			{
				return error_code::error;
			}

			const uint8_t* input = static_cast<const uint8_t*>(input_);
			const uint8_t* const bEnd = input + len;

			totalLen += len;

			if (bufferedSize + len <= internal_buffer_size) {  /* fill in tmp buffer */
				memcpy(buffer + bufferedSize, input, len);
				bufferedSize += (uint32_t)len;
				return error_code::ok;
			}
			/* input now > XXH3_INTERNALBUFFER_SIZE */


			if (bufferedSize) {   /* some input within internal buffer: fill then consume it */
				size_t const loadSize = internal_buffer_size - bufferedSize;
				memcpy(buffer + bufferedSize, input, loadSize);
				input += loadSize;
				consume_stripes(buffer, accWidth);
				bufferedSize = 0;
			}

			/* consume input by full buffer quantities */
			if (input + internal_buffer_size <= bEnd) {
				const uint8_t* const limit = bEnd - internal_buffer_size;
				do {
					consume_stripes(input, accWidth);
					input += internal_buffer_size;
				} while (input <= limit);
			}

			if (input < bEnd) { /* some remaining input input : buffer it */
				memcpy(buffer, input, (size_t)(bEnd - input));
				bufferedSize = (uint32_t)(bEnd - input);
			}


			return error_code::ok;
		}

		void digest_long(hash64_t* acc_, detail3::acc_width accWidth)
		{
			memcpy(acc_, acc, sizeof(acc));  /* digest locally, state remains unaltered, and can continue ingesting more input afterwards */
			if (bufferedSize >= detail3::stripe_len) {
				size_t const totalNbStripes = bufferedSize / detail3::stripe_len;
				uint32_t nbStripesSoFar = nbStripesSoFar;
				consume_stripes(buffer, accWidth);

				if (bufferedSize % detail3::stripe_len) {  /* one last partial stripe */
					detail3::accumulate_512<detail3::vector_mode>(acc, buffer + bufferedSize - detail3::stripe_len, secret + secretLimit - detail3::secret_lastacc_start, accWidth);
				}
			}
			else {  /* bufferedSize < STRIPE_LEN */
				if (bufferedSize) { /* one last stripe */
					uint8_t lastStripe[detail3::stripe_len];
					size_t const catchupSize = detail3::stripe_len - bufferedSize;
					memcpy(lastStripe, buffer + sizeof(buffer) - catchupSize, catchupSize);
					memcpy(lastStripe + catchupSize, buffer, bufferedSize);
					detail3::accumulate_512<detail3::vector_mode>(acc, lastStripe, secret + secretLimit - detail3::secret_lastacc_start, accWidth);
				}
			}
		}

	public:

		hash3_state_t operator=(hash3_state_t& other)
		{
			memcpy(this, &other, sizeof(hash3_state_t));
		}

		hash3_state_t(hash64_t seed_, const void* secret_ = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
		{
			reset(seed_, secret_, secretSize);
		}

		error_code reset(hash64_t seed_, const void* secret_ = detail3::default_secret, size_t secretSize = sizeof(detail3::default_secret))
		{ 
			memset(this, 0, sizeof(*this));
			memcpy(acc, detail3::init_acc.data(), sizeof(detail3::init_acc));
			seed = seed_;

			if (seed == 0)
			{
				secret = static_cast<const uint8_t*>(secret_);
			}
			else
			{
				detail3::init_custom_secret(customSecret, seed);
				secret = customSecret;
			}
			secretLimit = (uint32_t)(secretSize - detail3::stripe_len);
			nbStripesPerBlock = secretLimit / detail3::secret_consume_rate;

			return error_code::ok;
		}

		error_code update(const void* input, size_t len)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			return update_impl(static_cast<const void*>(input), len);
		}

		template <typename T>
		error_code update(const std::basic_string<T>& input)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			return update_impl(static_cast<const void*>(input.data()), input.length() * sizeof(T));
		}

		template <typename ContiguousIterator>
		error_code update(ContiguousIterator begin, ContiguousIterator end)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			using T = typename std::decay_t<decltype(*end)>;
			return update_impl(static_cast<const void*>(&*begin), (end - begin) * sizeof(T));
		}

		template <typename T>
		error_code update(const std::vector<T>& input)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			return update_impl(static_cast<const void*>(input.data()), input.size() * sizeof(T));
		}

		template <typename T, size_t AN>
		error_code update(const std::array<T, AN>& input)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			return update_impl(static_cast<const void*>(input.data()), AN * sizeof(T));
		}

		template <typename T>
		error_code update(const std::initializer_list<T>& input)
		{
			static_assert(!(N != 128 && N != 64), "You can only use xxhash3 streaming in 64 or 128 bit mode.");
			return update_impl(static_cast<const void*>(input.begin()), input.size() * sizeof(T));
		}

		hash_t<N> digest()
		{
			detail3::acc_width accWidth = (N == 64) ? detail3::acc_width::acc_64bits : detail3::acc_width::acc_128bits;

			if (totalLen > detail3::midsize_max) {
				alignas(detail3::acc_align) hash64_t acc[detail3::acc_nb];
				digest_long(acc, accWidth);

				if constexpr (N == 64)
				{
					return detail3::merge_accs(acc, secret + detail3::secret_mergeaccs_start, (uint64_t)totalLen * detail::PRIME<64>(1));
				}
				else
				{
					uint64_t const low64 = detail3::merge_accs(acc, secret + detail3::secret_mergeaccs_start, (uint64_t)totalLen * detail::PRIME<64>(1));
					uint64_t const high64 = detail3::merge_accs(acc, secret + secretLimit + detail3::stripe_len - sizeof(acc) - detail3::secret_mergeaccs_start, ~((uint64_t)totalLen * detail::PRIME<64>(2)));
					return  { low64, high64 };
				}
			}
			
			return detail3::xxhash3<N>(buffer, (size_t)totalLen, seed, secret, secretLimit + detail3::stripe_len);			
		}
	};

	using hash3_state64_t = hash3_state_t<64>;
	using hash3_state128_t = hash3_state_t<128>;

	/* *******************************************************************
	*  Canonical
	*********************************************************************/

	template <size_t N>
	struct canonical_t
	{
		std::array<uint8_t, N / 8> digest; 



		canonical_t(hash_t<N> hash)
		{
			if constexpr (N < 128)
			{
				if (mem_ops::is_little_endian()) 
				{ 
					hash = bit_ops::swap<N>(hash); 
				}
				memcpy(digest.data(), &hash, sizeof(canonical_t<N>));
			}
			else
			{
				if (mem_ops::is_little_endian()) 
				{ 
					hash.low64 = bit_ops::swap<64>(hash.low64); 
					hash.high64 = bit_ops::swap<64>(hash.high64);
				}
				memcpy(digest.data(), &hash.high64, sizeof(hash.high64));
				memcpy(digest.data() + sizeof(hash.high64), &hash.low64, sizeof(hash.low64));
			}
		}

		hash_t<N> get_hash() const
		{
			if constexpr (N < 128)
			{
				return mem_ops::readBE<N>(&digest);
			}
			else
			{
				hash128_t h;
				h.high64 = mem_ops::readBE<64>(&digest);
				h.low64 = mem_ops::readBE<64>(&digest[8]);
				return h;
			}
		}
	};

	using canonical32_t = canonical_t<32>;
	using canonical64_t = canonical_t<64>;
	using canonical128_t = canonical_t<128>;
}
