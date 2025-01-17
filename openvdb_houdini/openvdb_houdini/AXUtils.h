// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

/// @file ax/HoudiniAXUtils.h
///
/// @authors Nick Avramoussis
///
/// @brief  Utility methods for OpenVDB AX in Houdini, contains
///   VEX and channel expression conversion methods.
///

#ifndef OPENVDB_AX_HOUDINI_AX_UTILS_HAS_BEEN_INCLUDED
#define OPENVDB_AX_HOUDINI_AX_UTILS_HAS_BEEN_INCLUDED

#include <openvdb_ax/ast/AST.h>
#include <openvdb_ax/ast/Visitor.h>
#include <openvdb_ax/ast/Scanners.h>
#include <openvdb_ax/codegen/FunctionTypes.h>
#include <openvdb_ax/codegen/Functions.h>
#include <openvdb_ax/codegen/Utils.h>
#include <openvdb_ax/compiler/Compiler.h>
#include <openvdb_ax/compiler/CustomData.h>
#include <openvdb_ax/compiler/CompilerOptions.h>

#include <openvdb/Types.h>
#include <openvdb/Metadata.h>
#include <openvdb/Exceptions.h>
#include <openvdb/openvdb.h>

#include <UT/UT_Ramp.h>

#include <map>
#include <set>
#include <utility>
#include <string>

namespace openvdb_ax_houdini
{

enum class TargetType
{
    POINTS,
    VOLUMES,
    LOCAL
};

/// @brief Holds the name (path) and type of a channel expression. Note that
///        this cannot be a core AX Type enum as we additionally support
///        Houdini ramps as custom data.
using ChannelExpressionPair = std::pair<std::string, std::string>;
/// @brief Typedef for a unique set of channel expression
using ChannelExpressionSet = std::set<ChannelExpressionPair>;

/// @brief  Find any Houdini channel expressions represented inside the
///         provided Syntax Tree.
///
/// @param  tree     The AST to parse
/// @param  exprSet  The expression set to populate
inline void findChannelExpressions(const laovdb::ax::ast::Tree& tree,
                            ChannelExpressionSet& exprSet);

/// @brief  Find any Houdini $ expressions represented inside the
///         provided Syntax Tree.
///
/// @param  tree     The AST to parse
/// @param  exprSet  The expression set to populate
inline void findDollarExpressions(const laovdb::ax::ast::Tree& tree,
                            ChannelExpressionSet& exprSet);

/// @brief  Converts a Syntax Tree which contains possible representations of
///         Houdini VEX instructions to internally supported instructions
///
/// @param  tree        The AST to convert
/// @param  targetType  The type of primitive being compiled (this can potentially
///                     alter the generated instructions)
inline void convertASTFromVEX(laovdb::ax::ast::Tree& tree,
                       const TargetType targetType);

/// @brief  Converts external functions within a Syntax Tree to ExternalVariable nodes
///         if the argument is a string literal. This method is much faster than using
///         the external functions but uses $ AX syntax which isn't typical of a Houdini
///         session.
///
/// @param  tree        The AST to convert
/// @param  targetType  The type of primitive being compiled (this can potentially
///                     alter the generated instructions)
inline void convertASTKnownLookups(laovdb::ax::ast::Tree& tree);

/// @brief  Register custom Houdini functions, making them available to the
///         core compiler. These functions generally have specific Houdini only
///         functionality.
/// @param  reg      The function registry to add register the new functions into
/// @param  options The function options
inline void registerCustomHoudiniFunctions(laovdb::ax::codegen::FunctionRegistry& reg,
                                        const laovdb::ax::FunctionOptions* options = nullptr);


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/// @brief AST scanner to find channel expressions and them to the expression set
struct FindChannelExpressions :
    public laovdb::ax::ast::Visitor<FindChannelExpressions>
{
    using laovdb::ax::ast::Visitor<FindChannelExpressions>::traverse;
    using laovdb::ax::ast::Visitor<FindChannelExpressions>::visit;

    FindChannelExpressions(ChannelExpressionSet& expressions)
        : mExpressions(expressions) {}
    ~FindChannelExpressions() = default;

    static const std::string*
    getChannelPath(const laovdb::ax::ast::FunctionCall& call)
    {
        if (call.empty()) return nullptr; // avoid dync if nullptr
        const laovdb::ax::ast::Value<std::string>* const path =
            dynamic_cast<const laovdb::ax::ast::Value<std::string>*>(call.child(0));
        if (!path) return nullptr;
        return &(path->value());
    }

    /// @brief  Add channel expression function calls
    /// @param  node The FunctionCall AST node being visited
    bool visit(const laovdb::ax::ast::FunctionCall* node)
    {
        const std::string& name = node->name();
        if (name.empty()) return true;

        std::string type;

        if (name[0] == 'c') {
            if (name == "ch")          type = laovdb::typeNameAsString<float>();
            else if (name == "chv")    type = laovdb::typeNameAsString<laovdb::Vec3s>();
            else if (name == "chs")    type = laovdb::typeNameAsString<std::string>();
            else if (name == "chramp") type = "ramp";
        }
        else if (name[0] == 'e') {
            if (name == "external")       type = laovdb::typeNameAsString<float>();
            else if (name == "externalv") type = laovdb::typeNameAsString<laovdb::Vec3s>();
            else if (name == "externals") type = laovdb::typeNameAsString<std::string>();
        }

        if (type.empty()) return true;

        // Get channel arguments. If there are incorrect arguments, defer to
        // the compiler code generation function error system to report proper
        // errors later

        const std::string* path = getChannelPath(*node);
        if (path) mExpressions.emplace(type, *path);

        return true;
    }

private:
    ChannelExpressionSet& mExpressions;
};

inline void
findChannelExpressions(const laovdb::ax::ast::Tree& tree,
                       ChannelExpressionSet& exprSet)
{
    FindChannelExpressions op(exprSet);
    op.traverse(&tree);
}


///////////////////////////////////////////////////////////////////////////


inline void findDollarExpressions(const laovdb::ax::ast::Tree& tree,
                                  ChannelExpressionSet& exprSet)
{
    laovdb::ax::ast::visitNodeType<laovdb::ax::ast::ExternalVariable>(tree,
        [&](const laovdb::ax::ast::ExternalVariable& node) -> bool {
            exprSet.emplace(node.typestr(), node.name());
            return true;
        });
}


///////////////////////////////////////////////////////////////////////////

/// @brief AST modifier to convert VEX-like syntax from Houdini to AX.
///        Finds scalar and vector channel expressions and replace with AX custom
///        data lookups. Replaces volume intrinsics @P, @ix, @iy, @iz with AX function
///        calls. In the future this may be used to translate VEX syntax to an AX AST
///        and back to text for in application conversion to AX syntax.
struct ConvertFromVEX :
    public laovdb::ax::ast::Visitor<ConvertFromVEX, false>
{
    using laovdb::ax::ast::Visitor<ConvertFromVEX, false>::traverse;
    using laovdb::ax::ast::Visitor<ConvertFromVEX, false>::visit;

    ConvertFromVEX(const TargetType targetType,
        const std::vector<const laovdb::ax::ast::Variable*>& write)
        : mTargetType(targetType)
        , mWrite(write) {}
    ~ConvertFromVEX() = default;

    /// @brief  Convert channel function calls to internally supported functions
    /// @param  node  The FunctionCall AST node being visited
    bool visit(laovdb::ax::ast::FunctionCall* node)
    {
        const std::string& name = node->name();
        if (name.empty())   return true;
        if (name[0] != 'c') return true;

        std::string identifier;
        if (name == "ch")       identifier = "external";
        else if (name == "chv") identifier = "externalv";
        else if (name == "chs") identifier = "externals";
        else return true;

        laovdb::ax::ast::FunctionCall::UniquePtr
            replacement(new laovdb::ax::ast::FunctionCall(identifier));
        for (size_t i = 0; i < node->children(); ++i) {
            replacement->append(node->child(i)->copy());
        }

        if (!node->replace(replacement.get())) {
            throw std::runtime_error("Unable to convert AX snippet to VEX. Function \"" +
                node->name() + "\" produced errors.");
        }
        replacement.release();
        return true;
    }

    /// @brief  Convert Houdini instrinsic volume attribute read accesses
    /// @param  node  The AttributeValue AST node being visited
    bool visit(laovdb::ax::ast::Attribute* node)
    {
        if (mTargetType != TargetType::VOLUMES) return true;

        const std::string& name = node->name();

        if (name != "P"  && name != "ix" &&
            name != "iy" && name != "iz") {
            return true;
        }

        if (std::find(mWrite.cbegin(), mWrite.cend(), node) != mWrite.cend()) {
            throw std::runtime_error("Unable to write to a volume name \"@" +
                name + "\". This is a keyword identifier");
        }

        laovdb::ax::ast::FunctionCall::UniquePtr replacement;
        if (name == "P") {
            replacement.reset(new laovdb::ax::ast::FunctionCall("getvoxelpws"));
        }
        else if (name == "ix") {
            replacement.reset(new laovdb::ax::ast::FunctionCall("getcoordx"));
        }
        else if (name == "iy") {
            replacement.reset(new laovdb::ax::ast::FunctionCall("getcoordy"));
        }
        else if (name == "iz") {
            replacement.reset(new laovdb::ax::ast::FunctionCall("getcoordz"));
        }

        if (!node->replace(replacement.get())) {
            throw std::runtime_error("Unable to convert AX snippet to VEX. Attribute \"" +
                name + "\" produced errors.");
        }
        replacement.release();
        return true;
    }

private:
    const TargetType mTargetType;
    const std::vector<const laovdb::ax::ast::Variable*>& mWrite;
};

inline void convertASTFromVEX(laovdb::ax::ast::Tree& tree,
                       const TargetType targetType)
{
    std::vector<const laovdb::ax::ast::Variable*> write;
    laovdb::ax::ast::catalogueVariables(tree, nullptr, &write, &write,
        /*locals*/false, /*attributes*/true);
    ConvertFromVEX converter(targetType, write);
    converter.traverse(&tree);
}


///////////////////////////////////////////////////////////////////////////

/// @brief  Convert any external or channel functions to ExternalVariable nodes
///         if the path is a string literal
struct ConvertKnownExternalLookups :
    public laovdb::ax::ast::Visitor<ConvertKnownExternalLookups, false>
{
    using laovdb::ax::ast::Visitor<ConvertKnownExternalLookups, false>::traverse;
    using laovdb::ax::ast::Visitor<ConvertKnownExternalLookups, false>::visit;

    ConvertKnownExternalLookups() = default;
    ~ConvertKnownExternalLookups() = default;

    /// @brief  Performs the function call to external variable conversion
    /// @param  node  The FunctionCall AST node being visited
    bool visit(laovdb::ax::ast::FunctionCall* node)
    {
        const std::string& name = node->name();
        if (name.empty())   return true;
        if (name[0] != 'e') return true;

        laovdb::ax::ast::tokens::CoreType type =
            laovdb::ax::ast::tokens::UNKNOWN;

        if (name == "external")       type = laovdb::ax::ast::tokens::FLOAT;
        else if (name == "externalv") type = laovdb::ax::ast::tokens::VEC3F;
        else if (name == "externals") type = laovdb::ax::ast::tokens::STRING;
        else return true;

        const std::string* path =
            FindChannelExpressions::getChannelPath(*node);

        // If for any reason we couldn't validate or get the channel path from the
        // first argument, fall back to the internal lookup functions. These will
        // error with the expected function argument style results on invalid arguments
        // and, correctly support string attribute arguments provided by s@attribute
        // (although are much slower)

        if (!path) return true;

        laovdb::ax::ast::ExternalVariable::UniquePtr replacement;
        replacement.reset(new laovdb::ax::ast::ExternalVariable(*path, type));
        node->replace(replacement.get());
        replacement.release();

        return true;
    }
};

inline void convertASTKnownLookups(laovdb::ax::ast::Tree& tree)
{
    ConvertKnownExternalLookups converter;
    converter.traverse(&tree);
}


///////////////////////////////////////////////////////////////////////////

/// @brief  Custom derived metadata for ramp channel expressions to be used
///         with codegen::ax::CustomData
struct RampDataCache : public laovdb::Metadata
{
public:
    using RampData = std::map<float, laovdb::math::Vec3<float>>;
    using Ptr = laovdb::SharedPtr<RampDataCache>;
    using ConstPtr = laovdb::SharedPtr<const RampDataCache>;

    RampDataCache() : mData() {}
    virtual ~RampDataCache() {}
    virtual laovdb::Name typeName() const { return str(); }
    virtual laovdb::Metadata::Ptr copy() const {
        laovdb::Metadata::Ptr metadata(new RampDataCache());
        metadata->copy(*this);
        return metadata;
    }
    virtual void copy(const laovdb::Metadata& other) {
        const RampDataCache* t = dynamic_cast<const RampDataCache*>(&other);
        if (t == nullptr) OPENVDB_THROW(laovdb::TypeError, "Incompatible type during copy");
        mData = t->mData;
    }
    virtual std::string str() const { return "<compiler ramp data>"; }
    virtual bool asBool() const { return true; }
    virtual laovdb::Index32 size() const { return 0; }

    //////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////

    UT_Ramp& value() { return mData; }
    const UT_Ramp& value() const { return mData; }

protected:
    virtual void readValue(std::istream&s, laovdb::Index32 numBytes) {
        OPENVDB_THROW(laovdb::TypeError, "Metadata has unknown type");
    }
    virtual void writeValue(std::ostream&) const {
        OPENVDB_THROW(laovdb::TypeError, "Metadata has unknown type");
    }

private:
    UT_Ramp mData;
};

inline laovdb::ax::codegen::FunctionGroup::UniquePtr
hax_chramp(const laovdb::ax::FunctionOptions& op)
{
    static auto sample =
        [](float (*out)[3],
           const char* const name,
           float position,
           const void* const data)
    {
        const laovdb::ax::CustomData* const customData =
            static_cast<const laovdb::ax::CustomData* const>(data);
        const std::string nameString(name);

        const laovdb::Metadata::ConstPtr& meta = customData->getData(nameString);
        assert(meta.get());
        assert(dynamic_cast<const RampDataCache*>(meta.get()));
        const RampDataCache* const rampdata =
            static_cast<const RampDataCache*>(meta.get());

        float fvals[4];
        rampdata->value().getColor(position, fvals);
        std::memcpy(out, fvals, 3*sizeof(float));
    };

    using Sample = void(float(*)[3], const char* const, float,
           const void* const);

    return laovdb::ax::codegen::FunctionBuilder("_chramp")
        .addSignature<Sample>((Sample*)(sample))
        .setArgumentNames({"out", "ramp", "pos", "custom_data"})
        .setConstantFold(false)
        .addParameterAttribute(0, llvm::Attribute::NoAlias)
        .addParameterAttribute(0, llvm::Attribute::WriteOnly)
        .addParameterAttribute(1, llvm::Attribute::ReadOnly)
        .setPreferredImpl(op.mPrioritiseIR ?
            laovdb::ax::codegen::FunctionBuilder::IR :
            laovdb::ax::codegen::FunctionBuilder::C)
        .setDocumentation("Internal function for querying ramp data.")
        .get();
}

inline laovdb::ax::codegen::FunctionGroup::UniquePtr
haxchramp(const laovdb::ax::FunctionOptions& op)
{
    auto generate =
        [op](const std::vector<llvm::Value*>& args,
             llvm::IRBuilder<>& B) -> llvm::Value*
    {
        // Pull out the custom data from the parent function
        llvm::Function* compute = B.GetInsertBlock()->getParent();
        assert(compute);
        llvm::Value* arg = laovdb::ax::codegen::extractArgument(compute, 0);
        assert(arg);
        assert(arg->getName() == "custom_data");

        std::vector<llvm::Value*> inputs(args);
        inputs.emplace_back(arg);

        // call
        hax_chramp(op)->execute(inputs, B);
        return nullptr;
    };

    return laovdb::ax::codegen::FunctionBuilder("chramp")
        .addSignature<void(laovdb::math::Vec3<float>*, char*, float), true>(generate)
        .addDependency("_chramp")
        .setArgumentNames({"ramp", "pos"})
        .addParameterAttribute(0, llvm::Attribute::NoAlias)
        .addParameterAttribute(0, llvm::Attribute::WriteOnly)
        .addParameterAttribute(1, llvm::Attribute::ReadOnly)
        .setConstantFold(false)
        .setEmbedIR(true) // must be embedded
        .setPreferredImpl(op.mPrioritiseIR ?
            laovdb::ax::codegen::FunctionBuilder::IR :
            laovdb::ax::codegen::FunctionBuilder::C)
        .setDocumentation("Evaluate the channel referenced ramp value.")
        .get();
}

///////////////////////////////////////////////////////////////////////////


void registerCustomHoudiniFunctions(laovdb::ax::codegen::FunctionRegistry& registry,
                                    const laovdb::ax::FunctionOptions* options)
{
    // @note - we could alias matching functions such as ch and chv here, but we opt
    // to use the modifier so that all supported VEX conversion is in one place. chramp
    // is a re-implemented function and is not currently supported outside of the Houdini
    // plugin

    const bool create = options && !options->mLazyFunctions;
    auto add = [&](const std::string& name,
        const laovdb::ax::codegen::FunctionRegistry::ConstructorT creator,
        const bool internal = false)
    {
        if (create) registry.insertAndCreate(name, creator, *options, internal);
        else        registry.insert(name, creator, internal);
    };

    add("_chramp", hax_chramp, true);
    add("chramp", haxchramp);
}

} // namespace openvdb_ax_houdini

#endif // OPENVDB_AX_HOUDINI_AX_UTILS_HAS_BEEN_INCLUDED
