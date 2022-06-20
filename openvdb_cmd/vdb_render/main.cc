// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0
//
/// @file main.cc
///
/// @brief Simple ray tracer for OpenVDB volumes
///
/// @note This is intended mainly as an example of how to ray-trace
/// OpenVDB volumes.  It is not a production-quality renderer.

#include <openvdb/openvdb.h>
#include <openvdb/tools/RayIntersector.h>
#include <openvdb/tools/RayTracer.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>

#ifdef OPENVDB_USE_EXR
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfPixelType.h>
#endif

#ifdef OPENVDB_USE_PNG
#include <png.h>
#endif

// tbb/task_scheduler_init.h was removed in TBB 2021. The best construct to swap
// to is tbb/global_control (for executables). global_control was only officially
// added in TBB 2019U4 but exists in 2018 as a preview feature. To avoid more
// compile time branching (as we still support 2018), we use it in 2018 too by
// enabling the below define.
#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include <tbb/global_control.h>
#include <tbb/tick_count.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const char* gProgName = "";

const double LIGHT_DEFAULTS[] = { 0.3, 0.3, 0.0, 0.7, 0.7, 0.7 };

static const char* sExtensions = "ppm"
#ifdef OPENVDB_USE_EXR
",exr"
#endif
#ifdef OPENVDB_USE_PNG
",png"
#endif
;

struct RenderOpts
{
    std::string shader;
    std::string color;
    laovdb::Vec3SGrid::Ptr colorgrid;
    std::string camera;
    float aperture, focal, frame, znear, zfar;
    double isovalue;
    laovdb::Vec3d rotate;
    laovdb::Vec3d translate;
    laovdb::Vec3d target;
    laovdb::Vec3d up;
    bool lookat;
    size_t samples;
    laovdb::Vec3d absorb;
    std::vector<double> light;
    laovdb::Vec3d scatter;
    double cutoff, gain;
    laovdb::Vec2d step;
    size_t width, height;
    std::string compression;
    int threads;
    bool verbose;

    RenderOpts():
        shader("diffuse"),
        camera("perspective"),
        aperture(41.2136f),
        focal(50.0f),
        frame(1.0f),
        znear(1.0e-3f),
        zfar(std::numeric_limits<float>::max()),
        isovalue(0.0),
        rotate(0.0),
        translate(0.0),
        target(0.0),
        up(0.0, 1.0, 0.0),
        lookat(false),
        samples(1),
        absorb(0.1),
        light(LIGHT_DEFAULTS, LIGHT_DEFAULTS + 6),
        scatter(1.5),
        cutoff(0.005),
        gain(0.2),
        step(1.0, 3.0),
        width(1920),
        height(1080),
        compression("zip"),
        threads(0),
        verbose(false)
    {}

    std::string validate() const
    {
        if (shader != "diffuse" && shader != "matte" && shader != "normal" && shader != "position"){
            return "expected diffuse, matte, normal or position shader, got \"" + shader + "\"";
        }
        if (!boost::starts_with(camera, "ortho") && !boost::starts_with(camera, "persp")) {
            return "expected perspective or orthographic camera, got \"" + camera + "\"";
        }
        if (compression != "none" && compression != "rle" && compression != "zip") {
            return "expected none, rle or zip compression, got \"" + compression + "\"";
        }
        if (width < 1 || height < 1) {
            std::ostringstream ostr;
            ostr << "expected width > 0 and height > 0, got " << width << "x" << height;
            return ostr.str();
        }
        return "";
    }

    std::ostream& put(std::ostream& os) const
    {
        os << " -absorb " << absorb[0] << "," << absorb[1] << "," << absorb[2]
           << " -aperture " << aperture
           << " -camera " << camera;
        if (!color.empty()) os << " -color '" << color << "'";
        os << " -compression " << compression
           << " -cpus " << threads
           << " -cutoff " << cutoff
           << " -far " << zfar
           << " -focal " << focal
           << " -frame " << frame
           << " -gain " << gain
           << " -isovalue " << isovalue
           << " -light " << light[0] << "," << light[1] << "," << light[2]
               << "," << light[3] << "," << light[4] << "," << light[5];
        if (lookat) os << " -lookat " << target[0] << "," << target[1] << "," << target[2];
        os << " -near " << znear
           << " -res " << width << "x" << height;
        if (!lookat) os << " -rotate " << rotate[0] << "," << rotate[1] << "," << rotate[2];
        os << " -shader " << shader
           << " -samples " << samples
           << " -scatter " << scatter[0] << "," << scatter[1] << "," << scatter[2]
           << " -shadowstep " << step[1]
           << " -step " << step[0]
           << " -translate " << translate[0] << "," << translate[1] << "," << translate[2];
        if (lookat) os << " -up " << up[0] << "," << up[1] << "," << up[2];
        if (verbose) os << " -v";
        return os;
    }
};

std::ostream& operator<<(std::ostream& os, const RenderOpts& opts) { return opts.put(os); }

void
usage [[noreturn]] (int exitStatus = EXIT_FAILURE)
{
    RenderOpts opts; // default options
    const double fov = laovdb::tools::PerspectiveCamera::focalLengthToFieldOfView(
        opts.focal, opts.aperture);

    std::ostringstream ostr;
    ostr << std::setprecision(3) <<
"Usage: " << gProgName << " in.vdb out.{" << sExtensions << "} [options]\n" <<
"Which: ray-traces OpenVDB volumes\n" <<
"Options:\n" <<
"    -aperture F       perspective camera aperture in mm (default: " << opts.aperture << ")\n" <<
"    -camera S         camera type; either \"persp[ective]\" or \"ortho[graphic]\"\n" <<
"                      (default: " << opts.camera << ")\n" <<
#ifdef OPENVDB_USE_EXR
"    -compression S    EXR compression scheme; either \"none\" (uncompressed),\n" <<
"                      \"rle\" or \"zip\" (default: " << opts.compression << ")\n" <<
#endif
"    -cpus N           number of rendering threads, or 1 to disable threading,\n" <<
"                      or 0 to use all available CPUs (default: " << opts.threads << ")\n" <<
"    -far F            camera far plane depth (default: " << opts.zfar << ")\n" <<
"    -focal F          perspective camera focal length in mm (default: " << opts.focal << ")\n" <<
"    -fov F            perspective camera field of view in degrees\n" <<
"                      (default: " << fov << ")\n" <<
"    -frame F          ortho camera frame width in world units (default: " <<
    opts.frame << ")\n" <<
"    -lookat X,Y,Z     rotate the camera to point to (X, Y, Z)\n" <<
"    -name S           name of the volume to be rendered (default: render\n" <<
"                      the first floating-point volume found in in.vdb)\n" <<
"    -near F           camera near plane depth (default: " << opts.znear << ")\n" <<
"    -res WxH          image dimensions in pixels (default: " <<
    opts.width << "x" << opts.height << ")\n" <<
"    -r X,Y,Z                                    \n" <<
"    -rotate X,Y,Z     camera rotation in degrees\n" <<
"                      (default: look at the center of the volume)\n" <<
"    -t X,Y,Z                            \n" <<
"    -translate X,Y,Z  camera translation\n" <<
"    -up X,Y,Z         vector that should point up after rotation with -lookat\n" <<
"                      (default: " << opts.up << ")\n" <<
"\n" <<
"    -v                verbose (print timing and diagnostics)\n" <<
"    -version          print version information and exit\n" <<
"    -h, -help         print this usage message and exit\n" <<
"\n" <<
"Level set options:\n" <<
"    -color S          name of a vec3s volume to be used to set material colors\n" <<
"    -isovalue F       isovalue in world units for level set ray intersection\n" <<
"                      (default: " << opts.isovalue << ")\n" <<
"    -samples N        number of samples (rays) per pixel (default: " << opts.samples << ")\n" <<
"    -shader S         shader name; either \"diffuse\", \"matte\", \"normal\"\n" <<
"                      or \"position\" (default: " << opts.shader << ")\n" <<
"\n" <<
"Dense volume options:\n" <<
"    -absorb R,G,B     absorption coefficients (default: " << opts.absorb << ")\n" <<
"    -cutoff F         density and transmittance cutoff value (default: " << opts.cutoff << ")\n" <<
"    -gain F           amount of scatter along the shadow ray (default: " << opts.gain << ")\n" <<
"    -light X,Y,Z[,R,G,B]  light source direction and optional color\n" <<
"                      (default: [" << opts.light[0] << ", " << opts.light[1]
    << ", " << opts.light[2] << ", " << opts.light[3] << ", " << opts.light[4]
    << ", " << opts.light[5] << "])\n" <<
"    -scatter R,G,B    scattering coefficients (default: " << opts.scatter << ")\n" <<
"    -shadowstep F     step size in voxels for integration along the shadow ray\n" <<
"                      (default: " << opts.step[1] << ")\n" <<
"    -step F           step size in voxels for integration along the primary ray\n" <<
"                      (default: " << opts.step[0] << ")\n" <<
"\n" <<
"Examples:\n" <<
"    " << gProgName << " crawler.vdb crawler.{" << sExtensions << "} -shader diffuse -res 1920x1080 \\\n" <<
"        -focal 35 -samples 4 -translate 0,210.5,400 -compression rle -v\n" <<
"\n" <<
"    " << gProgName << " bunny_cloud.vdb bunny_cloud.{" << sExtensions << "} -res 1920x1080 \\\n" <<
"        -translate 0,0,110 -absorb 0.4,0.2,0.1 -gain 0.2 -v\n" <<
"\n" <<
"Warning:\n" <<
"     This is not (and is not intended to be) a production-quality renderer.\n" <<
"     Use it for fast previewing or simply as a reference implementation\n" <<
"     for integration into existing ray tracers.\n";

    std::cerr << ostr.str();
    exit(exitStatus);
}

inline void isExtensionSupported(const std::string& fname)
{
    // .ppm always supported
    if (boost::iends_with(fname, ".ppm")) return;
    else if (boost::iends_with(fname, ".exr"))
    {
#ifndef OPENVDB_USE_EXR
        OPENVDB_THROW(laovdb::RuntimeError,
            "vdb_render has not been compiled with .exr support.");
#endif
    }
    else if (boost::iends_with(fname, ".png"))
    {
#ifndef OPENVDB_USE_PNG
        OPENVDB_THROW(laovdb::RuntimeError,
            "vdb_render has not been compiled with .png support.");
#endif
    }
    else {
        OPENVDB_THROW(laovdb::ValueError,
            "unsupported image file format (" + fname + ")");
    }
}

#ifdef OPENVDB_USE_EXR
void
saveEXR(const std::string& fname, const laovdb::tools::Film& film, const RenderOpts& opts)
{
    using RGBA = laovdb::tools::Film::RGBA;

    std::string filename = fname;
    if (!boost::iends_with(filename, ".exr")) filename += ".exr";

    if (opts.verbose) {
        std::cout << gProgName << ": writing " << filename << "..." << std::endl;
    }

    const tbb::tick_count start = tbb::tick_count::now();

    int threads = (opts.threads == 0 ? 8 : opts.threads);
    Imf::setGlobalThreadCount(threads);

    Imf::Header header(int(film.width()), int(film.height()));
    if (opts.compression == "none") {
        header.compression() = Imf::NO_COMPRESSION;
    } else if (opts.compression == "rle") {
        header.compression() = Imf::RLE_COMPRESSION;
    } else if (opts.compression == "zip") {
        header.compression() = Imf::ZIP_COMPRESSION;
    } else {
        OPENVDB_THROW(laovdb::ValueError,
            "expected none, rle or zip compression, got \"" << opts.compression << "\"");
    }
    header.channels().insert("R", Imf::Channel(Imf::FLOAT));
    header.channels().insert("G", Imf::Channel(Imf::FLOAT));
    header.channels().insert("B", Imf::Channel(Imf::FLOAT));
    header.channels().insert("A", Imf::Channel(Imf::FLOAT));

    const size_t pixelBytes = sizeof(RGBA), rowBytes = pixelBytes * film.width();
    RGBA& pixel0 = const_cast<RGBA*>(film.pixels())[0];
    Imf::FrameBuffer framebuffer;
    framebuffer.insert("R",
        Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&pixel0.r), pixelBytes, rowBytes));
    framebuffer.insert("G",
        Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&pixel0.g), pixelBytes, rowBytes));
    framebuffer.insert("B",
        Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&pixel0.b), pixelBytes, rowBytes));
    framebuffer.insert("A",
        Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&pixel0.a), pixelBytes, rowBytes));

    Imf::OutputFile imgFile(filename.c_str(), header);
    imgFile.setFrameBuffer(framebuffer);
    imgFile.writePixels(int(film.height()));

    if (opts.verbose) {
        std::ostringstream ostr;
        ostr << gProgName << ": ...completed in " << std::setprecision(3)
            << (tbb::tick_count::now() - start).seconds() << " sec";
        std::cout << ostr.str() << std::endl;
    }
}
#else
void
saveEXR(const std::string&, const laovdb::tools::Film&, const RenderOpts&)
{
    OPENVDB_THROW(laovdb::RuntimeError,
        "vdb_render has not been compiled with .exr support.");
}
#endif

#ifdef OPENVDB_USE_PNG
struct PngWriter
{
    PngWriter() = default;
    ~PngWriter() { this->reset(); }

    inline void write(const std::string& fname, const laovdb::tools::Film& film)
    {
        png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) OPENVDB_THROW(laovdb::RuntimeError, "png_create_write_struct failed");
        info = png_create_info_struct(png);
        if (!info) OPENVDB_THROW(laovdb::RuntimeError, "png_create_info_struct failed.")

        fp = std::fopen(fname.c_str(), "wb");
        if (!fp) {
            OPENVDB_THROW(laovdb::IoError,
                "Unable to open '" + fname + "' for writing");
        }

        if (setjmp(png_jmpbuf(png))) {
            OPENVDB_THROW(laovdb::IoError, "Error during initialization of PNG I/O.");
        }
        png_init_io(png, fp);

        if (setjmp(png_jmpbuf(png))) {
            OPENVDB_THROW(laovdb::IoError, "Error writing PNG file header.");
        }
        // Output is 8bit depth, RGB format.
        png_set_IHDR(png, info,
            int(film.width()), int(film.height()),
            8,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        const size_t channels = 3; // 3 = RGB, 4 = RGBA
        auto buffer = film.convertToBitBuffer<uint8_t>(/*alpha=*/false);
        uint8_t* tmp = buffer.get();

        std::unique_ptr<png_bytep[]>
            row_pointers(new png_bytep[film.height()]);

        for (size_t row = 0; row < film.height(); row++) {
            row_pointers[row] = tmp + (row * film.width() * channels);
        }

        if (setjmp(png_jmpbuf(png))) {
            OPENVDB_THROW(laovdb::IoError, "Error writing PNG data buffers.");
        }
        /* write out the entire image data in one call */
        png_write_image(png, row_pointers.get());
        png_write_end(png, nullptr);

        this->reset();
    }

    inline void reset()
    {
        if (fp)  std::fclose(fp);
        if (png) png_destroy_write_struct(&png, &info);
        png = nullptr;
        info = nullptr;
        fp = nullptr;
    }

private:
    FILE* fp = nullptr;
    png_structp png = nullptr;
    png_infop info = nullptr;
};
#else
struct PngWriter {
    inline void write(const std::string&, const laovdb::tools::Film&) {
        OPENVDB_THROW(laovdb::RuntimeError,
            "vdb_render has not been compiled with .png support.");
    }
};
#endif

template<typename GridType>
void
render(const GridType& grid, const std::string& imgFilename, const RenderOpts& opts)
{
    using namespace laovdb;

    const bool isLevelSet = (grid.getGridClass() == GRID_LEVEL_SET);

    tools::Film film(opts.width, opts.height);

    std::unique_ptr<tools::BaseCamera> camera;
    if (boost::starts_with(opts.camera, "persp")) {
        camera.reset(new tools::PerspectiveCamera(film, opts.rotate, opts.translate,
            opts.focal, opts.aperture, opts.znear, opts.zfar));
    } else if (boost::starts_with(opts.camera, "ortho")) {
        camera.reset(new tools::OrthographicCamera(film, opts.rotate, opts.translate,
            opts.frame, opts.znear, opts.zfar));
    } else {
        OPENVDB_THROW(ValueError,
            "expected perspective or orthographic camera, got \"" << opts.camera << "\"");
    }
    if (opts.lookat) camera->lookAt(opts.target, opts.up);

    // Define the shader for level set rendering.  The default shader is a diffuse shader.
    std::unique_ptr<tools::BaseShader> shader;
    if (opts.shader == "matte") {
        if (opts.colorgrid) {
            shader.reset(new tools::MatteShader<laovdb::Vec3SGrid>(*opts.colorgrid));
        } else {
            shader.reset(new tools::MatteShader<>());
        }
    } else if (opts.shader == "normal") {
        if (opts.colorgrid) {
            shader.reset(new tools::NormalShader<Vec3SGrid>(*opts.colorgrid));
        } else {
            shader.reset(new tools::NormalShader<>());
        }
    } else if (opts.shader == "position") {
        const CoordBBox bbox = grid.evalActiveVoxelBoundingBox();
        const math::BBox<Vec3d> bboxIndex(bbox.min().asVec3d(), bbox.max().asVec3d());
        const math::BBox<Vec3R> bboxWorld = bboxIndex.applyMap(*(grid.transform().baseMap()));
        if (opts.colorgrid) {
            shader.reset(new tools::PositionShader<Vec3SGrid>(bboxWorld, *opts.colorgrid));
        } else {
            shader.reset(new tools::PositionShader<>(bboxWorld));
        }
    } else /* if (opts.shader == "diffuse") */ { // default
        if (opts.colorgrid) {
            shader.reset(new tools::DiffuseShader<Vec3SGrid>(*opts.colorgrid));
        } else {
            shader.reset(new tools::DiffuseShader<>());
        }
    }

    if (opts.verbose) {
        std::cout << gProgName << ": ray-tracing";
        const std::string gridName = grid.getName();
        if (!gridName.empty()) std::cout << " " << gridName;
        std::cout << "..." << std::endl;
    }
    const tbb::tick_count start = tbb::tick_count::now();

    if (isLevelSet) {
        tools::LevelSetRayIntersector<GridType> intersector(
            grid, static_cast<typename GridType::ValueType>(opts.isovalue));
        tools::rayTrace(grid, intersector, *shader, *camera, opts.samples,
            /*seed=*/0, (opts.threads != 1));
    } else {
        using IntersectorType = tools::VolumeRayIntersector<GridType>;
        IntersectorType intersector(grid);

        tools::VolumeRender<IntersectorType> renderer(intersector, *camera);
        renderer.setLightDir(opts.light[0], opts.light[1], opts.light[2]);
        renderer.setLightColor(opts.light[3], opts.light[4], opts.light[5]);
        renderer.setPrimaryStep(opts.step[0]);
        renderer.setShadowStep(opts.step[1]);
        renderer.setScattering(opts.scatter[0], opts.scatter[1], opts.scatter[2]);
        renderer.setAbsorption(opts.absorb[0], opts.absorb[1], opts.absorb[2]);
        renderer.setLightGain(opts.gain);
        renderer.setCutOff(opts.cutoff);

        renderer.render(opts.threads != 1);
    }

    if (opts.verbose) {
        std::ostringstream ostr;
        ostr << gProgName << ": ...completed in " << std::setprecision(3)
            << (tbb::tick_count::now() - start).seconds() << " sec";
        std::cout << ostr.str() << std::endl;
    }

    if (boost::iends_with(imgFilename, ".ppm")) {
        // Save as PPM (fast, but large file size).
        std::string filename = imgFilename;
        filename.erase(filename.size() - 4); // strip .ppm extension
        film.savePPM(filename);
    } else if (boost::iends_with(imgFilename, ".exr")) {
        // Save as EXR (slow, but small file size).
        saveEXR(imgFilename, film, opts);
    } else if (boost::iends_with(imgFilename, ".png")) {
        PngWriter png;
        png.write(imgFilename, film);
    } else {
        OPENVDB_THROW(ValueError, "unsupported image file format (" + imgFilename + ")");
    }
}


void
strToSize(const std::string& s, size_t& x, size_t& y)
{
    std::vector<std::string> elems;
    boost::split(elems, s, boost::algorithm::is_any_of(",x"));
    const size_t numElems = elems.size();
    if (numElems > 0) x = size_t(std::max(0, atoi(elems[0].c_str())));
    if (numElems > 1) y = size_t(std::max(0, atoi(elems[1].c_str())));
}


std::vector<double>
strToVec(const std::string& s)
{
    std::vector<double> result;
    std::vector<std::string> elems;
    boost::split(elems, s, boost::algorithm::is_any_of(","));
    for (size_t i = 0, N = elems.size(); i < N; ++i) {
        result.push_back(atof(elems[i].c_str()));
    }
    return result;
}


laovdb::Vec3d
strToVec3d(const std::string& s)
{
    laovdb::Vec3d result(0.0, 0.0, 0.0);
    std::vector<double> elems = strToVec(s);
    if (!elems.empty()) {
        result = laovdb::Vec3d(elems[0]);
        for (int i = 1, N = std::min(3, int(elems.size())); i < N; ++i) {
            result[i] = elems[i];
        }
    }
    return result;
}


struct OptParse
{
    int argc;
    char** argv;

    OptParse(int argc_, char* argv_[]): argc(argc_), argv(argv_) {}

    bool check(int idx, const std::string& name, int numArgs = 1) const
    {
        if (argv[idx] == name) {
            if (idx + numArgs >= argc) {
                OPENVDB_LOG_FATAL("option " << name << " requires "
                    << numArgs << " argument" << (numArgs == 1 ? "" : "s"));
                usage();
            }
            return true;
        }
        return false;
    }
};

} // unnamed namespace


int
main(int argc, char *argv[])
{
    OPENVDB_START_THREADSAFE_STATIC_WRITE
    gProgName = argv[0];
    if (const char* ptr = ::strrchr(gProgName, '/')) gProgName = ptr + 1;
    OPENVDB_FINISH_THREADSAFE_STATIC_WRITE

    int retcode = EXIT_SUCCESS;

    if (argc == 1) usage();

    laovdb::logging::initialize(argc, argv);

    std::string vdbFilename, imgFilename, gridName;
    RenderOpts opts;

    bool hasFocal = false, hasFov = false, hasRotate = false, hasLookAt = false;
    float fov = 0.0;

    OptParse parser(argc, argv);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-') {
            if (parser.check(i, "-absorb")) {
                ++i;
                opts.absorb = strToVec3d(argv[i]);
            } else if (parser.check(i, "-aperture")) {
                ++i;
                opts.aperture = float(atof(argv[i]));
            } else if (parser.check(i, "-camera")) {
                ++i;
                opts.camera = argv[i];
            } else if (parser.check(i, "-color")) {
                ++i;
                opts.color = argv[i];
            } else if (parser.check(i, "-compression")) {
                ++i;
                opts.compression = argv[i];
            } else if (parser.check(i, "-cpus")) {
                ++i;
                opts.threads = std::max(0, atoi(argv[i]));
            } else if (parser.check(i, "-cutoff")) {
                ++i;
                opts.cutoff = atof(argv[i]);
            } else if (parser.check(i, "-isovalue")) {
                ++i;
                opts.isovalue = atof(argv[i]);
            } else if (parser.check(i, "-far")) {
                ++i;
                opts.zfar = float(atof(argv[i]));
            } else if (parser.check(i, "-focal")) {
                ++i;
                opts.focal = float(atof(argv[i]));
                hasFocal = true;
            } else if (parser.check(i, "-fov")) {
                ++i;
                fov = float(atof(argv[i]));
                hasFov = true;
            } else if (parser.check(i, "-frame")) {
                ++i;
                opts.frame = float(atof(argv[i]));
            } else if (parser.check(i, "-gain")) {
                ++i;
                opts.gain = atof(argv[i]);
            } else if (parser.check(i, "-light")) {
                ++i;
                opts.light = strToVec(argv[i]);
            } else if (parser.check(i, "-lookat")) {
                ++i;
                opts.lookat = true;
                opts.target = strToVec3d(argv[i]);
                hasLookAt = true;
            } else if (parser.check(i, "-name")) {
                ++i;
                gridName = argv[i];
            } else if (parser.check(i, "-near")) {
                ++i;
                opts.znear = float(atof(argv[i]));
            } else if (parser.check(i, "-r") || parser.check(i, "-rotate")) {
                ++i;
                opts.rotate = strToVec3d(argv[i]);
                hasRotate = true;
            } else if (parser.check(i, "-res")) {
                ++i;
                strToSize(argv[i], opts.width, opts.height);
            } else if (parser.check(i, "-scatter")) {
                ++i;
                opts.scatter = strToVec3d(argv[i]);
            } else if (parser.check(i, "-shader")) {
                ++i;
                opts.shader = argv[i];
            } else if (parser.check(i, "-shadowstep")) {
                ++i;
                opts.step[1] = atof(argv[i]);
            } else if (parser.check(i, "-samples")) {
                ++i;
                opts.samples = size_t(std::max(0, atoi(argv[i])));
            } else if (parser.check(i, "-step")) {
                ++i;
                opts.step[0] = atof(argv[i]);
            } else if (parser.check(i, "-t") || parser.check(i, "-translate")) {
                ++i;
                opts.translate = strToVec3d(argv[i]);
            } else if (parser.check(i, "-up")) {
                ++i;
                opts.up = strToVec3d(argv[i]);
            } else if (arg == "-v") {
                opts.verbose = true;
            } else if (arg == "-version" || arg == "--version") {
                std::cout << "OpenVDB library version: "
                    << laovdb::getLibraryAbiVersionString() << "\n";
                std::cout << "OpenVDB file format version: "
                    << laovdb::OPENVDB_FILE_VERSION << std::endl;
                return EXIT_SUCCESS;
            } else if (arg == "-h" || arg == "-help" || arg == "--help") {
                usage(EXIT_SUCCESS);
            } else {
                OPENVDB_LOG_FATAL("\"" << arg << "\" is not a valid option");
                usage();
            }
        } else if (vdbFilename.empty()) {
            vdbFilename = arg;
        } else if (imgFilename.empty()) {
            imgFilename = arg;
        } else {
            usage();
        }
    }
    if (vdbFilename.empty() || imgFilename.empty()) {
        usage();
    }
    if (hasFov) {
        if (hasFocal) {
            OPENVDB_LOG_FATAL("specify -focal or -fov, but not both");
            usage();
        }
        opts.focal = float(
            laovdb::tools::PerspectiveCamera::fieldOfViewToFocalLength(fov, opts.aperture));
    }
    if (hasLookAt && hasRotate) {
        OPENVDB_LOG_FATAL("specify -lookat or -r[otate], but not both");
        usage();
    }
    {
        const std::string err = opts.validate();
        if (!err.empty()) {
            OPENVDB_LOG_FATAL(err);
            usage();
        }
    }

    try {
        // throw if an extension has been set but we don't support it
        isExtensionSupported(imgFilename);

        std::unique_ptr<tbb::global_control> control;
        if (opts.threads > 0) {
            // note, opts.threads == 0 means use all threads (default), so don't
            // manually create a tbb::global_control in this case
            control.reset(new tbb::global_control(tbb::global_control::max_allowed_parallelism, opts.threads));
        }

        laovdb::initialize();

        const tbb::tick_count start = tbb::tick_count::now();
        if (opts.verbose) {
            std::cout << gProgName << ": reading ";
            if (!gridName.empty()) std::cout << gridName << " from ";
            std::cout << vdbFilename << "..." << std::endl;
        }

        laovdb::FloatGrid::Ptr grid;
        {
            laovdb::io::File file(vdbFilename);

            if (!gridName.empty()) {
                file.open();
                grid = laovdb::gridPtrCast<laovdb::FloatGrid>(file.readGrid(gridName));
                if (!grid) {
                    OPENVDB_THROW(laovdb::ValueError,
                        gridName + " is not a scalar, floating-point volume");
                }
            } else {
                // If no grid was specified by name, retrieve the first float grid from the file.
                file.open(/*delayLoad=*/false);
                laovdb::io::File::NameIterator it = file.beginName();
                laovdb::GridPtrVecPtr grids = file.readAllGridMetadata();
                for (size_t i = 0; i < grids->size(); ++i, ++it) {
                    grid = laovdb::gridPtrCast<laovdb::FloatGrid>(grids->at(i));
                    if (grid) {
                        gridName = *it;
                        file.close();
                        file.open();
                        grid = laovdb::gridPtrCast<laovdb::FloatGrid>(file.readGrid(gridName));
                        break;
                    }
                }
                if (!grid) {
                    OPENVDB_THROW(laovdb::ValueError,
                        "no scalar, floating-point volumes in file " + vdbFilename);
                }
            }

            if (!opts.color.empty()) {
                opts.colorgrid =
                    laovdb::gridPtrCast<laovdb::Vec3SGrid>(file.readGrid(opts.color));
                if (!opts.colorgrid) {
                    OPENVDB_THROW(laovdb::ValueError,
                        opts.color + " is not a vec3s color volume");
                }
            }
        }

        if (opts.verbose) {
            std::ostringstream ostr;
            ostr << gProgName << ": ...completed in " << std::setprecision(3)
                << (tbb::tick_count::now() - start).seconds() << " sec";
            std::cout << ostr.str() << std::endl;
        }

        if (grid) {
            if (!hasLookAt && !hasRotate) {
                // If the user specified neither the camera rotation nor a target
                // to look at, orient the camera to point to the center of the grid.
                opts.target = grid->evalActiveVoxelBoundingBox().getCenter();
                opts.target = grid->constTransform().indexToWorld(opts.target);
                opts.lookat = true;
            }

            if (opts.verbose) std::cout << opts << std::endl;

            render<laovdb::FloatGrid>(*grid, imgFilename, opts);
        }
    } catch (std::exception& e) {
        OPENVDB_LOG_FATAL(e.what());
        retcode = EXIT_FAILURE;
    } catch (...) {
        OPENVDB_LOG_FATAL("Exception caught (unexpected type)");
        std::terminate();
    }

    return retcode;
}
