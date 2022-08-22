#!/bin/bash

generate_asm_file() {
    INCBIN_LINE=""
    if [ -e "$2" ]; then
        INCBIN_LINE=".incbin \"$2\""
    fi

    cat >$3 <<EOF
.section .rodata

.global $1_start;
.global $1_end;
.global $1_size;

$1_start: ${INCBIN_LINE}
$1_end:
$1_size: .long $1_end - $1_start
EOF
}

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

if [ -z $(command -v yarang) ]; then
    echo "You need to have yarangc available in PATH"
    exit 1
fi

if [ -z ${HYPERSCAN_ROOT_DIR} ]; then
    echo "You need to have HYPERSCAN_ROOT_DIR environment variable specified"
    exit 1
fi

if [ -e ${HYPERSCAN_ROOT_DIR}/lib64 ]; then
    HYPERSCAN_LIB_DIR=${HYPERSCAN_ROOT_DIR}/lib64
else
    HYPERSCAN_LIB_DIR=${HYPERSCAN_ROOT_DIR}/lib
fi

yarangc "$@"

RULESET_CPP_FILE=${SCRIPT_DIR}/ruleset.yar.cpp
RULESET_OBJ_FILE=$(pwd)/$(basename ${RULESET_CPP_FILE}).o
LITERAL_DB_FILE=$1.literal.db
REGEX_DB_FILE=$1.regex.db
MUTEX_DB_FILE=$1.mutex.db
RULESET_BIN=$1.bin

LITERAL_ASM_FILE=${LITERAL_DB_FILE}.s
REGEX_ASM_FILE=${REGEX_DB_FILE}.s
MUTEX_ASM_FILE=${MUTEX_DB_FILE}.s

DB_OBJECT_FILES=()

generate_asm_file "literal_db" ${LITERAL_DB_FILE} ${LITERAL_ASM_FILE}
g++ -nostdlib -c ${LITERAL_ASM_FILE} -o ${LITERAL_DB_FILE}.o
DB_OBJECT_FILES+=(${LITERAL_DB_FILE}.o)

generate_asm_file "regex_db" ${REGEX_DB_FILE} ${REGEX_ASM_FILE}
g++ -nostdlib -c ${REGEX_ASM_FILE} -o ${REGEX_DB_FILE}.o
DB_OBJECT_FILES+=(${REGEX_DB_FILE}.o)

generate_asm_file "mutex_db" ${MUTEX_DB_FILE} ${MUTEX_ASM_FILE}
g++ -nostdlib -c ${MUTEX_ASM_FILE} -o ${MUTEX_DB_FILE}.o
DB_OBJECT_FILES+=(${MUTEX_DB_FILE}.o)

# Uncomment for release
g++ -fPIC -O3 -c -std=c++20 -Wno-narrowing -I ${HYPERSCAN_ROOT_DIR}/include -I $(pwd) -o ${RULESET_OBJ_FILE} ${RULESET_CPP_FILE}
g++ -fPIC -rdynamic -shared -std=c++20 -o ${RULESET_BIN} ${RULESET_OBJ_FILE} "${DB_OBJECT_FILES[@]}" ${HYPERSCAN_LIB_DIR}/libhs_runtime.a -Wl,--retain-symbols-file=${SCRIPT_DIR}/yng.syms

# Uncomment for debug
#g++ -g -fPIC -O0 -c -std=c++20 -I ${HYPERSCAN_ROOT_DIR}/include -I $(pwd) -o ${RULESET_OBJ_FILE} ${RULESET_CPP_FILE}
#g++ -g -fPIC -rdynamic -shared -std=c++20 -o ${RULESET_BIN} ${RULESET_OBJ_FILE} "${DB_OBJECT_FILES[@]}" ${HYPERSCAN_LIB_DIR}/libhs_runtime.a

rm -f ${LITERAL_DB_FILE} ${REGEX_DB_FILE} ${LITERAL_ASM_FILE} ${REGEX_ASM_FILE} ${MUTEX_DB_FILE} ${MUTEX_ASM_FILE} ${RULESET_OBJ_FILE} ${LITERAL_DB_FILE}.o ${REGEX_DB_FILE}.o ${MUTEX_DB_FILE}.o
