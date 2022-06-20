// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include "TestHarness.h"
#include "../util.h"

#include <openvdb_ax/compiler/PointExecutable.h>
#include <openvdb_ax/compiler/VolumeExecutable.h>

#include <openvdb/points/PointConversion.h>
#include <openvdb/tools/ValueTransformer.h>

namespace unittest_util
{

std::string loadText(const std::string& codeFileName)
{
    std::ostringstream sstream;
    std::ifstream fs(codeFileName);

    if (fs.fail()) {
        throw std::runtime_error(std::string("Failed to open ") + std::string(codeFileName));
    }

    sstream << fs.rdbuf();
    return sstream.str();
}

bool wrapExecution(laovdb::points::PointDataGrid& grid,
                   const std::string& codeFileName,
                   const std::string * const group,
                   laovdb::ax::Logger& logger,
                   const laovdb::ax::CustomData::Ptr& data,
                   const laovdb::ax::CompilerOptions& opts,
                   const bool createMissing)
{
    using namespace laovdb::ax;

    Compiler compiler(opts);
    const std::string code = loadText(codeFileName);
    ast::Tree::ConstPtr syntaxTree = ast::parse(code.c_str(), logger);
    if (!syntaxTree) return false;
    PointExecutable::Ptr executable = compiler.compile<PointExecutable>(*syntaxTree, logger, data);
    if (!executable) return false;
    executable->setCreateMissing(createMissing);
    if (group) executable->setGroupExecution(*group);
    executable->execute(grid);
    return true;
}

bool wrapExecution(laovdb::GridPtrVec& grids,
                   const std::string& codeFileName,
                   laovdb::ax::Logger& logger,
                   const laovdb::ax::CustomData::Ptr& data,
                   const laovdb::ax::CompilerOptions& opts,
                   const bool createMissing)
{
    using namespace laovdb::ax;

    Compiler compiler(opts);
    const std::string code = loadText(codeFileName);

    ast::Tree::ConstPtr syntaxTree = ast::parse(code.c_str(), logger);
    if (!syntaxTree) return false;
    VolumeExecutable::Ptr executable = compiler.compile<VolumeExecutable>(*syntaxTree, logger, data);
    if (!executable) return false;
    executable->setCreateMissing(createMissing);
    executable->setValueIterator(VolumeExecutable::IterType::ON);
    executable->execute(grids);
    return true;
}

void AXTestHarness::addInputGroups(const std::vector<std::string> &names,
                                   const std::vector<bool> &defaults)
{
    for (size_t i = 0; i < names.size(); i++) {
        for (auto& grid : mInputPointGrids) {
            laovdb::points::appendGroup(grid->tree(), names[i]);
            laovdb::points::setGroup(grid->tree(), names[i], defaults[i]);
        }
    }
}

void AXTestHarness::addExpectedGroups(const std::vector<std::string> &names,
                                      const std::vector<bool> &defaults)
{
    for (size_t i = 0; i < names.size(); i++) {
        for (auto& grid : mOutputPointGrids) {
            laovdb::points::appendGroup(grid->tree(), names[i]);
            laovdb::points::setGroup(grid->tree(), names[i], defaults[i]);
        }
    }
}

bool AXTestHarness::executeCode(const std::string& codeFile,
                                const std::string* const group,
                                const bool createMissing)
{
    if (mUsePoints) {
        for (auto& grid : mInputPointGrids) {
            this->clear();
            if (!wrapExecution(*grid, codeFile, group, mLogger, mCustomData, mOpts, createMissing)) {
                return false;
            }
        }
    }

    if (mUseDenseVolumes) {
        this->clear();
        if (!wrapExecution(mInputDenseVolumeGrids, codeFile, mLogger, mCustomData, mOpts, createMissing)) {
            return false;
        }
    }
    if (mUseSparseVolumes) {
        this->clear();
        if (!wrapExecution(mInputSparseVolumeGrids, codeFile, mLogger, mCustomData, mOpts, createMissing)) {
            return false;
        }
    }
    return true;
}

template <typename T>
void AXTestHarness::addInputPtAttributes(const std::vector<std::string>& names,
                          const std::vector<T>& values)
{
    for (size_t i = 0; i < names.size(); i++) {
        for (auto& grid : mInputPointGrids) {
            laovdb::points::appendAttribute<T>(grid->tree(), names[i], values[i]);
       }
    }
}

template <typename T>
void AXTestHarness::addInputVolumes(const std::vector<std::string>& names,
                     const std::vector<T>& values)
{
    using GridType = typename laovdb::BoolGrid::ValueConverter<T>::Type;

    for (size_t i = 0; i < names.size(); i++) {
        typename GridType::Ptr grid = GridType::create();
        grid->denseFill(mVolumeBounds, values[i], true/*active*/);
        grid->setName(names[i]);
        mInputDenseVolumeGrids.emplace_back(grid);
    }

    for (size_t i = 0; i < names.size(); i++) {
        typename GridType::Ptr grid = GridType::create();
        for (const auto& config : mSparseVolumeConfig) {
            for (const auto& coord : config.second) {
                grid->tree().addTile(config.first, coord, values[i], true);
            }
        }
        grid->setName(names[i]);
        mInputSparseVolumeGrids.emplace_back(grid);
    }
}

template <typename T>
void AXTestHarness::addExpectedPtAttributes(const std::vector<std::string>& names,
                             const std::vector<T>& values)
{
    for (size_t i = 0; i < names.size(); i++) {
        for (auto& grid : mOutputPointGrids) {
            laovdb::points::appendAttribute<T>(grid->tree(), names[i], values[i]);
       }
    }
}

template <typename T>
void AXTestHarness::addExpectedVolumes(const std::vector<std::string>& names,
                        const std::vector<T>& values)
{
    using GridType = typename laovdb::BoolGrid::ValueConverter<T>::Type;

    for (size_t i = 0; i < names.size(); i++) {
        typename GridType::Ptr grid = GridType::create();
        grid->denseFill(mVolumeBounds, values[i], true/*active*/);
        grid->setName(names[i] + "_expected");
        mOutputDenseVolumeGrids.emplace_back(grid);
    }

    for (size_t i = 0; i < names.size(); i++) {
        typename GridType::Ptr grid = GridType::create();
        for (const auto& config : mSparseVolumeConfig) {
            for (const auto& coord : config.second) {
                grid->tree().addTile(config.first, coord, values[i], true);
            }
        }
        grid->setName(names[i]);
        mOutputSparseVolumeGrids.emplace_back(grid);
    }
}

bool AXTestHarness::checkAgainstExpected(std::ostream& sstream)
{
    unittest_util::ComparisonSettings settings;
    bool success = true;

    if (mUsePoints) {
        std::stringstream resultStream;
        unittest_util::ComparisonResult result(resultStream);

        const size_t count = mInputPointGrids.size();
        for (size_t i = 0; i < count; ++i) {
            const auto& input = mInputPointGrids[i];
            const auto& expected = mOutputPointGrids[i];
            const bool pass =
                unittest_util::compareGrids(result, *expected, *input, settings, nullptr);
            if (!pass) sstream << resultStream.str() << std::endl;
            success &= pass;
        }
    }

    if (mUseDenseVolumes) {
        for (size_t i = 0; i < mInputDenseVolumeGrids.size(); i++) {
            std::stringstream resultStream;
            unittest_util::ComparisonResult result(resultStream);
            const bool volumeSuccess =
                unittest_util::compareUntypedGrids(result, *mOutputDenseVolumeGrids[i],
                    *mInputDenseVolumeGrids[i], settings, nullptr);
            success &= volumeSuccess;
            if (!volumeSuccess)  sstream << resultStream.str() << std::endl;
        }
    }

    if (mUseSparseVolumes) {
        for (size_t i = 0; i < mInputSparseVolumeGrids.size(); i++) {
            std::stringstream resultStream;
            unittest_util::ComparisonResult result(resultStream);
            const bool volumeSuccess =
                unittest_util::compareUntypedGrids(result, *mOutputSparseVolumeGrids[i],
                    *mInputSparseVolumeGrids[i], settings, nullptr);
            success &= volumeSuccess;
            if (!volumeSuccess)  sstream << resultStream.str() << std::endl;
        }
    }

    return success;
}

void AXTestHarness::testVolumes(const bool enable)
{
    mUseSparseVolumes = enable;
    mUseDenseVolumes = enable;
}

void AXTestHarness::testSparseVolumes(const bool enable)
{
    mUseSparseVolumes = enable;
}

void AXTestHarness::testDenseVolumes(const bool enable)
{
    mUseDenseVolumes = enable;
}

void AXTestHarness::testPoints(const bool enable)
{
    mUsePoints = enable;
}

void AXTestHarness::reset(const laovdb::Index64 ppv, const laovdb::CoordBBox& bounds)
{
    using laovdb::points::PointDataGrid;
    using laovdb::points::NullCodec;

    mInputPointGrids.clear();
    mOutputPointGrids.clear();
    mInputSparseVolumeGrids.clear();
    mInputDenseVolumeGrids.clear();
    mOutputSparseVolumeGrids.clear();
    mOutputDenseVolumeGrids.clear();

    laovdb::math::Transform::Ptr transform =
        laovdb::math::Transform::createLinearTransform(1.0);
    laovdb::MaskGrid::Ptr mask = laovdb::MaskGrid::create();
    mask->setTransform(transform);
    mask->sparseFill(bounds, true, true);
    laovdb::points::PointDataGrid::Ptr points =
        laovdb::points::denseUniformPointScatter(*mask, static_cast<float>(ppv));
    mask.reset();

    mInputPointGrids.emplace_back(points);
    mOutputPointGrids.emplace_back(points->deepCopy());
    mOutputPointGrids.back()->setName("custom_expected");

    mVolumeBounds = bounds;

    this->clear();
}

void AXTestHarness::reset()
{
    using laovdb::points::PointDataGrid;
    using laovdb::points::NullCodec;

    mInputPointGrids.clear();
    mOutputPointGrids.clear();
    mInputSparseVolumeGrids.clear();
    mInputDenseVolumeGrids.clear();
    mOutputSparseVolumeGrids.clear();
    mOutputDenseVolumeGrids.clear();

    std::vector<laovdb::Vec3d> coordinates =
        {laovdb::Vec3d(0.0, 0.0, 0.0),
         laovdb::Vec3d(0.0, 0.0, 0.05),
         laovdb::Vec3d(0.0, 1.0, 0.0),
         laovdb::Vec3d(1.0, 1.0, 0.0)};

    laovdb::math::Transform::Ptr transform1 =
        laovdb::math::Transform::createLinearTransform(1.0);

    laovdb::points::PointDataGrid::Ptr onePointGrid =
        laovdb::points::createPointDataGrid<NullCodec, PointDataGrid>
            (std::vector<laovdb::Vec3d>{coordinates[0]}, *transform1);

    onePointGrid->setName("1_point");
    mInputPointGrids.emplace_back(onePointGrid);
    mOutputPointGrids.emplace_back(onePointGrid->deepCopy());
    mOutputPointGrids.back()->setName("1_point_expected");

    laovdb::math::Transform::Ptr transform2 =
        laovdb::math::Transform::createLinearTransform(0.1);

    laovdb::points::PointDataGrid::Ptr fourPointGrid =
        laovdb::points::createPointDataGrid<NullCodec, PointDataGrid>
            (coordinates, *transform2);

    fourPointGrid->setName("4_points");
    mInputPointGrids.emplace_back(fourPointGrid);
    mOutputPointGrids.emplace_back(fourPointGrid->deepCopy());
    mOutputPointGrids.back()->setName("4_points_expected");

    mVolumeBounds = laovdb::CoordBBox({0,0,0}, {0,0,0});

    this->clear();
}

template <typename ValueT>
using ConverterT = typename laovdb::BoolGrid::ValueConverter<ValueT>::Type;

void AXTestHarness::resetInputsToZero()
{
    for (auto& grid : mInputPointGrids) {
        laovdb::tree::LeafManager<laovdb::points::PointDataTree> manager(grid->tree());
        manager.foreach([](laovdb::points::PointDataTree::LeafNodeType& leaf, size_t) {
            const size_t attrs = leaf.attributeSet().size();
            const size_t pidx = leaf.attributeSet().descriptor().find("P");
            for (size_t idx = 0; idx < attrs; ++idx) {
                if (idx == pidx) continue;
                leaf.attributeArray(idx).collapse();
            }
        });
    }

    /// @todo: share with volume executable when the move to header files is made
    ///        for customization of grid types.
    using SupportedTypeList = laovdb::TypeList<
        ConverterT<double>,
        ConverterT<float>,
        ConverterT<int64_t>,
        ConverterT<int32_t>,
        ConverterT<int16_t>,
        ConverterT<bool>,
        ConverterT<laovdb::math::Vec2<double>>,
        ConverterT<laovdb::math::Vec2<float>>,
        ConverterT<laovdb::math::Vec2<int32_t>>,
        ConverterT<laovdb::math::Vec3<double>>,
        ConverterT<laovdb::math::Vec3<float>>,
        ConverterT<laovdb::math::Vec3<int32_t>>,
        ConverterT<laovdb::math::Vec4<double>>,
        ConverterT<laovdb::math::Vec4<float>>,
        ConverterT<laovdb::math::Vec4<int32_t>>,
        ConverterT<laovdb::math::Mat3<double>>,
        ConverterT<laovdb::math::Mat3<float>>,
        ConverterT<laovdb::math::Mat4<double>>,
        ConverterT<laovdb::math::Mat4<float>>,
        ConverterT<std::string>>;

    for (auto& grid : mInputSparseVolumeGrids) {
        const bool success = grid->apply<SupportedTypeList>([](auto& typed) {
            using GridType = typename std::decay<decltype(typed)>::type;
            laovdb::tools::foreach(typed.beginValueAll(), [](auto it) {
                it.setValue(laovdb::zeroVal<typename GridType::ValueType>());
            });
        });
        if (!success) {
            throw std::runtime_error("Unable to reset input grid of an unsupported type");
        }
    }

    for (auto& grid : mInputDenseVolumeGrids) {
        const bool success = grid->apply<SupportedTypeList>([](auto& typed) {
            using GridType = typename std::decay<decltype(typed)>::type;
            laovdb::tools::foreach(typed.beginValueAll(), [](auto it) {
                it.setValue(laovdb::zeroVal<typename GridType::ValueType>());
            });
        });
        if (!success) {
            throw std::runtime_error("Unable to reset input grid of an unsupported type");
        }
    }
}


#define REGISTER_HARNESS_METHODS(T) \
template void AXTestHarness::addInputPtAttributes<T>(const std::vector<std::string>&, const std::vector<T>&); \
template void AXTestHarness::addInputVolumes<T>(const std::vector<std::string>&, const std::vector<T>&); \
template void AXTestHarness::addExpectedPtAttributes<T>(const std::vector<std::string>&, const std::vector<T>&); \
template void AXTestHarness::addExpectedVolumes<T>(const std::vector<std::string>&, const std::vector<T>&);

REGISTER_HARNESS_METHODS(double)
REGISTER_HARNESS_METHODS(float)
REGISTER_HARNESS_METHODS(int64_t)
REGISTER_HARNESS_METHODS(int32_t)
REGISTER_HARNESS_METHODS(int16_t)
REGISTER_HARNESS_METHODS(bool)
REGISTER_HARNESS_METHODS(laovdb::math::Vec2<double>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec2<float>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec2<int32_t>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec3<double>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec3<float>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec3<int32_t>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec4<double>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec4<float>)
REGISTER_HARNESS_METHODS(laovdb::math::Vec4<int32_t>)
REGISTER_HARNESS_METHODS(laovdb::math::Mat3<double>)
REGISTER_HARNESS_METHODS(laovdb::math::Mat3<float>)
REGISTER_HARNESS_METHODS(laovdb::math::Mat4<double>)
REGISTER_HARNESS_METHODS(laovdb::math::Mat4<float>)
REGISTER_HARNESS_METHODS(std::string)

}


