// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <iostream> // must be included before python on macos
#include <cstring> // for strncmp(), strrchr(), etc.
#include <limits>
#include <string>
#include <utility> // for std::make_pair()
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/exception_translator.hpp>
#include "openvdb/openvdb.h"
#include "pyopenvdb.h"
#include "pyGrid.h"
#include "pyutil.h"

#ifdef PY_OPENVDB_USE_AX
#include <openvdb_ax/ax.h>
#endif

namespace py = boost::python;


// Forward declarations
void exportTransform();
void exportMetadata();
void exportFloatGrid();
void exportIntGrid();
void exportVec3Grid();
void exportPointGrid();


namespace _openvdbmodule {

using namespace laovdb;


/// Helper class to convert between a Python numeric sequence
/// (tuple, list, etc.) and an laovdb::Coord
struct CoordConverter
{
    /// @return a Python tuple object equivalent to the given Coord.
    static PyObject* convert(const laovdb::Coord& xyz)
    {
        py::object obj = py::make_tuple(xyz[0], xyz[1], xyz[2]);
        Py_INCREF(obj.ptr());
            ///< @todo is this the right way to ensure that the object
            ///< doesn't get freed on exit?
        return obj.ptr();
    }

    /// @return nullptr if the given Python object is not convertible to a Coord.
    static void* convertible(PyObject* obj)
    {
        if (!PySequence_Check(obj)) return nullptr; // not a Python sequence

        Py_ssize_t len = PySequence_Length(obj);
        if (len != 3 && len != 1) return nullptr; // not the right length

        return obj;
    }

    /// Convert from a Python object to a Coord.
    static void construct(PyObject* obj,
        py::converter::rvalue_from_python_stage1_data* data)
    {
        // Construct a Coord in the provided memory location.
        using StorageT = py::converter::rvalue_from_python_storage<laovdb::Coord>;
        void* storage = reinterpret_cast<StorageT*>(data)->storage.bytes;
        new (storage) laovdb::Coord; // placement new
        data->convertible = storage;

        laovdb::Coord* xyz = static_cast<laovdb::Coord*>(storage);

        // Populate the Coord.
        switch (PySequence_Length(obj)) {
        case 1:
            xyz->reset(pyutil::getSequenceItem<laovdb::Int32>(obj, 0));
            break;
        case 3:
            xyz->reset(
                pyutil::getSequenceItem<laovdb::Int32>(obj, 0),
                pyutil::getSequenceItem<laovdb::Int32>(obj, 1),
                pyutil::getSequenceItem<laovdb::Int32>(obj, 2));
            break;
        default:
            PyErr_Format(PyExc_ValueError,
                "expected a sequence of three integers");
            py::throw_error_already_set();
            break;
        }
    }

    /// Register both the Coord-to-tuple and the sequence-to-Coord converters.
    static void registerConverter()
    {
        py::to_python_converter<laovdb::Coord, CoordConverter>();
        py::converter::registry::push_back(
            &CoordConverter::convertible,
            &CoordConverter::construct,
            py::type_id<laovdb::Coord>());
    }
}; // struct CoordConverter

/// @todo CoordBBoxConverter?


////////////////////////////////////////


/// Helper class to convert between a Python numeric sequence
/// (tuple, list, etc.) and an laovdb::Vec
template<typename VecT>
struct VecConverter
{
    static PyObject* convert(const VecT& v)
    {
        py::object obj;
        OPENVDB_NO_UNREACHABLE_CODE_WARNING_BEGIN
        switch (VecT::size) { // compile-time constant
            case 2: obj = py::make_tuple(v[0], v[1]); break;
            case 3: obj = py::make_tuple(v[0], v[1], v[2]); break;
            case 4: obj = py::make_tuple(v[0], v[1], v[2], v[3]); break;
            default:
            {
                py::list lst;
                for (int n = 0; n < VecT::size; ++n) lst.append(v[n]);
                obj = lst;
            }
        }
        OPENVDB_NO_UNREACHABLE_CODE_WARNING_END
        Py_INCREF(obj.ptr());
        return obj.ptr();
    }

    static void* convertible(PyObject* obj)
    {
        if (!PySequence_Check(obj)) return nullptr; // not a Python sequence

        Py_ssize_t len = PySequence_Length(obj);
        if (len != VecT::size) return nullptr;

        // Check that all elements of the Python sequence are convertible
        // to the Vec's value type.
        py::object seq = pyutil::pyBorrow(obj);
        for (int i = 0; i < VecT::size; ++i) {
            if (!py::extract<typename VecT::value_type>(seq[i]).check()) {
                return nullptr;
            }
        }
        return obj;
    }

    static void construct(PyObject* obj,
        py::converter::rvalue_from_python_stage1_data* data)
    {
        // Construct a Vec in the provided memory location.
        using StorageT = py::converter::rvalue_from_python_storage<VecT>;
        void* storage = reinterpret_cast<StorageT*>(data)->storage.bytes;
        new (storage) VecT; // placement new
        data->convertible = storage;
        VecT* v = static_cast<VecT*>(storage);

        // Populate the vector.
        for (int n = 0; n < VecT::size; ++n) {
            (*v)[n] = pyutil::getSequenceItem<typename VecT::value_type>(obj, n);
        }
    }

    static void registerConverter()
    {
        py::to_python_converter<VecT, VecConverter<VecT> >();
        py::converter::registry::push_back(
            &VecConverter<VecT>::convertible,
            &VecConverter<VecT>::construct,
            py::type_id<VecT>());
    }
}; // struct VecConverter


////////////////////////////////////////


/// Helper class to convert between a 2D Python numeric sequence
/// (tuple, list, etc.) and an laovdb::Mat
template<typename MatT>
struct MatConverter
{
    /// Return the given matrix as a Python list of lists.
    static py::object toList(const MatT& m)
    {
        py::list obj;
        for (int i = 0; i < MatT::size; ++i) {
            py::list rowObj;
            for (int j = 0; j < MatT::size; ++j) { rowObj.append(m(i, j)); }
            obj.append(rowObj);
        }
        return std::move(obj);
    }

    /// Extract a matrix from a Python sequence of numeric sequences.
    static MatT fromSeq(py::object obj)
    {
        MatT m = MatT::zero();
        if (py::len(obj) == MatT::size) {
            for (int i = 0; i < MatT::size; ++i) {
                py::object rowObj = obj[i];
                if (py::len(rowObj) != MatT::size) return MatT::zero();
                for (int j = 0; j < MatT::size; ++j) {
                    m(i, j) = py::extract<typename MatT::value_type>(rowObj[j]);
                }
            }
        }
        return m;
    }

    static PyObject* convert(const MatT& m)
    {
        py::object obj = toList(m);
        Py_INCREF(obj.ptr());
        return obj.ptr();
    }

    static void* convertible(PyObject* obj)
    {
        if (!PySequence_Check(obj)) return nullptr; // not a Python sequence

        Py_ssize_t len = PySequence_Length(obj);
        if (len != MatT::size) return nullptr;

        py::object seq = pyutil::pyBorrow(obj);
        for (int i = 0; i < MatT::size; ++i) {
            py::object rowObj = seq[i];
            if (py::len(rowObj) != MatT::size) return nullptr;
            // Check that all elements of the Python sequence are convertible
            // to the Mat's value type.
            for (int j = 0; j < MatT::size; ++j) {
                if (!py::extract<typename MatT::value_type>(rowObj[j]).check()) {
                    return nullptr;
                }
            }
        }
        return obj;
    }

    static void construct(PyObject* obj,
        py::converter::rvalue_from_python_stage1_data* data)
    {
        // Construct a Mat in the provided memory location.
        using StorageT = py::converter::rvalue_from_python_storage<MatT>;
        void* storage = reinterpret_cast<StorageT*>(data)->storage.bytes;
        new (storage) MatT; // placement new
        data->convertible = storage;
        *(static_cast<MatT*>(storage)) = fromSeq(pyutil::pyBorrow(obj));
    }

    static void registerConverter()
    {
        py::to_python_converter<MatT, MatConverter<MatT> >();
        py::converter::registry::push_back(
            &MatConverter<MatT>::convertible,
            &MatConverter<MatT>::construct,
            py::type_id<MatT>());
    }
}; // struct MatConverter


////////////////////////////////////////


/// Helper class to convert between a Python integer and a laovdb::PointIndex
template <typename PointIndexT>
struct PointIndexConverter
{
    using IntType = typename PointIndexT::IntType;

    /// @return a Python integer object equivalent to the given PointIndex.
    static PyObject* convert(const PointIndexT& index)
    {
        py::object obj(static_cast<IntType>(index));
        Py_INCREF(obj.ptr());
        return obj.ptr();
    }

    /// @return nullptr if the given Python object is not convertible to the PointIndex.
    static void* convertible(PyObject* obj)
    {
#if PY_MAJOR_VERSION >= 3
        if (!PyLong_Check(obj)) return nullptr; // not a Python integer
#else
        if (!PyInt_Check(obj)) return nullptr; // not a Python integer
#endif
        return obj;
    }

    /// Convert from a Python object to a PointIndex.
    static void construct(PyObject* obj,
        py::converter::rvalue_from_python_stage1_data* data)
    {
        // Construct a PointIndex in the provided memory location.
        using StorageT = py::converter::rvalue_from_python_storage<PointIndexT>;
        void* storage = reinterpret_cast<StorageT*>(data)->storage.bytes;
        new (storage) PointIndexT; // placement new
        data->convertible = storage;

        // Extract the PointIndex from the python integer
        PointIndexT* index = static_cast<PointIndexT*>(storage);
#if PY_MAJOR_VERSION >= 3
        *index = static_cast<IntType>(PyLong_AsLong(obj));
#else
        *index = static_cast<IntType>(PyInt_AsLong(obj));
#endif
    }

    /// Register both the PointIndex-to-integer and the integer-to-PointIndex converters.
    static void registerConverter()
    {
        py::to_python_converter<PointIndexT, PointIndexConverter>();
        py::converter::registry::push_back(
            &PointIndexConverter::convertible,
            &PointIndexConverter::construct,
            py::type_id<PointIndexT>());
    }
}; // struct PointIndexConverter


////////////////////////////////////////


/// Helper class to convert between a Python dict and an laovdb::MetaMap
/// @todo Consider implementing a separate, templated converter for
/// the various Metadata types.
struct MetaMapConverter
{
    static PyObject* convert(const MetaMap& metaMap)
    {
        py::dict ret;
        for (MetaMap::ConstMetaIterator it = metaMap.beginMeta();
            it != metaMap.endMeta(); ++it)
        {
            if (Metadata::Ptr meta = it->second) {
                py::object obj(meta);
                const std::string typeName = meta->typeName();
                if (typeName == StringMetadata::staticTypeName()) {
                    obj = py::str(static_cast<StringMetadata&>(*meta).value());
                } else if (typeName == DoubleMetadata::staticTypeName()) {
                    obj = py::object(static_cast<DoubleMetadata&>(*meta).value());
                } else if (typeName == FloatMetadata::staticTypeName()) {
                    obj = py::object(static_cast<FloatMetadata&>(*meta).value());
                } else if (typeName == Int32Metadata::staticTypeName()) {
                    obj = py::object(static_cast<Int32Metadata&>(*meta).value());
                } else if (typeName == Int64Metadata::staticTypeName()) {
                    obj = py::object(static_cast<Int64Metadata&>(*meta).value());
                } else if (typeName == BoolMetadata::staticTypeName()) {
                    obj = py::object(static_cast<BoolMetadata&>(*meta).value());
                } else if (typeName == Vec2DMetadata::staticTypeName()) {
                    const Vec2d v = static_cast<Vec2DMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1]);
                } else if (typeName == Vec2IMetadata::staticTypeName()) {
                    const Vec2i v = static_cast<Vec2IMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1]);
                } else if (typeName == Vec2SMetadata::staticTypeName()) {
                    const Vec2s v = static_cast<Vec2SMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1]);
                } else if (typeName == Vec3DMetadata::staticTypeName()) {
                    const Vec3d v = static_cast<Vec3DMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2]);
                } else if (typeName == Vec3IMetadata::staticTypeName()) {
                    const Vec3i v = static_cast<Vec3IMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2]);
                } else if (typeName == Vec3SMetadata::staticTypeName()) {
                    const Vec3s v = static_cast<Vec3SMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2]);
                } else if (typeName == Vec4DMetadata::staticTypeName()) {
                    const Vec4d v = static_cast<Vec4DMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2], v[3]);
                } else if (typeName == Vec4IMetadata::staticTypeName()) {
                    const Vec4i v = static_cast<Vec4IMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2], v[3]);
                } else if (typeName == Vec4SMetadata::staticTypeName()) {
                    const Vec4s v = static_cast<Vec4SMetadata&>(*meta).value();
                    obj = py::make_tuple(v[0], v[1], v[2], v[3]);
                } else if (typeName == Mat4SMetadata::staticTypeName()) {
                    const Mat4s m = static_cast<Mat4SMetadata&>(*meta).value();
                    obj = MatConverter<Mat4s>::toList(m);
                } else if (typeName == Mat4DMetadata::staticTypeName()) {
                    const Mat4d m = static_cast<Mat4DMetadata&>(*meta).value();
                    obj = MatConverter<Mat4d>::toList(m);
                }
                ret[it->first] = obj;
            }
        }
        Py_INCREF(ret.ptr());
        return ret.ptr();
    }

    static void* convertible(PyObject* obj)
    {
        return (PyMapping_Check(obj) ? obj : nullptr);
    }

    static void construct(PyObject* obj,
        py::converter::rvalue_from_python_stage1_data* data)
    {
        // Construct a MetaMap in the provided memory location.
        using StorageT = py::converter::rvalue_from_python_storage<MetaMap>;
        void* storage = reinterpret_cast<StorageT*>(data)->storage.bytes;
        new (storage) MetaMap; // placement new
        data->convertible = storage;
        MetaMap* metaMap = static_cast<MetaMap*>(storage);

        // Populate the map.
        py::dict pyDict(pyutil::pyBorrow(obj));
        py::list keys = pyDict.keys();
        for (size_t i = 0, N = py::len(keys); i < N; ++i) {
            std::string name;
            py::object key = keys[i];
            if (py::extract<std::string>(key).check()) {
                name = py::extract<std::string>(key);
            } else {
                const std::string
                    keyAsStr = py::extract<std::string>(key.attr("__str__")()),
                    keyType = pyutil::className(key);
                PyErr_Format(PyExc_TypeError,
                    "expected string as metadata name, found object"
                    " \"%s\" of type %s", keyAsStr.c_str(), keyType.c_str());
                py::throw_error_already_set();
            }

            // Note: the order of the following tests is significant, as it
            // avoids unnecessary type promotion (e.g., of ints to floats).
            py::object val = pyDict[keys[i]];
            Metadata::Ptr value;
            if (py::extract<std::string>(val).check()) {
                value.reset(new StringMetadata(py::extract<std::string>(val)));
            } else if (bool(PyBool_Check(val.ptr()))) {
                value.reset(new BoolMetadata(py::extract<bool>(val)));
            } else if (py::extract<Int64>(val).check()) {
                const Int64 n = py::extract<Int64>(val);
                if (n <= std::numeric_limits<Int32>::max()
                    && n >= std::numeric_limits<Int32>::min())
                {
                    value.reset(new Int32Metadata(static_cast<Int32>(n)));
                } else {
                    value.reset(new Int64Metadata(n));
                }
            //} else if (py::extract<float>(val).check()) {
            //    value.reset(new FloatMetadata(py::extract<float>(val)));
            } else if (py::extract<double>(val).check()) {
                value.reset(new DoubleMetadata(py::extract<double>(val)));
            } else if (py::extract<Vec2i>(val).check()) {
                value.reset(new Vec2IMetadata(py::extract<Vec2i>(val)));
            } else if (py::extract<Vec2d>(val).check()) {
                value.reset(new Vec2DMetadata(py::extract<Vec2d>(val)));
            } else if (py::extract<Vec2s>(val).check()) {
                value.reset(new Vec2SMetadata(py::extract<Vec2s>(val)));
            } else if (py::extract<Vec3i>(val).check()) {
                value.reset(new Vec3IMetadata(py::extract<Vec3i>(val)));
            } else if (py::extract<Vec3d>(val).check()) {
                value.reset(new Vec3DMetadata(py::extract<Vec3d>(val)));
            } else if (py::extract<Vec3s>(val).check()) {
                value.reset(new Vec3SMetadata(py::extract<Vec3s>(val)));
            } else if (py::extract<Vec4i>(val).check()) {
                value.reset(new Vec4IMetadata(py::extract<Vec4i>(val)));
            } else if (py::extract<Vec4d>(val).check()) {
                value.reset(new Vec4DMetadata(py::extract<Vec4d>(val)));
            } else if (py::extract<Vec4s>(val).check()) {
                value.reset(new Vec4SMetadata(py::extract<Vec4s>(val)));
            } else if (py::extract<Mat4d>(val).check()) {
                value.reset(new Mat4DMetadata(py::extract<Mat4d>(val)));
            } else if (py::extract<Mat4s>(val).check()) {
                value.reset(new Mat4SMetadata(py::extract<Mat4s>(val)));
            } else if (py::extract<Metadata::Ptr>(val).check()) {
                value = py::extract<Metadata::Ptr>(val);
            } else {
                const std::string
                    valAsStr = py::extract<std::string>(val.attr("__str__")()),
                    valType = pyutil::className(val);
                PyErr_Format(PyExc_TypeError,
                    "metadata value \"%s\" of type %s is not allowed",
                    valAsStr.c_str(), valType.c_str());
                py::throw_error_already_set();
            }
            if (value) metaMap->insertMeta(name, *value);
        }
    }

    static void registerConverter()
    {
        py::to_python_converter<MetaMap, MetaMapConverter>();
        py::converter::registry::push_back(
            &MetaMapConverter::convertible,
            &MetaMapConverter::construct,
            py::type_id<MetaMap>());
    }
}; // struct MetaMapConverter


////////////////////////////////////////


template<typename T> void translateException(const T&) {}

/// @brief Define a function that translates an OpenVDB exception into
/// the equivalent Python exception.
/// @details laovdb::Exception::what() typically returns a string of the form
/// "<exception>: <description>".  To avoid duplication of the exception name in Python
/// stack traces, the function strips off the "<exception>: " prefix.  To do that,
/// it needs the class name in the form of a string, hence the preprocessor macro.
#define PYOPENVDB_CATCH(_openvdbname, _pyname)                      \
    template<>                                                      \
    void translateException<_openvdbname>(const _openvdbname& e)    \
    {                                                               \
        const char* name = #_openvdbname;                           \
        if (const char* c = std::strrchr(name, ':')) name = c + 1;  \
        const int namelen = int(std::strlen(name));                 \
        const char* msg = e.what();                                 \
        if (0 == std::strncmp(msg, name, namelen)) msg += namelen;  \
        if (0 == std::strncmp(msg, ": ", 2)) msg += 2;              \
        PyErr_SetString(_pyname, msg);                              \
    }


/// Define an overloaded function that translate all OpenVDB exceptions into
/// their Python equivalents.
/// @todo LookupError is redundant and should someday be removed.
PYOPENVDB_CATCH(laovdb::ArithmeticError,       PyExc_ArithmeticError)
PYOPENVDB_CATCH(laovdb::IndexError,            PyExc_IndexError)
PYOPENVDB_CATCH(laovdb::IoError,               PyExc_IOError)
PYOPENVDB_CATCH(laovdb::KeyError,              PyExc_KeyError)
PYOPENVDB_CATCH(laovdb::LookupError,           PyExc_LookupError)
PYOPENVDB_CATCH(laovdb::NotImplementedError,   PyExc_NotImplementedError)
PYOPENVDB_CATCH(laovdb::ReferenceError,        PyExc_ReferenceError)
PYOPENVDB_CATCH(laovdb::RuntimeError,          PyExc_RuntimeError)
PYOPENVDB_CATCH(laovdb::TypeError,             PyExc_TypeError)
PYOPENVDB_CATCH(laovdb::ValueError,            PyExc_ValueError)

#undef PYOPENVDB_CATCH


////////////////////////////////////////


py::object readFromFile(const std::string&, const std::string&);
py::tuple readAllFromFile(const std::string&);
py::dict readFileMetadata(const std::string&);
py::object readGridMetadataFromFile(const std::string&, const std::string&);
py::list readAllGridMetadataFromFile(const std::string&);
void writeToFile(const std::string&, py::object, py::object);

#ifdef PY_OPENVDB_USE_AX
void axrun(const std::string&, py::object);
#endif


py::object
readFromFile(const std::string& filename, const std::string& gridName)
{
    io::File vdbFile(filename);
    vdbFile.open();

    if (!vdbFile.hasGrid(gridName)) {
        PyErr_Format(PyExc_KeyError,
            "file %s has no grid named \"%s\"",
            filename.c_str(), gridName.c_str());
        py::throw_error_already_set();
    }

    return pyGrid::getGridFromGridBase(vdbFile.readGrid(gridName));
}


py::tuple
readAllFromFile(const std::string& filename)
{
    io::File vdbFile(filename);
    vdbFile.open();

    GridPtrVecPtr grids = vdbFile.getGrids();
    MetaMap::Ptr metadata = vdbFile.getMetadata();
    vdbFile.close();

    py::list gridList;
    for (GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it) {
        gridList.append(pyGrid::getGridFromGridBase(*it));
    }

    return py::make_tuple(gridList, py::dict(*metadata));
}


py::dict
readFileMetadata(const std::string& filename)
{
    io::File vdbFile(filename);
    vdbFile.open();

    MetaMap::Ptr metadata = vdbFile.getMetadata();
    vdbFile.close();

    return py::dict(*metadata);
}


py::object
readGridMetadataFromFile(const std::string& filename, const std::string& gridName)
{
    io::File vdbFile(filename);
    vdbFile.open();

    if (!vdbFile.hasGrid(gridName)) {
        PyErr_Format(PyExc_KeyError,
            "file %s has no grid named \"%s\"",
            filename.c_str(), gridName.c_str());
        py::throw_error_already_set();
    }

    return pyGrid::getGridFromGridBase(vdbFile.readGridMetadata(gridName));
}


py::list
readAllGridMetadataFromFile(const std::string& filename)
{
    io::File vdbFile(filename);
    vdbFile.open();
    GridPtrVecPtr grids = vdbFile.readAllGridMetadata();
    vdbFile.close();

    py::list gridList;
    for (GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it) {
        gridList.append(pyGrid::getGridFromGridBase(*it));
    }
    return gridList;
}


void
writeToFile(const std::string& filename, py::object gridOrSeqObj, py::object dictObj)
{
    GridPtrVec gridVec;
    try {
        GridBase::Ptr base = pyopenvdb::getGridFromPyObject(gridOrSeqObj);
        gridVec.push_back(base);
    } catch (laovdb::TypeError&) {
        for (py::stl_input_iterator<py::object> it(gridOrSeqObj), end; it != end; ++it) {
            if (GridBase::Ptr base = pyGrid::getGridBaseFromGrid(*it)) {
                gridVec.push_back(base);
            }
        }
    }

    io::File vdbFile(filename);
    if (dictObj.is_none()) {
        vdbFile.write(gridVec);
    } else {
        MetaMap metadata = py::extract<MetaMap>(dictObj);
        vdbFile.write(gridVec, metadata);
    }
    vdbFile.close();
}

#ifdef PY_OPENVDB_USE_AX
void axrun(const std::string& code, py::object gridOrSeqObj)
{
    GridPtrVec gridVec;
    try {
        GridBase::Ptr base = pyopenvdb::getGridFromPyObject(gridOrSeqObj);
        gridVec.push_back(base);
    } catch (laovdb::TypeError&) {
        for (py::stl_input_iterator<py::object> it(gridOrSeqObj), end; it != end; ++it) {
            if (GridBase::Ptr base = pyGrid::getGridBaseFromGrid(*it)) {
                gridVec.push_back(base);
            }
        }
    }

    try { laovdb::ax::run(code.c_str(), gridVec); }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
#endif

////////////////////////////////////////


std::string getLoggingLevel();
void setLoggingLevel(py::object);
void setProgramName(py::object, bool);


std::string
getLoggingLevel()
{
    switch (logging::getLevel()) {
        case logging::Level::Debug: return "debug";
        case logging::Level::Info:  return "info";
        case logging::Level::Warn:  return "warn";
        case logging::Level::Error: return "error";
        case logging::Level::Fatal: break;
    }
    return "fatal";
}


void
setLoggingLevel(py::object pyLevelObj)
{
    std::string levelStr;
    if (!py::extract<py::str>(pyLevelObj).check()) {
        levelStr = py::extract<std::string>(pyLevelObj.attr("__str__")());
    } else {
        const py::str pyLevelStr =
            py::extract<py::str>(pyLevelObj.attr("lower")().attr("lstrip")("-"));
        levelStr = py::extract<std::string>(pyLevelStr);
        if (levelStr == "debug") { logging::setLevel(logging::Level::Debug); return; }
        else if (levelStr == "info") { logging::setLevel(logging::Level::Info); return; }
        else if (levelStr == "warn") { logging::setLevel(logging::Level::Warn); return; }
        else if (levelStr == "error") { logging::setLevel(logging::Level::Error); return; }
        else if (levelStr == "fatal") { logging::setLevel(logging::Level::Fatal); return; }
    }
    PyErr_Format(PyExc_ValueError,
        "expected logging level \"debug\", \"info\", \"warn\", \"error\", or \"fatal\","
        " got \"%s\"", levelStr.c_str());
    py::throw_error_already_set();
}


void
setProgramName(py::object nameObj, bool color)
{
    if (py::extract<std::string>(nameObj).check()) {
        logging::setProgramName(py::extract<std::string>(nameObj), color);
    } else {
        const std::string
            str = py::extract<std::string>(nameObj.attr("__str__")()),
            typ = pyutil::className(nameObj).c_str();
        PyErr_Format(PyExc_TypeError,
            "expected string as program name, got \"%s\" of type %s",
            str.c_str(), typ.c_str());
        py::throw_error_already_set();
    }
}


////////////////////////////////////////


// Descriptor for the laovdb::GridClass enum (for use with pyutil::StringEnum)
struct GridClassDescr
{
    static const char* name() { return "GridClass"; }
    static const char* doc()
    {
        return "Classes of volumetric data (level set, fog volume, etc.)";
    }
    static pyutil::CStringPair item(int i)
    {
        static const int sCount = 4;
        static const char* const sStrings[sCount][2] = {
            { "UNKNOWN",    strdup(GridBase::gridClassToString(GRID_UNKNOWN).c_str()) },
            { "LEVEL_SET",  strdup(GridBase::gridClassToString(GRID_LEVEL_SET).c_str()) },
            { "FOG_VOLUME", strdup(GridBase::gridClassToString(GRID_FOG_VOLUME).c_str()) },
            { "STAGGERED",  strdup(GridBase::gridClassToString(GRID_STAGGERED).c_str()) }
        };
        if (i >= 0 && i < sCount) return pyutil::CStringPair(&sStrings[i][0], &sStrings[i][1]);
        return pyutil::CStringPair(static_cast<char**>(nullptr), static_cast<char**>(nullptr));
    }
};


// Descriptor for the laovdb::VecType enum (for use with pyutil::StringEnum)
struct VecTypeDescr
{
    static const char* name() { return "VectorType"; }
    static const char* doc()
    {
        return
            "The type of a vector determines how transforms are applied to it.\n"
            "  - INVARIANT:\n"
            "      does not transform (e.g., tuple, uvw, color)\n"
            "  - COVARIANT:\n"
            "      apply inverse-transpose transformation with w = 0\n"
            "      and ignore translation (e.g., gradient/normal)\n"
            "  - COVARIANT_NORMALIZE:\n"
            "      apply inverse-transpose transformation with w = 0\n"
            "      and ignore translation, vectors are renormalized\n"
            "      (e.g., unit normal)\n"
            "  - CONTRAVARIANT_RELATIVE:\n"
            "      apply \"regular\" transformation with w = 0 and ignore\n"
            "      translation (e.g., displacement, velocity, acceleration)\n"
            "  - CONTRAVARIANT_ABSOLUTE:\n"
            "      apply \"regular\" transformation with w = 1 so that\n"
            "      vector translates (e.g., position)\n";
    }
    static pyutil::CStringPair item(int i)
    {
        static const int sCount = 5;
        static const char* const sStrings[sCount][2] = {
            { "INVARIANT", strdup(GridBase::vecTypeToString(laovdb::VEC_INVARIANT).c_str()) },
            { "COVARIANT", strdup(GridBase::vecTypeToString(laovdb::VEC_COVARIANT).c_str()) },
            { "COVARIANT_NORMALIZE",
                strdup(GridBase::vecTypeToString(laovdb::VEC_COVARIANT_NORMALIZE).c_str()) },
            { "CONTRAVARIANT_RELATIVE",
                strdup(GridBase::vecTypeToString(laovdb::VEC_CONTRAVARIANT_RELATIVE).c_str()) },
            { "CONTRAVARIANT_ABSOLUTE",
                strdup(GridBase::vecTypeToString(laovdb::VEC_CONTRAVARIANT_ABSOLUTE).c_str()) }
        };
        if (i >= 0 && i < sCount) return std::make_pair(&sStrings[i][0], &sStrings[i][1]);
        return pyutil::CStringPair(static_cast<char**>(nullptr), static_cast<char**>(nullptr));
    }
};

} // namespace _openvdbmodule


////////////////////////////////////////


#ifdef DWA_OPENVDB
#define PY_OPENVDB_MODULE_NAME  _openvdb
extern "C" { void init_openvdb(); }
#else
#define PY_OPENVDB_MODULE_NAME  pyopenvdb
extern "C" { void initpyopenvdb(); }
#endif

BOOST_PYTHON_MODULE(PY_OPENVDB_MODULE_NAME)
{
    // Don't auto-generate ugly, C++-style function signatures.
    py::docstring_options docOptions;
    docOptions.disable_signatures();
    docOptions.enable_user_defined();

#ifdef PY_OPENVDB_USE_NUMPY
    // Initialize NumPy.
#ifdef PY_OPENVDB_USE_BOOST_PYTHON_NUMPY
    boost::python::numpy::initialize();
#else
#if PY_MAJOR_VERSION >= 3
    if (_import_array()) {}
#else
    import_array();
#endif
#endif
#endif

    using namespace laovdb::OPENVDB_VERSION_NAME;

    // Initialize OpenVDB.
    initialize();
#ifdef PY_OPENVDB_USE_AX
    laovdb::ax::initialize();
#endif
    _openvdbmodule::CoordConverter::registerConverter();

    _openvdbmodule::VecConverter<Vec2i>::registerConverter();
    _openvdbmodule::VecConverter<Vec2I>::registerConverter();
    _openvdbmodule::VecConverter<Vec2s>::registerConverter();
    _openvdbmodule::VecConverter<Vec2d>::registerConverter();

    _openvdbmodule::VecConverter<Vec3i>::registerConverter();
    _openvdbmodule::VecConverter<Vec3I>::registerConverter();
    _openvdbmodule::VecConverter<Vec3s>::registerConverter();
    _openvdbmodule::VecConverter<Vec3d>::registerConverter();

    _openvdbmodule::VecConverter<Vec4i>::registerConverter();
    _openvdbmodule::VecConverter<Vec4I>::registerConverter();
    _openvdbmodule::VecConverter<Vec4s>::registerConverter();
    _openvdbmodule::VecConverter<Vec4d>::registerConverter();

    _openvdbmodule::MatConverter<Mat4s>::registerConverter();
    _openvdbmodule::MatConverter<Mat4d>::registerConverter();

    _openvdbmodule::PointIndexConverter<PointDataIndex32>::registerConverter();

    _openvdbmodule::MetaMapConverter::registerConverter();

#define PYOPENVDB_TRANSLATE_EXCEPTION(_classname) \
    py::register_exception_translator<_classname>(&_openvdbmodule::translateException<_classname>)

    PYOPENVDB_TRANSLATE_EXCEPTION(ArithmeticError);
    PYOPENVDB_TRANSLATE_EXCEPTION(IndexError);
    PYOPENVDB_TRANSLATE_EXCEPTION(IoError);
    PYOPENVDB_TRANSLATE_EXCEPTION(KeyError);
    PYOPENVDB_TRANSLATE_EXCEPTION(LookupError);
    PYOPENVDB_TRANSLATE_EXCEPTION(NotImplementedError);
    PYOPENVDB_TRANSLATE_EXCEPTION(ReferenceError);
    PYOPENVDB_TRANSLATE_EXCEPTION(RuntimeError);
    PYOPENVDB_TRANSLATE_EXCEPTION(TypeError);
    PYOPENVDB_TRANSLATE_EXCEPTION(ValueError);

#undef PYOPENVDB_TRANSLATE_EXCEPTION


    // Export the python bindings.
    exportTransform();
    exportMetadata();
    exportFloatGrid();
    exportIntGrid();
    exportVec3Grid();
    exportPointGrid();


    py::def("read",
        &_openvdbmodule::readFromFile,
        (py::arg("filename"), py::arg("gridname")),
        "read(filename, gridname) -> Grid\n\n"
        "Read a single grid from a .vdb file.");

#ifdef PY_OPENVDB_USE_AX
    py::def("ax",
        &_openvdbmodule::axrun,
        (py::arg("code"), py::arg("grids")),
        "ax(code, grids) -> Grid\n\n"
        "Run AX code on some VDB grids.");
#endif

    py::def("readAll",
        &_openvdbmodule::readAllFromFile,
        py::arg("filename"),
        "readAll(filename) -> list, dict\n\n"
        "Read a .vdb file and return a list of grids and\n"
        "a dict of file-level metadata.");

    py::def("readMetadata",
        &_openvdbmodule::readFileMetadata,
        py::arg("filename"),
        "readMetadata(filename) -> dict\n\n"
        "Read file-level metadata from a .vdb file.");

    py::def("readGridMetadata",
        &_openvdbmodule::readGridMetadataFromFile,
        (py::arg("filename"), py::arg("gridname")),
        "readGridMetadata(filename, gridname) -> Grid\n\n"
        "Read a single grid's metadata and transform (but not its tree)\n"
        "from a .vdb file.");

    py::def("readAllGridMetadata",
        &_openvdbmodule::readAllGridMetadataFromFile,
        py::arg("filename"),
        "readAllGridMetadata(filename) -> list\n\n"
        "Read a .vdb file and return a list of grids populated with\n"
        "their metadata and transforms, but not their trees.");

    py::def("write",
        &_openvdbmodule::writeToFile,
        (py::arg("filename"), py::arg("grids"), py::arg("metadata") = py::object()),
        "write(filename, grids, metadata=None)\n\n"
        "Write a grid or a sequence of grids and, optionally, a dict\n"
        "of (name, value) metadata pairs to a .vdb file.");

    py::def("getLoggingLevel", &_openvdbmodule::getLoggingLevel,
        "getLoggingLevel() -> str\n\n"
        "Return the severity threshold (\"debug\", \"info\", \"warn\", \"error\",\n"
        "or \"fatal\") for error messages.");
    py::def("setLoggingLevel", &_openvdbmodule::setLoggingLevel,
        (py::arg("level")),
        "setLoggingLevel(level)\n\n"
        "Specify the severity threshold (\"debug\", \"info\", \"warn\", \"error\",\n"
        "or \"fatal\") for error messages.  Messages of lower severity\n"
        "will be suppressed.");
    py::def("setProgramName", &_openvdbmodule::setProgramName,
        (py::arg("name"), py::arg("color") = true),
        "setProgramName(name, color=True)\n\n"
        "Specify the program name to be displayed in error messages,\n"
        "and optionally specify whether to print error messages in color.");

    // Add some useful module-level constants.
    py::scope().attr("LIBRARY_VERSION") = py::make_tuple(
        laovdb::OPENVDB_LIBRARY_MAJOR_VERSION,
        laovdb::OPENVDB_LIBRARY_MINOR_VERSION,
        laovdb::OPENVDB_LIBRARY_PATCH_VERSION);
    py::scope().attr("FILE_FORMAT_VERSION") = laovdb::OPENVDB_FILE_VERSION;
    py::scope().attr("COORD_MIN") = laovdb::Coord::min();
    py::scope().attr("COORD_MAX") = laovdb::Coord::max();
    py::scope().attr("LEVEL_SET_HALF_WIDTH") = laovdb::LEVEL_SET_HALF_WIDTH;

    pyutil::StringEnum<_openvdbmodule::GridClassDescr>::wrap();
    pyutil::StringEnum<_openvdbmodule::VecTypeDescr>::wrap();

} // BOOST_PYTHON_MODULE
