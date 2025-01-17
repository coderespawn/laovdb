// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

/// @file points/PointCount.h
///
/// @author Dan Bailey
///
/// @brief  Methods for counting points in VDB Point grids.

#ifndef OPENVDB_POINTS_POINT_COUNT_HAS_BEEN_INCLUDED
#define OPENVDB_POINTS_POINT_COUNT_HAS_BEEN_INCLUDED

#include <openvdb/openvdb.h>

#include "PointDataGrid.h"
#include "PointMask.h"
#include "IndexFilter.h"

#include <tbb/parallel_reduce.h>

#include <vector>


namespace laovdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {
namespace points {


/// @brief Count the total number of points in a PointDataTree
/// @param tree         the PointDataTree in which to count the points
/// @param filter       an optional index filter
/// @param inCoreOnly   if true, points in out-of-core leaf nodes are not counted
/// @param threaded     enable or disable threading  (threading is enabled by default)
template <typename PointDataTreeT, typename FilterT = NullFilter>
inline Index64 pointCount(  const PointDataTreeT& tree,
                            const FilterT& filter = NullFilter(),
                            const bool inCoreOnly = false,
                            const bool threaded = true);


/// @brief Populate an array of cumulative point offsets per leaf node.
/// @param pointOffsets     array of offsets to be populated
/// @param tree             the PointDataTree from which to populate the offsets
/// @param filter           an optional index filter
/// @param inCoreOnly       if true, points in out-of-core leaf nodes are ignored
/// @param threaded         enable or disable threading  (threading is enabled by default)
/// @return The final cumulative point offset.
template <typename PointDataTreeT, typename FilterT = NullFilter>
inline Index64 pointOffsets(std::vector<Index64>& pointOffsets,
                            const PointDataTreeT& tree,
                            const FilterT& filter = NullFilter(),
                            const bool inCoreOnly = false,
                            const bool threaded = true);


/// @brief Generate a new grid with voxel values to store the number of points per voxel
/// @param grid             the PointDataGrid to use to compute the count grid
/// @param filter           an optional index filter
/// @note The return type of the grid must be an integer or floating-point scalar grid.
template <typename PointDataGridT,
    typename GridT = typename PointDataGridT::template ValueConverter<Int32>::Type,
    typename FilterT = NullFilter>
inline typename GridT::Ptr
pointCountGrid( const PointDataGridT& grid,
                const FilterT& filter = NullFilter());


/// @brief Generate a new grid that uses the supplied transform with voxel values to store the
///        number of points per voxel.
/// @param grid             the PointDataGrid to use to compute the count grid
/// @param transform        the transform to use to compute the count grid
/// @param filter           an optional index filter
/// @note The return type of the grid must be an integer or floating-point scalar grid.
template <typename PointDataGridT,
    typename GridT = typename PointDataGridT::template ValueConverter<Int32>::Type,
    typename FilterT = NullFilter>
inline typename GridT::Ptr
pointCountGrid( const PointDataGridT& grid,
                const laovdb::math::Transform& transform,
                const FilterT& filter = NullFilter());


////////////////////////////////////////


template <typename PointDataTreeT, typename FilterT>
Index64 pointCount(const PointDataTreeT& tree,
                   const FilterT& filter,
                   const bool inCoreOnly,
                   const bool threaded)
{
    using LeafManagerT = tree::LeafManager<const PointDataTreeT>;
    using LeafRangeT = typename LeafManagerT::LeafRange;

    auto countLambda =
        [&filter, &inCoreOnly] (const LeafRangeT& range, Index64 sum) -> Index64 {
            for (const auto& leaf : range) {
                if (inCoreOnly && leaf.buffer().isOutOfCore())  continue;
                auto state = filter.state(leaf);
                if (state == index::ALL) {
                    sum += leaf.pointCount();
                } else if (state != index::NONE) {
                    sum += iterCount(leaf.beginIndexAll(filter));
                }
            }
            return sum;
        };

    LeafManagerT leafManager(tree);
    if (threaded) {
        return tbb::parallel_reduce(leafManager.leafRange(), Index64(0), countLambda,
            [] (Index64 n, Index64 m) -> Index64 { return n + m; });
    }
    else {
        return countLambda(leafManager.leafRange(), Index64(0));
    }
}


template <typename PointDataTreeT, typename FilterT>
Index64 pointOffsets(   std::vector<Index64>& pointOffsets,
                        const PointDataTreeT& tree,
                        const FilterT& filter,
                        const bool inCoreOnly,
                        const bool threaded)
{
    using LeafT = typename PointDataTreeT::LeafNodeType;
    using LeafManagerT = typename tree::LeafManager<const PointDataTreeT>;

    // allocate and zero values in point offsets array

    pointOffsets.assign(tree.leafCount(), Index64(0));
    if (pointOffsets.empty()) return 0;

    // compute total points per-leaf

    LeafManagerT leafManager(tree);
    leafManager.foreach(
        [&pointOffsets, &filter, &inCoreOnly](const LeafT& leaf, size_t pos) {
            if (inCoreOnly && leaf.buffer().isOutOfCore())  return;
            auto state = filter.state(leaf);
            if (state == index::ALL) {
                pointOffsets[pos] = leaf.pointCount();
            } else if (state != index::NONE) {
                pointOffsets[pos] = iterCount(leaf.beginIndexAll(filter));
            }
        },
    threaded);

    // turn per-leaf totals into cumulative leaf totals

    Index64 pointOffset(pointOffsets[0]);
    for (size_t n = 1; n < pointOffsets.size(); n++) {
        pointOffset += pointOffsets[n];
        pointOffsets[n] = pointOffset;
    }

    return pointOffset;
}


template <typename PointDataGridT, typename GridT, typename FilterT>
typename GridT::Ptr
pointCountGrid( const PointDataGridT& points,
                const FilterT& filter)
{
    static_assert(std::is_integral<typename GridT::ValueType>::value ||
                  std::is_floating_point<typename GridT::ValueType>::value,
        "laovdb::points::pointCountGrid must return an integer or floating-point scalar grid");

    using PointDataTreeT = typename PointDataGridT::TreeType;
    using TreeT = typename GridT::TreeType;

    typename TreeT::Ptr tree =
        point_mask_internal::convertPointsToScalar<TreeT, PointDataTreeT, FilterT>
            (points.tree(), filter);

    typename GridT::Ptr grid(new GridT(tree));
    grid->setTransform(points.transform().copy());
    return grid;
}


template <typename PointDataGridT, typename GridT, typename FilterT>
typename GridT::Ptr
pointCountGrid( const PointDataGridT& points,
                const laovdb::math::Transform& transform,
                const FilterT& filter)
{
    static_assert(  std::is_integral<typename GridT::ValueType>::value ||
                    std::is_floating_point<typename GridT::ValueType>::value,
        "laovdb::points::pointCountGrid must return an integer or floating-point scalar grid");

    // This is safe because the PointDataGrid can only be modified by the deformer
    using AdapterT = TreeAdapter<typename PointDataGridT::TreeType>;
    auto& nonConstPoints = const_cast<typename AdapterT::NonConstGridType&>(points);

    NullDeformer deformer;
    return point_mask_internal::convertPointsToScalar<GridT>(
        nonConstPoints, transform, filter, deformer);
}


////////////////////////////////////////


} // namespace points
} // namespace OPENVDB_VERSION_NAME
} // namespace laovdb

#endif // OPENVDB_POINTS_POINT_COUNT_HAS_BEEN_INCLUDED
