// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include "Parse.h"
#include "../Exceptions.h"

// if OPENVDB_AX_REGENERATE_GRAMMAR is defined, we've re-generated the
// grammar - include path should be set up to pull in from the temp dir
// @note We need to include this to get access to axlloc. Should look to
//   re-work this so we don't have to (would require a reentrant parser)
#ifdef OPENVDB_AX_REGENERATE_GRAMMAR
#include "axparser.h"
#else
#include "../grammar/generated/axparser.h"
#endif

#include <mutex>
#include <string>
#include <memory>

namespace {
// Declare this at file scope to ensure thread-safe initialization.
std::mutex sInitMutex;
}

laovdb::ax::Logger* axlog = nullptr;
using YY_BUFFER_STATE = struct yy_buffer_state*;
extern int axparse(laovdb::ax::ast::Tree**);
extern YY_BUFFER_STATE ax_scan_string(const char * str);
extern void ax_delete_buffer(YY_BUFFER_STATE buffer);
extern void axerror (laovdb::ax::ast::Tree**, char const *s) {
    //@todo: add check for memory exhaustion
    assert(axlog);
    axlog->error(/*starts with 'syntax error, '*/s + 14,
        {axlloc.first_line, axlloc.first_column});
}

laovdb::ax::ast::Tree::ConstPtr
laovdb::ax::ast::parse(const char* code,
    laovdb::ax::Logger& logger)
{
    std::lock_guard<std::mutex> lock(sInitMutex);
    axlog = &logger; // for lexer errs
    logger.setSourceCode(code);

    const size_t err = logger.errors();

    // reset all locations
    axlloc.first_line = axlloc.last_line = 1;
    axlloc.first_column = axlloc.last_column = 1;

    YY_BUFFER_STATE buffer = ax_scan_string(code);

    laovdb::ax::ast::Tree* tree(nullptr);
    axparse(&tree);
    axlog = nullptr;

    laovdb::ax::ast::Tree::ConstPtr ptr(const_cast<const laovdb::ax::ast::Tree*>(tree));

    ax_delete_buffer(buffer);

    if (logger.errors() > err) ptr.reset();

    logger.setSourceTree(ptr);
    return ptr;
}


laovdb::ax::ast::Tree::Ptr
laovdb::ax::ast::parse(const char* code)
{
    laovdb::ax::Logger logger(
        [](const std::string& error) {
            OPENVDB_THROW(laovdb::AXSyntaxError, error);
        });

    laovdb::ax::ast::Tree::ConstPtr constTree = laovdb::ax::ast::parse(code, logger);

    return std::const_pointer_cast<laovdb::ax::ast::Tree>(constTree);
}

