#pragma once
#include "common/logging/log.h"
#include <cmath>

// Used for debugging

struct BinarySearch
{
    size_t min;
    size_t mid;
    size_t max;
    BinarySearch(size_t max) : min(0), mid(max / 2), max(max) { }
    BinarySearch(size_t min, size_t max) : min(min), mid((min + max) / 2), max(max) { }
    BinarySearch l() const { return BinarySearch(min, mid); }
    BinarySearch r() const { return BinarySearch(mid, max); }
    operator size_t()
    {
        LOG_DEBUG(BinaryTranslator, "BinarySearch: %x: %x - %x (%x, %d)", mid, max, min, max - min, (size_t)std::log2(max - min));
        return mid;
    }
    operator int() { return static_cast<size_t>(*this); }
};