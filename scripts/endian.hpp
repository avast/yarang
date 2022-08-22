#pragma once

#include <cstdint>
#include <type_traits>

#include <byteswap.h>

#if defined(__LITTLE_ENDIAN__)
	#define CCOOL_LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN__)
	#define CCOOL_BIG_ENDIAN
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define CCOOL_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#define CCOOL_BIG_ENDIAN
#else
	#error "Can't determine endianness"
#endif

enum class Endian
{
	Little = 0,
	Big = 1,
#ifdef CCOOL_LITTLE_ENDIAN
	Native = Little
#elif CCOOL_BIG_ENDIAN
	Native = Big
#endif
};

namespace detail {

// Template for integer types of at least Width bytes in size
template <std::size_t Width> struct IntLeast {};
template <> struct IntLeast<1> { using Type = std::uint8_t; };
template <> struct IntLeast<2> { using Type = std::uint16_t; };
template <> struct IntLeast<4> { using Type = std::uint32_t; };
template <> struct IntLeast<8> { using Type = std::uint64_t; };

// Actually swapping or just identity for 8-bit values
template <std::size_t Width> struct EndianConvertSwap { static auto convert(typename IntLeast<Width>::Type val) { return val; } };
#define ENDIAN_CONVERT_SWAP(size, swap_func) template <> struct EndianConvertSwap<size> { static auto convert(typename IntLeast<size>::Type val) { return swap_func(val); } }
ENDIAN_CONVERT_SWAP(2, bswap_16);
ENDIAN_CONVERT_SWAP(4, bswap_32);
ENDIAN_CONVERT_SWAP(8, bswap_64);

// When converting from little-to-little or big-to-big
template <Endian From, Endian To, typename T>
struct EndianConvertHelper { static T convert(T val) { return val; } };

// When converting from little-to-big
template <typename T>
struct EndianConvertHelper<Endian::Little, Endian::Big, T> { static T convert(T val) { return EndianConvertSwap<sizeof(T)>::convert(val); } };

// When converting from big-to-little
template <typename T>
struct EndianConvertHelper<Endian::Big, Endian::Little, T> { static T convert(T val) { return EndianConvertSwap<sizeof(T)>::convert(val); } };

// Ultimate conversion helper
template <Endian From, Endian To, typename T, typename = std::enable_if_t<std::is_integral_v<T>, void>>
struct EndianConvert { static T convert(T val) { return EndianConvertHelper<From, To, T>::convert(val); } };

} // namespace detail

template <Endian From, Endian To, typename T>
auto endian_convert(T val)
{
	return detail::EndianConvert<From, To, T>::convert(val);
}
