#include <array>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdlib.h>

/*
#ifdef __GNUC__
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void handler(int sig) {
	void* arr_[16];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(arr_, 16);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(arr_, size, STDERR_FILENO);
	exit(1);
}


#endif
*/

#define XXH_STATIC_LINKING_ONLY

//#define XXH_VECTOR 0
#include "xxhash.hpp"


//#include "xxhash.h"
//#undef XXH_VECTOR

#include "xxh3.h"

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

template <typename T1, typename T2>
bool cmp(T1 a, T2 b)
{
	return !(std::memcmp(&a, &b, sizeof(T1)));
}

template <typename T1, typename T2, typename = std::enable_if_t<sizeof(T1) == sizeof(T2)>>
bool cool_cmp(T1 a, T2 b)
{
	bool good = true;
	for (int i = 0; i < sizeof(T1); i++)
	{
		uint8_t v1 = *(static_cast<uint8_t*>(static_cast<void*>(&a)) + i);
		uint8_t v2 = *(static_cast<uint8_t*>(static_cast<void*>(&b)) + i);
		if (v1 != v2)
		{
			std::cout << "CMP Mismatch at " << i << ": " << byte_print(v1) << " | " << byte_print(v2) << std::endl;
			good = false;
		}
	}
	return good;
}



std::string pretty_time(uint64_t ns)
{
	std::string out = "";

	if (ns > 1000000000)
	{
		uint64_t sec = ns / 1000000000;
		ns %= 1000000000;
		out += std::to_string(sec) + "s ";
	}
	if (ns > 1000000)
	{
		uint64_t ms = ns / 1000000;
		ns %= 1000000;
		out += std::to_string(ms) + "ms ";
	}
	if (ns > 1000)
	{
		uint64_t us = ns / 1000;
		ns %= 1000;
		out += std::to_string(us) + "us ";
	}
	if (ns > 0)
	{
		out += std::to_string(ns) + "ns";
	}

	return out;
}

bool operator == (XXH128_hash_t h1, xxh::hash128_t h2)
{
	return h1.high64 == h2.high64 && h1.low64 == h2.low64;
}

int main(int argc, char** argv)
{

	int res = Catch::Session().run(argc, argv);

	/*
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

			std::cout << "C xxhash.h:\n* Total time: " << pretty_time(total_h) <<
				"\n* Mean: " << pretty_time(mean_h) <<
				"\n* Min: " << pretty_time(min_h) <<
				"\n* Max: " << pretty_time(max_h) <<
				"\n* Median: " << pretty_time(median_h) << "\n\n";

			std::cout << "C++ xxhash.hpp:\n* Total time: " << pretty_time(total_hpp) <<
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
	
	*/
	std::cin.get();
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
	
	REQUIRE(XXH128(alternating_data, 32, 0).high64 == xxh::xxhash3<128>(alternating_data, 32, 0).high64);
	REQUIRE(XXH128(alternating_data, 32, 0).low64 == xxh::xxhash3<128>(alternating_data, 32, 0).low64);
	REQUIRE(XXH128(zero_data, 32, 0).high64 == xxh::xxhash3<128>(zero_data, 32, 0).high64);
	REQUIRE(XXH128(zero_data, 32, 0).low64 == xxh::xxhash3<128>(zero_data, 32, 0).low64);
	REQUIRE(XXH128(one_data, 32, 0).high64 == xxh::xxhash3<128>(one_data, 32, 0).high64);
	REQUIRE(XXH128(one_data, 32, 0).low64 == xxh::xxhash3<128>(one_data, 32, 0).low64);
	REQUIRE(XXH128(alternating_data2, 32, 0).high64 == xxh::xxhash3<128>(alternating_data2, 32, 0).high64);
	REQUIRE(XXH128(alternating_data2, 32, 0).low64 == xxh::xxhash3<128>(alternating_data2, 32, 0).low64);

	REQUIRE(XXH3_64bits_withSeed(alternating_data, 32, 0) == xxh::xxhash3<64>(alternating_data, 32, 0));
	REQUIRE(XXH3_64bits_withSeed(zero_data, 32, 0) == xxh::xxhash3<64>(zero_data, 32, 0));
	REQUIRE(XXH3_64bits_withSeed(one_data, 32, 0) == xxh::xxhash3<64>(one_data, 32, 0));
	REQUIRE(XXH3_64bits_withSeed(alternating_data2, 32, 0) == xxh::xxhash3<64>(alternating_data2, 32, 0));
}

TEST_CASE("Results are the same as the original implementation for large, randomly generated inputs", "[compatibility]")
{
	constexpr int32_t test_num = 1024;
	//constexpr size_t test_buf_size = (1 << 16);

	std::minstd_rand rng(static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count()));
	std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

	

	for (size_t i = 0; i < test_num; i++)
	{
		size_t test_buf_size = i + 1;
		

		std::vector<uint32_t> input_buffer;	

		std::array<uint8_t, xxh::detail3::secret_size_min> secret_min_size;
		std::array<uint8_t, xxh::detail3::secret_default_size> secret_default_size;
		std::array<uint8_t, 256> secret_plus_size;

		input_buffer.resize(test_buf_size);
		std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });
		std::generate(secret_min_size.begin(), secret_min_size.end(), [&rng, &dist]() {return dist(rng); });
		std::generate(secret_default_size.begin(), secret_default_size.end(), [&rng, &dist]() {return dist(rng); });
		std::generate(secret_plus_size.begin(), secret_plus_size.end(), [&rng, &dist]() {return dist(rng); });

		uint32_t seed = dist(rng);

		xxh::hash_state32_t hash_state_32_cpp(seed);
		xxh::hash_state64_t hash_state_64_cpp(seed);
		XXH32_state_t* hash_state_32_c = XXH32_createState();
		XXH64_state_t* hash_state_64_c = XXH64_createState();

		XXH32_reset(hash_state_32_c, seed);
		XXH64_reset(hash_state_64_c, seed);

		

		xxh::hash3_state64_t hash3_state_64_cpp_seed(seed);
		xxh::hash3_state128_t hash3_state_128_cpp_seed(seed);
		XXH3_state_t* hash3_state_64_c_seed = XXH3_createState();
		XXH3_state_t* hash3_state_128_c_seed = XXH3_createState();

		XXH3_64bits_reset_withSeed(hash3_state_64_c_seed, seed);
		XXH3_128bits_reset_withSeed(hash3_state_128_c_seed, seed);

	

		xxh::hash3_state64_t hash3_state_64_cpp_secdef(0, secret_default_size.data());
		xxh::hash3_state128_t hash3_state_128_cpp_secdef(0, secret_default_size.data());
		XXH3_state_t* hash3_state_64_c_secdef = XXH3_createState();
		XXH3_state_t* hash3_state_128_c_secdef = XXH3_createState();

		XXH3_64bits_reset_withSecret(hash3_state_64_c_secdef, secret_default_size.data(), sizeof(secret_default_size));
		XXH3_128bits_reset_withSecret(hash3_state_128_c_secdef, secret_default_size.data(), sizeof(secret_default_size));

		

		xxh::hash3_state64_t hash3_state_64_cpp_secplus(0, secret_plus_size.data(), sizeof(secret_plus_size));
		xxh::hash3_state128_t hash3_state_128_cpp_secplus(0, secret_plus_size.data(), sizeof(secret_plus_size));
		XXH3_state_t* hash3_state_64_c_secplus = XXH3_createState();
		XXH3_state_t* hash3_state_128_c_secplus = XXH3_createState();

		XXH3_64bits_reset_withSecret(hash3_state_64_c_secplus, secret_plus_size.data(), sizeof(secret_plus_size));
		XXH3_128bits_reset_withSecret(hash3_state_128_c_secplus, secret_plus_size.data(), sizeof(secret_plus_size));

		

		xxh::hash3_state64_t hash3_state_64_cpp_secmin(0, secret_min_size.data(), sizeof(secret_min_size));
		xxh::hash3_state128_t hash3_state_128_cpp_secmin(0, secret_min_size.data(), sizeof(secret_min_size));
		XXH3_state_t* hash3_state_64_c_secmin = XXH3_createState();
		XXH3_state_t* hash3_state_128_c_secmin = XXH3_createState();

		XXH3_64bits_reset_withSecret(hash3_state_64_c_secmin, secret_min_size.data(), sizeof(secret_min_size));
		XXH3_128bits_reset_withSecret(hash3_state_128_c_secmin, secret_min_size.data(), sizeof(secret_min_size));

		

		hash_state_32_cpp.update(input_buffer);
		hash_state_64_cpp.update(input_buffer);

		XXH32_update(hash_state_32_c, input_buffer.data(), test_buf_size * sizeof(uint32_t));
		XXH64_update(hash_state_64_c, input_buffer.data(), test_buf_size * sizeof(uint32_t));


		hash3_state_64_cpp_seed.update(input_buffer);
		hash3_state_128_cpp_seed.update(input_buffer);

		XXH3_64bits_update(hash3_state_64_c_seed, input_buffer.data(), test_buf_size * sizeof(uint32_t));
		XXH3_128bits_update(hash3_state_128_c_seed, input_buffer.data(), test_buf_size * sizeof(uint32_t));

		hash3_state_64_cpp_secdef.update(input_buffer);
		hash3_state_128_cpp_secdef.update(input_buffer);

		XXH3_64bits_update(hash3_state_64_c_secdef, input_buffer.data(), test_buf_size * sizeof(uint32_t));
		XXH3_128bits_update(hash3_state_128_c_secdef, input_buffer.data(), test_buf_size * sizeof(uint32_t));

		hash3_state_64_cpp_secplus.update(input_buffer);
		hash3_state_128_cpp_secplus.update(input_buffer);

		XXH3_64bits_update(hash3_state_64_c_secplus, input_buffer.data(), test_buf_size * sizeof(uint32_t));
		XXH3_128bits_update(hash3_state_128_c_secplus, input_buffer.data(), test_buf_size * sizeof(uint32_t));

		hash3_state_64_cpp_secmin.update(input_buffer);
		hash3_state_128_cpp_secmin.update(input_buffer);

		XXH3_64bits_update(hash3_state_64_c_secmin, input_buffer.data(), test_buf_size * sizeof(uint32_t));
		XXH3_128bits_update(hash3_state_128_c_secmin, input_buffer.data(), test_buf_size * sizeof(uint32_t));

		

		xxh::canonical32_t canonical_32_cpp(xxh::xxhash<32>(input_buffer, seed));
		xxh::canonical64_t canonical_64_cpp(xxh::xxhash<64>(input_buffer, seed));

		XXH32_canonical_t canonical_32_c;
		XXH64_canonical_t canonical_64_c;

		XXH32_canonicalFromHash(&canonical_32_c, XXH32(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed));
		XXH64_canonicalFromHash(&canonical_64_c, XXH64(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed));


		xxh::canonical64_t canonical3_64_cpp_seed(xxh::xxhash3<64>(input_buffer, seed));
		xxh::canonical128_t canonical3_128_cpp_seed(xxh::xxhash3<128>(input_buffer, seed));

		XXH64_canonical_t canonical3_64_c_seed;
		XXH128_canonical_t canonical3_128_c_seed;

		XXH64_canonicalFromHash(&canonical3_64_c_seed, XXH3_64bits_withSeed(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed));
		XXH128_canonicalFromHash(&canonical3_128_c_seed, XXH3_128bits_withSeed(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed));


		xxh::canonical64_t canonical3_64_cpp_secdef(xxh::xxhash3<64>(input_buffer, 0, secret_default_size.data()));
		xxh::canonical128_t canonical3_128_cpp_secdef(xxh::xxhash3<128>(input_buffer, 0, secret_default_size.data()));

		XXH64_canonical_t canonical3_64_c_secdef;
		XXH128_canonical_t canonical3_128_c_secdef;

		XXH64_canonicalFromHash(&canonical3_64_c_secdef, XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_default_size.data(), secret_default_size.size()));
		XXH128_canonicalFromHash(&canonical3_128_c_secdef, XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_default_size.data(), secret_default_size.size()));


		xxh::canonical64_t canonical3_64_cpp_secplus(xxh::xxhash3<64>(input_buffer, 0, secret_plus_size.data(), secret_plus_size.size()));
		xxh::canonical128_t canonical3_128_cpp_secplus(xxh::xxhash3<128>(input_buffer, 0, secret_plus_size.data(), secret_plus_size.size()));

		XXH64_canonical_t canonical3_64_c_secplus;
		XXH128_canonical_t canonical3_128_c_secplus;

		XXH64_canonicalFromHash(&canonical3_64_c_secplus, XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_plus_size.data(), secret_plus_size.size()));
		XXH128_canonicalFromHash(&canonical3_128_c_secplus, XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_plus_size.data(), secret_plus_size.size()));


		xxh::canonical64_t canonical3_64_cpp_secmin(xxh::xxhash3<64>(input_buffer, 0, secret_min_size.data(), secret_min_size.size()));
		xxh::canonical128_t canonical3_128_cpp_secmin(xxh::xxhash3<128>(input_buffer, 0, secret_min_size.data(), secret_min_size.size()));

		XXH64_canonical_t canonical3_64_c_secmin;
		XXH128_canonical_t canonical3_128_c_secmin;

		XXH64_canonicalFromHash(&canonical3_64_c_secmin, XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_min_size.data(), secret_min_size.size()));
		XXH128_canonicalFromHash(&canonical3_128_c_secmin, XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_min_size.data(), secret_min_size.size()));

	


		
		REQUIRE(XXH32(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash<32>(input_buffer, seed));
		REQUIRE(XXH64(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash<64>(input_buffer, seed));
		/*
		std::array<uint8_t, xxh::detail3::secret_default_size> secret_c, secret_cpp;
		XXH3_initCustomSecret(secret_c.data(), seed);
		xxh::detail3::init_custom_secret(secret_cpp.data(), seed);

		//std::cout << "Comparing secrets | test_num = " << i << "\n";
		if (cool_cmp(secret_c, secret_cpp))
		{
		//	std::cout << "Secrets Identical.\n";
		}
		//uint64_t c = 

		XXH_ALIGN(XXH_ACC_ALIGN) std::array<xxh_u64, ACC_NB> acc_c = XXH3_INIT_ACC;
		alignas(xxh::detail3::acc_align) std::array<uint64_t, xxh::detail3::acc_nb> acc_cpp = xxh::detail3::init_acc;

		if (cool_cmp(acc_c, acc_cpp))
		{
		//	std::cout << "Initial accumulators identical.\n";
		}

		xxh::detail3::accumulate_512<xxh::vec_mode::scalar>(acc_cpp.data(), input_buffer.data(), secret_cpp.data(), xxh::detail3::acc_width::acc_128bits);
		XXH3_accumulate_512(acc_c.data(), input_buffer.data(), secret_c.data(), XXH3_accWidth_e::XXH3_acc_128bits);

		if (cool_cmp(acc_c, acc_cpp))
		{
		//	std::cout << "Accumulate 512 accumulators identical.\n";
		}

		acc_c = XXH3_INIT_ACC;
		acc_cpp = xxh::detail3::init_acc;

		size_t const nb_rounds = (xxh::detail3::secret_default_size - xxh::detail3::stripe_len) / xxh::detail3::secret_consume_rate;

		xxh::detail3::accumulate(acc_cpp.data(), (uint8_t*)(void*)input_buffer.data(), secret_cpp.data(), nb_rounds, xxh::detail3::acc_width::acc_128bits);
		XXH3_accumulate(acc_c.data(), (uint8_t*)(void*)input_buffer.data(), secret_c.data(), nb_rounds, XXH3_accWidth_e::XXH3_acc_128bits);

		if (cool_cmp(acc_c, acc_cpp))
		{
		//	std::cout << "Accumulate accumulators identical.\n";
		}


		acc_c = XXH3_INIT_ACC;
		acc_cpp = xxh::detail3::init_acc;

		xxh::detail3::scramble_acc<xxh::vec_mode::scalar>(acc_cpp.data(), secret_cpp.data());
		XXH3_scrambleAcc(acc_c.data(), secret_c.data());

		if (cool_cmp(acc_c, acc_cpp))
		{
		//	std::cout << "Scramble acc accumulators identical.\n";
		}

		//std::cin.get();


		acc_c = XXH3_INIT_ACC;
		acc_cpp = xxh::detail3::init_acc;

		xxh::detail3::hash_long_internal_loop(acc_cpp.data(), (uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), (uint8_t*)(void*)secret_cpp.data(), xxh::detail3::secret_default_size, xxh::detail3::acc_width::acc_128bits);
		XXH3_hashLong_internal_loop(acc_c.data(), (uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), (uint8_t*)(void*)secret_cpp.data(), xxh::detail3::secret_default_size, XXH3_accWidth_e::XXH3_acc_128bits);

		if (cool_cmp(acc_c, acc_cpp))
		{
		//	std::cout << "Hash long internal loop accumulators identical.\n";
		}

		XXH128_hash_t c_res = XXH3_hashLong_128b_internal((uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_c.data(), xxh::detail3::secret_default_size);
		xxh::uint128_t cpp_res = xxh::detail3::hash_long_internal<128>((uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_cpp.data());

		REQUIRE(c_res.high64 == cpp_res.high64 );
		REQUIRE(c_res.low64 == cpp_res.low64);

		REQUIRE(XXH3_hashLong_internal((uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_c.data(), xxh::detail3::secret_default_size) == xxh::detail3::hash_long_internal<64>((uint8_t*)(void*)input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_cpp.data()));
		*/

		XXH_ALIGN(XXH_ACC_ALIGN) std::array<xxh_u64, ACC_NB> acc_c = XXH3_INIT_ACC;
		alignas(xxh::detail3::acc_align) std::array<uint64_t, xxh::detail3::acc_nb> acc_cpp = xxh::detail3::init_acc;

		if (cool_cmp(acc_c, acc_cpp))
		{
			//	std::cout << "Initial accumulators identical.\n";
		}

		//xxh::detail3::accumulate_512<xxh::vec_mode::scalar>(acc_cpp.data(), input_buffer.data(), secret_cpp.data(), xxh::detail3::acc_width::acc_128bits);
		//XXH3_accumulate_512(acc_c.data(), input_buffer.data(), secret_c.data(), XXH3_accWidth_e::XXH3_acc_128bits);

		if (cool_cmp(acc_c, acc_cpp))
		{
			//	std::cout << "Accumulate 512 accumulators identical.\n";
		}

		REQUIRE(XXH3_64bits_withSeed(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash3<64>(input_buffer, seed));
		REQUIRE(XXH3_128bits_withSeed(input_buffer.data(), test_buf_size * sizeof(uint32_t), seed) == xxh::xxhash3<128>(input_buffer, seed));

		REQUIRE(XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_default_size.data(), secret_default_size.size()) == xxh::xxhash3<64>(input_buffer, 0, secret_default_size.data(), secret_default_size.size()));
		REQUIRE(XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_default_size.data(), secret_default_size.size()) == xxh::xxhash3<128>(input_buffer, 0, secret_default_size.data(), secret_default_size.size()));

		REQUIRE(XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_plus_size.data(), secret_plus_size.size()) == xxh::xxhash3<64>(input_buffer, 0, secret_plus_size.data(), secret_plus_size.size()));
		REQUIRE(XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_plus_size.data(), secret_plus_size.size()) == xxh::xxhash3<128>(input_buffer, 0, secret_plus_size.data(), secret_plus_size.size()));

		REQUIRE(XXH3_64bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_min_size.data(), secret_min_size.size()) == xxh::xxhash3<64>(input_buffer, 0, secret_min_size.data(), secret_min_size.size()));
		REQUIRE(XXH3_128bits_withSecret(input_buffer.data(), test_buf_size * sizeof(uint32_t), secret_min_size.data(), secret_min_size.size()) == xxh::xxhash3<128>(input_buffer, 0, secret_min_size.data(), secret_min_size.size()));

		
		

		REQUIRE(XXH32_digest(hash_state_32_c) == hash_state_32_cpp.digest());
		REQUIRE(XXH64_digest(hash_state_64_c) == hash_state_64_cpp.digest());

		REQUIRE(XXH3_64bits_digest(hash3_state_64_c_seed) == hash3_state_64_cpp_seed.digest());
		REQUIRE(XXH3_128bits_digest(hash3_state_128_c_seed) == hash3_state_128_cpp_seed.digest());

		REQUIRE(XXH3_64bits_digest(hash3_state_64_c_secdef) == hash3_state_64_cpp_secdef.digest());
		REQUIRE(XXH3_128bits_digest(hash3_state_128_c_secdef) == hash3_state_128_cpp_secdef.digest());

		

		REQUIRE(XXH3_64bits_digest(hash3_state_64_c_secplus) == hash3_state_64_cpp_secplus.digest());
		REQUIRE(XXH3_128bits_digest(hash3_state_128_c_secplus) == hash3_state_128_cpp_secplus.digest());

		REQUIRE(XXH3_64bits_digest(hash3_state_64_c_secmin) == hash3_state_64_cpp_secmin.digest());
		REQUIRE(XXH3_128bits_digest(hash3_state_128_c_secmin) == hash3_state_128_cpp_secmin.digest());

		

		REQUIRE(XXH32_hashFromCanonical(&canonical_32_c) == canonical_32_cpp.get_hash());
		REQUIRE(XXH64_hashFromCanonical(&canonical_64_c) == canonical_64_cpp.get_hash());

		REQUIRE(XXH64_hashFromCanonical(&canonical3_64_c_seed) == canonical3_64_cpp_seed.get_hash());
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_seed).high64 == canonical3_128_cpp_seed.get_hash().high64);
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_seed).low64 == canonical3_128_cpp_seed.get_hash().low64);

		REQUIRE(XXH64_hashFromCanonical(&canonical3_64_c_secdef) == canonical3_64_cpp_secdef.get_hash());
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secdef).high64 == canonical3_128_cpp_secdef.get_hash().high64);
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secdef).low64 == canonical3_128_cpp_secdef.get_hash().low64);

		REQUIRE(XXH64_hashFromCanonical(&canonical3_64_c_secplus) == canonical3_64_cpp_secplus.get_hash());
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secplus).high64 == canonical3_128_cpp_secplus.get_hash().high64);
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secplus).low64 == canonical3_128_cpp_secplus.get_hash().low64);

		REQUIRE(XXH64_hashFromCanonical(&canonical3_64_c_secmin) == canonical3_64_cpp_secmin.get_hash());
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secmin).high64 == canonical3_128_cpp_secmin.get_hash().high64);
		REQUIRE(XXH128_hashFromCanonical(&canonical3_128_c_secmin).low64 == canonical3_128_cpp_secmin.get_hash().low64);

		
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

	RAW_PRINT(xxh::bit_ops::rotl<32>(one, 4));
	RAW_PRINT(xxh::bit_ops::rotl<32>(last_byte, 4));
	RAW_PRINT(xxh::bit_ops::rotl<32>(first_byte, 4));
	RAW_PRINT(xxh::bit_ops::rotl<32>(cascade, 4));

	RAW_PRINT(xxh::bit_ops::rotl<32>(one, 16));
	RAW_PRINT(xxh::bit_ops::rotl<32>(last_byte, 16));
	RAW_PRINT(xxh::bit_ops::rotl<32>(first_byte, 16));
	RAW_PRINT(xxh::bit_ops::rotl<32>(cascade, 16));

	RAW_PRINT(xxh::bit_ops::rotl<64>(one64, 4));
	RAW_PRINT(xxh::bit_ops::rotl<64>(last_byte64, 4));
	RAW_PRINT(xxh::bit_ops::rotl<64>(first_byte64, 4));
	RAW_PRINT(xxh::bit_ops::rotl<64>(cascade64, 4));

	RAW_PRINT(xxh::bit_ops::rotl<64>(one64, 16));
	RAW_PRINT(xxh::bit_ops::rotl<64>(last_byte64, 16));
	RAW_PRINT(xxh::bit_ops::rotl<64>(first_byte64, 16));
	RAW_PRINT(xxh::bit_ops::rotl<64>(cascade64, 16));

	RAW_PRINT(xxh::bit_ops::swap<32>(one));
	RAW_PRINT(xxh::bit_ops::swap<64>(one64));

	RAW_PRINT(xxh::bit_ops::swap<32>(last_byte));
	RAW_PRINT(xxh::bit_ops::swap<64>(last_byte64));

	RAW_PRINT(xxh::bit_ops::swap<32>(first_byte));
	RAW_PRINT(xxh::bit_ops::swap<64>(first_byte64));

	RAW_PRINT(xxh::bit_ops::swap<32>(cascade));
	RAW_PRINT(xxh::bit_ops::swap<64>(cascade64));

	RAW_PRINT(xxh::xxhash<32>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(alternating_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(alternating_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<64>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<64>(alternating_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<128>(alternating_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<128>(alternating_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(zero_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(zero_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<64>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<64>(zero_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<128>(zero_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<128>(zero_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(one_data, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(one_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<64>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<64>(one_data, 32, 65536));
	RAW_PRINT(xxh::xxhash3<128>(one_data, 32, 0));
	RAW_PRINT(xxh::xxhash3<128>(one_data, 32, 65536));

	RAW_PRINT(xxh::xxhash<32>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash<64>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash<32>(alternating_data2, 32, 65536));
	RAW_PRINT(xxh::xxhash<64>(alternating_data2, 32, 65536));
	RAW_PRINT(xxh::xxhash3<64>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash3<64>(alternating_data2, 32, 65536));
	RAW_PRINT(xxh::xxhash3<128>(alternating_data2, 32, 0));
	RAW_PRINT(xxh::xxhash3<128>(alternating_data2, 32, 65536));
}
