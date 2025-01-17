// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#ifndef OPENVDB_IO_GRIDDESCRIPTOR_HAS_BEEN_INCLUDED
#define OPENVDB_IO_GRIDDESCRIPTOR_HAS_BEEN_INCLUDED

#include <openvdb/Grid.h>
#include <iostream>
#include <string>

namespace laovdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {
namespace io {

/// This structure stores useful information that describes a grid on disk.
/// It can be used to retrieve I/O information about the grid such as
/// offsets into the file where the grid is located, its type, etc.
class OPENVDB_API GridDescriptor
{
public:
    GridDescriptor();
    GridDescriptor(const Name& name, const Name& gridType, bool saveFloatAsHalf = false);
    GridDescriptor(const GridDescriptor&) = default;
    GridDescriptor& operator=(const GridDescriptor&) = default;
    ~GridDescriptor();

    const Name& gridType() const { return mGridType; }
    const Name& gridName() const { return mGridName; }
    const Name& uniqueName() const { return mUniqueName; }

    const Name& instanceParentName() const { return mInstanceParentName; }
    void setInstanceParentName(const Name& name) { mInstanceParentName = name; }
    bool isInstance() const { return !mInstanceParentName.empty(); }

    bool saveFloatAsHalf() const { return mSaveFloatAsHalf; }

    void setGridPos(int64_t pos) { mGridPos = pos; }
    int64_t getGridPos() const { return mGridPos; }

    void setBlockPos(int64_t pos) { mBlockPos = pos; }
    int64_t getBlockPos() const { return mBlockPos; }

    void setEndPos(int64_t pos) { mEndPos = pos; }
    int64_t getEndPos() const { return mEndPos; }

    // These methods seek to the right position in the given stream.
    void seekToGrid(std::istream&) const;
    void seekToBlocks(std::istream&) const;
    void seekToEnd(std::istream&) const;

    void seekToGrid(std::ostream&) const;
    void seekToBlocks(std::ostream&) const;
    void seekToEnd(std::ostream&) const;

    /// @brief Write out this descriptor's header information (all data except for
    /// stream offsets).
    void writeHeader(std::ostream&) const;

    /// @brief Since positions into the stream are known at a later time, they are
    /// written out separately.
    void writeStreamPos(std::ostream&) const;

    /// @brief Read a grid descriptor from the given stream.
    /// @return an empty grid of the type specified by the grid descriptor.
    GridBase::Ptr read(std::istream&);

    /// @brief Append the number @a n to the given name (separated by an ASCII
    /// "record separator" character) and return the resulting name.
    static Name addSuffix(const Name&, int n);
    /// @brief Strip from the given name any suffix that is separated by an ASCII
    /// "record separator" character and return the resulting name.
    static Name stripSuffix(const Name&);
    /// @brief Given a name with suffix N, return "name[N]", otherwise just return "name".
    /// Use this to produce a human-readable string from a descriptor's unique name.
    static std::string nameAsString(const Name&);
    /// @brief Given a string of the form "name[N]", return "name" with the suffix N
    /// separated by an ASCII "record separator" character).  Otherwise just return
    /// the string as is.
    static Name stringAsUniqueName(const std::string&);

private:
    /// Name of the grid
    Name mGridName;
    /// Unique name for this descriptor
    Name mUniqueName;
    /// If nonempty, the name of another grid that shares this grid's tree
    Name mInstanceParentName;
    /// The type of the grid
    Name mGridType;
    /// Are floats quantized to 16 bits on disk?
    bool mSaveFloatAsHalf;
    /// Location in the stream where the grid data is stored
    int64_t mGridPos;
    /// Location in the stream where the grid blocks are stored
    int64_t mBlockPos;
    /// Location in the stream where the next grid descriptor begins
    int64_t mEndPos;
};

} // namespace io
} // namespace OPENVDB_VERSION_NAME
} // namespace laovdb

#endif // OPENVDB_IO_GRIDDESCRIPTOR_HAS_BEEN_INCLUDED
