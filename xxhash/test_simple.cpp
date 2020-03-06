#include <array>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdlib.h>

#define XXH_STATIC_LINKING_ONLY

#define XXH_VECTOR 2
#include "xxhash.hpp"

#include "xxh3.h"





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

#define REQUIRE(x) { \
	all++; \
	if (x) res++; \
	else std::cout << "Failed: (" << TO_STRING(x) << ")\n"; \
	}

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

	int res = 0, all = 0;

	constexpr int32_t test_num = 100;

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

	std::cout << "all: " << all << " | res: " << res << "\n";
	std::cin.get();

}


