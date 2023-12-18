# Copyright (c) 2014-2019, The Monero Project
# Copyright (c)      2019, The Loki Project
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers


find_program(GIT git REQUIRED)

message(STATUS "generate version using: ${GIT} rev-parse --short HEAD")

# Check what commit we're on
execute_process(COMMAND "${GIT}" rev-parse --short HEAD 
    RESULT_VARIABLE RET 
    OUTPUT_VARIABLE COMMIT 
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    
if(RET)
	# Something went wrong, set the version tag to -unknown
    message(WARNING "Cannot determine current commit. Make sure that you are building either from a Git working tree or from a source archive.")
    set(VERSIONTAG "unknown")
else()
	message(STATUS "You are currently on commit ${COMMIT}")

	# Get all the tags
	execute_process(COMMAND "${GIT}" rev-list --tags --max-count=1 --abbrev-commit RESULT_VARIABLE RET OUTPUT_VARIABLE TAGGEDCOMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT TAGGEDCOMMIT)
        message(WARNING "Cannot determine most recent tag. Make sure that you are building either from a Git working tree or from a source archive.")
        set(VERSIONTAG "${COMMIT}")
    else()
        # Check if we're building that tagged commit or a different one
        if(COMMIT STREQUAL TAGGEDCOMMIT)
            message(STATUS "${COMMIT} is a tagged release; setting version tag to 'release'")
            set(VERSIONTAG "release")
        else()
            message(STATUS "You are not building a tagged release; setting version tag to '${COMMIT}'")
            set(VERSIONTAG "${COMMIT}")
        endif()
    endif()
endif()

configure_file("${PROJECT_ROOT_DIR}/llarp/constants/version.cpp.in" "${CMAKE_BINARY_DIR}/llarp/constants/version.cpp" @ONLY)
