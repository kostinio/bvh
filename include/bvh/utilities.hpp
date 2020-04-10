#ifndef BVH_UTILITIES_HPP
#define BVH_UTILITIES_HPP

#include <cstring>
#include <cstdint>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>

#include "bvh/bounding_box.hpp"

namespace bvh {

/// Safe function to reinterpret the bits of the given value as another type.
template <typename To, typename From>
To as(From from) {
    To to;
    std::memcpy(&to, &from, sizeof(from));
    return to;
}

/// Equivalent to copysign(x, x * y).
inline float product_sign(float x, float y) {
    return as<float>(as<uint32_t>(x) ^ (as<uint32_t>(y) & UINT32_C(0x80000000)));
}

/// Equivalent to copysign(x, x * y).
inline double product_sign(double x, double y) {
    return as<double>(as<uint64_t>(x) ^ (as<uint64_t>(y) & UINT64_C(0x8000000000000000)));
}

inline float multiply_add(float x, float y, float z) {
#ifdef FP_FAST_FMAF
    return std::fmaf(x, y, z);
#else
    return x * y + z;
#endif
}

inline double multiply_add(double x, double y, double z) {
#ifdef FP_FAST_FMA
    return std::fma(x, y, z);
#else
    return x * y + z;
#endif
}

template <typename Scalar>
void atomic_max(std::atomic<Scalar>& x, Scalar y) {
    auto z = x.load();
    while (z < y && !x.compare_exchange_weak(z, y)) ;
}

/// Shuffles primitives such that the primitive at index i is `primitives[indices[i]]`.
template <typename Primitive>
void shuffle_primitives(Primitive* primitives, const size_t* indices, size_t primitive_count) {
    auto primitives_copy = std::make_unique<Primitive[]>(primitive_count);
    std::move(primitives, primitives + primitive_count, primitives_copy.get());
    for (size_t i = 0; i < primitive_count; ++i)
        primitives[i] = std::move(primitives_copy[indices[i]]);
}

template <typename Primitive, typename Scalar = typename Primitive::ScalarType>
std::pair<std::unique_ptr<BoundingBox<Scalar>[]>, std::unique_ptr<Vector3<Scalar>[]>>
compute_bounding_boxes_and_centers(const Primitive* primitives, size_t primitive_count) {
    auto bboxes  = std::make_unique<BoundingBox<Scalar>[]>(primitive_count);
    auto centers = std::make_unique<Vector3<Scalar>[]>(primitive_count);
    #pragma omp parallel for
    for (size_t i = 0; i < primitive_count; ++i) {
        bboxes[i]  = primitives[i].bounding_box();
        centers[i] = primitives[i].center();
    }
    return std::make_pair(std::move(bboxes), std::move(centers));
}

/// Template that is used to select the appropriate index
/// type of size equivalent to the given type.
template <typename T>
struct SimilarlySizedIndex {};

template <> struct SimilarlySizedIndex<float>  { using IndexType = uint32_t; };
template <> struct SimilarlySizedIndex<double> { using IndexType = uint64_t; };

/// Computes the (rounded-up) compile-time log in base-2 of an unsigned integer.
template <size_t P, size_t I = 0, bool Found = P == 0>
struct RoundedUpLog2 {};

template <size_t P, size_t I>
struct RoundedUpLog2<P, I, false> {
    static constexpr size_t value = RoundedUpLog2<P, I + 1, (1 << (I + 1)) >= P>::value;
};

template <size_t P, size_t I>
struct RoundedUpLog2<P, I, true> {
    static constexpr size_t value = I;
};

} // namespace bvh

#endif