#include <array>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdlib.h>

#include "xxhash.hpp"
#include "xxhash.h"


#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

template <typename T>
std::string byte_print(T val)
{
	constexpr std::array<char, 16> hex_digits = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	const uint8_t* inspect_ptr = reinterpret_cast<const uint8_t*>(&val); 
	std::string output = "";
	for (int i = 0; i < sizeof(T); i++)
	{
		output += std::string("0x") + hex_digits[(*inspect_ptr) >> 4] + hex_digits[((*inspect_ptr) & 0x0F)] + ' ';
		inspect_ptr++;
	}
	return output;
}

#define STRINGIFY(x) #x
#define TO_STRING(s) STRINGIFY(s)
#define RAW_PRINT(...) std::cout << std::left << std::setw(50) << TO_STRING(__VA_ARGS__) " == " << byte_print((__VA_ARGS__)) << "\n";



template <typename T>
bool cmp(T a, T b)
{
	return !(std::memcmp(&a, &b, sizeof(T)));
}

std::string pretty_time(uint64_t ns)
{
	std::string out = "";

	if (ns > 1000000000)
	{
		int32_t sec = ns / 1000000000;
		ns %= 1000000000;
		out += std::to_string(sec) + "s ";
	}
	if (ns > 1000000)
	{
		int32_t ms = ns / 1000000;
		ns %= 1000000;
		out += std::to_string(ms) + "ms ";
	}
	if (ns > 1000)
	{
		int32_t us = ns / 1000;
		ns %= 1000;
		out += std::to_string(us) + "us ";
	}
	if (ns > 0)
	{
		out += std::to_string(ns) + "ns";
	}

	return out;
}

int main(int argc, char** argv)
{
	int res = Catch::Session().run(argc, argv);

	
	std::cout << "Naive Benchmark\n\n";

	{
		std::cout << "\n=== 64BIT: ===\n\n";;

		constexpr int32_t base_num = (1 << 12);
		constexpr int32_t base_buf_size = (1 << 16);
		constexpr int32_t adv_steps = 8;

		int32_t test_buf_size = base_buf_size;
		int32_t test_num = base_num;

		for (int adv_step = 0; adv_step < adv_steps; adv_step++)
		{
			std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
			std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

			std::array<uint64_t, base_num> bench_time_h, bench_time_hpp;

			std::vector<uint32_t> input_buffer;
			input_buffer.resize(test_buf_size);

			for (int i = 0; i < test_num; i++)
			{

				std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });
				uint64_t seed = ((static_cast<uint64_t>(dist(rng)) << 32) + dist(rng));

				// Each test is ran four time, first with the C version, second with the C++ version, third again with the C version, and fourth with the C++ version. This is done to account for any possible slowdowns caused by cache misses.

				// warmup
				auto tp_h_1 = std::chrono::system_clock::now();
				uint64_t res_h = XXH64(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed);
				auto tp_h_2 = std::chrono::system_clock::now();

				auto tp_hpp_1 = std::chrono::system_clock::now();
				uint64_t res_hpp = xxh::xxhash<64>(input_buffer, seed);
				auto tp_hpp_2 = std::chrono::system_clock::now();

				// reality
				tp_h_1 = std::chrono::system_clock::now();
				res_h = XXH64(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed);
				tp_h_2 = std::chrono::system_clock::now();

				bench_time_h[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_h_2 - tp_h_1).count();

				tp_hpp_1 = std::chrono::system_clock::now();
				res_hpp = xxh::xxhash<64>(input_buffer, seed);
				tp_hpp_2 = std::chrono::system_clock::now();

				bench_time_hpp[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_hpp_2 - tp_hpp_1).count();

				if (res_h != res_hpp)
				{
					std::cout << "ERROR: adv_step: " << adv_step << " i: " << i << " | res_h: " << res_h << " | res_hpp: " << res_hpp << "\n";
				}
			}

			uint64_t min_h = *std::min_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t min_hpp = *std::min_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t max_h = *std::max_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t max_hpp = *std::max_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t total_h = std::accumulate(bench_time_h.begin(), bench_time_h.begin() + test_num, 0ull);
			uint64_t total_hpp = std::accumulate(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num, 0ull);

			uint64_t mean_h = total_h / test_num;
			uint64_t mean_hpp = total_hpp / test_num;

			std::sort(bench_time_h.begin(), bench_time_h.begin() + test_num);
			std::sort(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t median_h = bench_time_h[test_num / 2];
			uint64_t median_hpp = bench_time_hpp[test_num / 2];

			std::cout << "Tests ran: " << test_num << "\nInput buffer size: " << test_buf_size * sizeof(uint32_t) << "\n\n";

			std::cout << "meow_hash.h:\n* Total time: " << pretty_time(total_h) <<
				"\n* Mean: " << pretty_time(mean_h) <<
				"\n* Min: " << pretty_time(min_h) <<
				"\n* Max: " << pretty_time(max_h) <<
				"\n* Median: " << pretty_time(median_h) << "\n\n";

			std::cout << "meow_hash.hpp:\n* Total time: " << pretty_time(total_hpp) <<
				"\n* Mean: " << pretty_time(mean_hpp) <<
				"\n* Min: " << pretty_time(min_hpp) <<
				"\n* Max: " << pretty_time(max_hpp) <<
				"\n* Median: " << pretty_time(median_hpp) << "\n\n";

			std::cout << "Advantage of the C version: " <<
				"\n* Mean: " << 100 * (1 - (static_cast<double>(mean_h) / static_cast<double>(mean_hpp))) << "%"
				"\n* Min: " << 100 * (1 - (static_cast<double>(min_h) / static_cast<double>(min_hpp))) << "%"
				"\n* Max: " << 100 * (1 - (static_cast<double>(max_h) / static_cast<double>(max_hpp))) << "%"
				"\n* Median: " << 100 * (1 - (static_cast<double>(median_h) / static_cast<double>(median_hpp))) << "%\n\n\n";
				
			test_num /= 2;
			test_buf_size *= 2;
		}
	}


	{
		std::cout << "\n=== 32BIT: ===\n\n";;

		constexpr int32_t base_num = (1 << 12);
		constexpr int32_t base_buf_size = (1 << 16);
		constexpr int32_t adv_steps = 8;

		int32_t test_buf_size = base_buf_size;
		int32_t test_num = base_num;

		for (int adv_step = 0; adv_step < adv_steps; adv_step++)
		{
			std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
			std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

			std::array<uint64_t, base_num> bench_time_h, bench_time_hpp;

			std::vector<uint32_t> input_buffer;
			input_buffer.resize(test_buf_size);

			for (int i = 0; i < test_num; i++)
			{

				std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });
				uint32_t seed = dist(rng);

				// Each test is ran four time, first with the C version, second with the C++ version, third again with the C version, and fourth with the C++ version. This is done to account for any possible slowdowns caused by cache misses.

				// warmup
				auto tp_h_1 = std::chrono::system_clock::now();
				uint32_t res_h = XXH32(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed);
				auto tp_h_2 = std::chrono::system_clock::now();

				auto tp_hpp_1 = std::chrono::system_clock::now();
				uint32_t res_hpp = xxh::xxhash<32>(input_buffer, seed);
				auto tp_hpp_2 = std::chrono::system_clock::now();

				// reality
				tp_h_1 = std::chrono::system_clock::now();
				res_h = XXH32(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed);
				tp_h_2 = std::chrono::system_clock::now();

				bench_time_h[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_h_2 - tp_h_1).count();

				tp_hpp_1 = std::chrono::system_clock::now();
				res_hpp = xxh::xxhash<32>(input_buffer, seed);
				tp_hpp_2 = std::chrono::system_clock::now();

				bench_time_hpp[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_hpp_2 - tp_hpp_1).count();

				if (res_h != res_hpp)
				{
					std::cout << "ERROR: adv_step: " << adv_step << " i: " << i << " | res_h: " << res_h << " | res_hpp: " << res_hpp << "\n";
				}
			}

			uint64_t min_h = *std::min_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t min_hpp = *std::min_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t max_h = *std::max_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t max_hpp = *std::max_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t total_h = std::accumulate(bench_time_h.begin(), bench_time_h.begin() + test_num, 0ull);
			uint64_t total_hpp = std::accumulate(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num, 0ull);

			uint64_t mean_h = total_h / test_num;
			uint64_t mean_hpp = total_hpp / test_num;

			std::sort(bench_time_h.begin(), bench_time_h.begin() + test_num);
			std::sort(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t median_h = bench_time_h[test_num / 2];
			uint64_t median_hpp = bench_time_hpp[test_num / 2];
			
			std::cout << "Tests ran: " << test_num << "\nInput buffer size: " << test_buf_size * sizeof(uint32_t) << "\n\n";

			std::cout << "meow_hash.h:\n* Total time: " << pretty_time(total_h) <<
				"\n* Mean: " << pretty_time(mean_h) <<
				"\n* Min: " << pretty_time(min_h) <<
				"\n* Max: " << pretty_time(max_h) <<
				"\n* Median: " << pretty_time(median_h) << "\n\n";

			std::cout << "meow_hash.hpp:\n* Total time: " << pretty_time(total_hpp) <<
				"\n* Mean: " << pretty_time(mean_hpp) <<
				"\n* Min: " << pretty_time(min_hpp) <<
				"\n* Max: " << pretty_time(max_hpp) <<
				"\n* Median: " << pretty_time(median_hpp) << "\n\n";

			std::cout << "Advantage of the C version: " <<
				"\n* Mean: " << 100 * (1 - (static_cast<double>(mean_h) / static_cast<double>(mean_hpp))) << "%"
				"\n* Min: " << 100 * (1 - (static_cast<double>(min_h) / static_cast<double>(min_hpp))) << "%"
				"\n* Max: " << 100 * (1 - (static_cast<double>(max_h) / static_cast<double>(max_hpp))) << "%"
				"\n* Median: " << 100 * (1 - (static_cast<double>(median_h) / static_cast<double>(median_hpp))) << "%\n\n\n";
				
			test_num /= 2;
			test_buf_size *= 2;
		}
	}

	return res;
}

TEST_CASE("Results are the same as the original implementation for small inputs", "[compatibility]")
{
	uint32_t alternating_data[] = { 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF };
	uint32_t zero_data[] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
	uint32_t one_data[] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	uint32_t alternating_data2[] = { 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA };

	REQUIRE(XXH64(alternating_data, 32, 0) == xxh::xxhash<64>(alternating_data, 32, 0));
	REQUIRE(XXH64(zero_data, 32, 0) == xxh::xxhash<64>(zero_data, 32, 0));
	REQUIRE(XXH64(one_data, 32, 0) == xxh::xxhash<64>(one_data, 32, 0));
	REQUIRE(XXH64(alternating_data2, 32, 0) == xxh::xxhash<64>(alternating_data2, 32, 0));

	REQUIRE(XXH32(alternating_data, 32, 0) == xxh::xxhash<32>(alternating_data, 32, 0));
	REQUIRE(XXH32(zero_data, 32, 0) == xxh::xxhash<32>(zero_data, 32, 0));
	REQUIRE(XXH32(one_data, 32, 0) == xxh::xxhash<32>(one_data, 32, 0));
	REQUIRE(XXH32(alternating_data2, 32, 0) == xxh::xxhash<32>(alternating_data2, 32, 0));
	
}

TEST_CASE("Results are the same as the original implementation for large, randomly generated inputs", "[compatibility]")
{
	constexpr int32_t test_num = 64;
	constexpr int32_t test_buf_size = (1 << 20);

	std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

	for (int i = 0; i < test_num; i++)
	{
		std::vector<uint32_t> input_buffer;

		input_buffer.resize(test_buf_size);
		std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });

		uint64_t seed = ((static_cast<uint64_t>(dist(rng)) << 32) + dist(rng));

		REQUIRE(XXH64(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash<64>(input_buffer, seed));
		REQUIRE(XXH32(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash<32>(input_buffer, seed));
	}
}

TEST_CASE("Printing results for inter-platform comparison", "[platform]")
{
	uint32_t alternating_data[] = { 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF };
	uint32_t zero_data[] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
	uint32_t one_data[] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	uint32_t alternating_data2[] = { 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA };

	constexpr uint32_t zero = 0x00000000;
	constexpr uint64_t zero64 = 0x0000000000000000;

	constexpr uint32_t one = 0x00000001;
	constexpr uint64_t one64 = 0x0000000000000001;

	constexpr uint32_t last_byte = 0x000000FF;
	constexpr uint64_t last_byte64 = 0x00000000000000FF;

	constexpr uint32_t first_byte = 0xFF000000;
	constexpr uint64_t first_byte64 = 0xFF00000000000000;

	constexpr uint32_t cascade = 0x01234567;
	constexpr uint64_t cascade64 = 0x0123456789ABCDEF;

	RAW_PRINT(xxh::mem_ops::read32(&zero));
	RAW_PRINT(xxh::mem_ops::read64(&zero64));

	RAW_PRINT(xxh::mem_ops::read32(&one));
	RAW_PRINT(xxh::mem_ops::read64(&one64));

	RAW_PRINT(xxh::mem_ops::read32(&last_byte));
	RAW_PRINT(xxh::mem_ops::read64(&last_byte64));

	RAW_PRINT(xxh::mem_ops::read32(&first_byte));
	RAW_PRINT(xxh::mem_ops::read64(&first_byte64));

	RAW_PRINT(xxh::bit_ops::rotl32(one, 4));
	RAW_PRINT(xxh::bit_ops::rotl32(last_byte, 4));
	RAW_PRINT(xxh::bit_ops::rotl32(first_byte, 4));
	RAW_PRINT(xxh::bit_ops::rotl32(cascade, 4));

	RAW_PRINT(xxh::bit_ops::rotl32(one, 16));
	RAW_PRINT(xxh::bit_ops::rotl32(last_byte, 16));
	RAW_PRINT(xxh::bit_ops::rotl32(first_byte, 16));
	RAW_PRINT(xxh::bit_ops::rotl32(cascade, 16));

	RAW_PRINT(xxh::bit_ops::rotl64(one64, 4));
	RAW_PRINT(xxh::bit_ops::rotl64(last_byte64, 4));
	RAW_PRINT(xxh::bit_ops::rotl64(first_byte64, 4));
	RAW_PRINT(xxh::bit_ops::rotl64(cascade64, 4));

	RAW_PRINT(xxh::bit_ops::rotl64(one64, 16));
	RAW_PRINT(xxh::bit_ops::rotl64(last_byte64, 16));
	RAW_PRINT(xxh::bit_ops::rotl64(first_byte64, 16));
	RAW_PRINT(xxh::bit_ops::rotl64(cascade64, 16));

	RAW_PRINT(xxh::bit_ops::swap32(one));
	RAW_PRINT(xxh::bit_ops::swap64(one64));

	RAW_PRINT(xxh::bit_ops::swap32(last_byte));
	RAW_PRINT(xxh::bit_ops::swap64(last_byte64));

	RAW_PRINT(xxh::bit_ops::swap32(first_byte));
	RAW_PRINT(xxh::bit_ops::swap64(first_byte64));

	RAW_PRINT(xxh::bit_ops::swap32(cascade));
	RAW_PRINT(xxh::bit_ops::swap64(cascade64));

	RAW_PRINT(xxh::xxhash<32>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(alternating_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(alternating_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(zero_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(zero_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(one_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(one_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(alternating_data2, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(alternating_data2, 32, 65536));
}