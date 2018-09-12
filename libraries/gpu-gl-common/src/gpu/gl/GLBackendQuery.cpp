//
//  GLBackendQuery.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 7/7/2015.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GLBackend.h"
#include "GLQuery.h"
#include "GLShared.h"

using namespace gpu;
using namespace gpu::gl;

// Eventually, we want to test with TIME_ELAPSED instead of TIMESTAMP
#ifdef Q_OS_MAC
const uint32_t MAX_RANGE_QUERY_DEPTH = 1;
static bool timeElapsed = true;
#else
const uint32_t MAX_RANGE_QUERY_DEPTH = 10000;
static bool timeElapsed = false;
#endif

void GLBackend::do_beginQuery(const Batch& batch, size_t paramOffset) {
    auto query = batch._queries.get(batch._params[paramOffset]._uint);
    GLQuery* glquery = syncGPUObject(*query);
    if (glquery) {
        PROFILE_RANGE_BEGIN(render_gpu_gl_detail, glquery->_profileRangeId, query->getName().c_str(), 0xFFFF7F00);

        ++_queryStage._rangeQueryDepth;
        glquery->_batchElapsedTimeBegin = std::chrono::high_resolution_clock::now();

#if !defined(USE_GLES)
        if (timeElapsed) {
            if (_queryStage._rangeQueryDepth <= MAX_RANGE_QUERY_DEPTH) {
                glBeginQuery(GL_TIME_ELAPSED, glquery->_endqo);
            }
        } else {
            glQueryCounter(glquery->_beginqo, GL_TIMESTAMP);
        }
#endif

        glquery->_rangeQueryDepth = _queryStage._rangeQueryDepth;
        (void)CHECK_GL_ERROR();
    }
}

void GLBackend::do_endQuery(const Batch& batch, size_t paramOffset) {
    auto query = batch._queries.get(batch._params[paramOffset]._uint);
    GLQuery* glquery = syncGPUObject(*query);
    if (glquery) {
#if !defined(USE_GLES)
        if (timeElapsed) {
            if (_queryStage._rangeQueryDepth <= MAX_RANGE_QUERY_DEPTH) {
                glEndQuery(GL_TIME_ELAPSED);
            }
        } else {
            glQueryCounter(glquery->_endqo, GL_TIMESTAMP);
        }
#endif

        --_queryStage._rangeQueryDepth;
        auto duration_ns = (std::chrono::high_resolution_clock::now() - glquery->_batchElapsedTimeBegin);
        glquery->_batchElapsedTime = duration_ns.count();

        PROFILE_RANGE_END(render_gpu_gl_detail, glquery->_profileRangeId);

        (void)CHECK_GL_ERROR();
    }
}

void GLBackend::do_getQuery(const Batch& batch, size_t paramOffset) {
    auto query = batch._queries.get(batch._params[paramOffset]._uint);
    GLQuery* glquery = syncGPUObject(*query);
    if (glquery) {
        if (glquery->_rangeQueryDepth > MAX_RANGE_QUERY_DEPTH) {
            query->triggerReturnHandler(glquery->_result, glquery->_batchElapsedTime);
        } else {
#if !defined(USE_GLES)
            glGetQueryObjectui64v(glquery->_endqo, GL_QUERY_RESULT_AVAILABLE, &glquery->_result);
            if (glquery->_result == GL_TRUE) {
                if (timeElapsed) {
                    glGetQueryObjectui64v(glquery->_endqo, GL_QUERY_RESULT, &glquery->_result);
                } else {
                    GLuint64 start, end;
                    glGetQueryObjectui64v(glquery->_beginqo, GL_QUERY_RESULT, &start);
                    glGetQueryObjectui64v(glquery->_endqo, GL_QUERY_RESULT, &end);
                    glquery->_result = end - start;
                }
                query->triggerReturnHandler(glquery->_result, glquery->_batchElapsedTime);
            }
#else 
            // gles3 is not supporting true time query returns just the batch elapsed time
            query->triggerReturnHandler(0, glquery->_batchElapsedTime);
#endif
            (void)CHECK_GL_ERROR();
        }
    }
}

void GLBackend::resetQueryStage() {
}
