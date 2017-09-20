#include "xxhash.hpp"
#include "xxhash_c_version.h"
#include <iostream>
#include <random>
#include <ctime>

using namespace xxh;

int main()
{
    const unsigned int dat[] = {1254, 54893,29385, 20310, 05231, 10234, 23412, 42510, 112 };
    const unsigned int* pdat = dat;
    const unsigned short dats[] = {1254, 0, 54893, 0,29385, 0, 20310, 0, 05231, 0, 10234, 0, 23412, 0, 42510, 0, 112, 0 };
    std::vector<unsigned short> dat2 = { 1254, 54893,29385, 20310, 05231, 10234, 23412, 42510, 112 };
    std::array<unsigned short, 9> dat3 = { 1254, 54893,29385, 20310, 05231, 10234, 23412, 42510, 112 };
    const char* strdat = "My favourite host, Segmentation fault (core dumped)!";
    std::string strdat2 = "My favourite host, Segmentation fault (core dumped)!";

    std::default_random_engine RNG (time(0));
    std::uniform_int_distribution<xxh::hash32_t> rand32(0, std::numeric_limits<xxh::hash32_t>::max());
    std::uniform_int_distribution<xxh::hash64_t> rand64(0, std::numeric_limits<xxh::hash64_t>::max()); 
    std::uniform_int_distribution<unsigned short> rand_endian(0, 1);



    std::cin.get();
}