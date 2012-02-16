PROJECT(mapexport)

INCLUDE_DIRECTORIES (
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${dfhack_SOURCE_DIR}/depends/protobuf/
    ${dfhack_SOURCE_DIR}/depends/zlib/
)

LINK_DIRECTORIES(
    ${dfhack_SOURCE_DIR}/depends/zlib/
)

#The protobuf sources we generate will require these headers
SET(PROTOBUF_DIR ${dfhack_SOURCE_DIR}/depends)
SET(PROJECT_HDRS
${PROTOBUF_DIR}/google/protobuf/stubs/once.h
${PROTOBUF_DIR}/google/protobuf/stubs/common.h
${PROTOBUF_DIR}/google/protobuf/io/coded_stream.h
${PROTOBUF_DIR}/google/protobuf/wire_format_lite_inl.h
${PROTOBUF_DIR}/google/protobuf/generated_message_util.h
${PROTOBUF_DIR}/google/protobuf/repeated_field.h
${PROTOBUF_DIR}/google/protobuf/extension_set.h
)

SET(PROJECT_SRCS
mapexport.cpp
)

SET(PROJECT_PROTOS
${CMAKE_CURRENT_SOURCE_DIR}/proto/Tile.proto
${CMAKE_CURRENT_SOURCE_DIR}/proto/Plant.proto
${CMAKE_CURRENT_SOURCE_DIR}/proto/Block.proto
${CMAKE_CURRENT_SOURCE_DIR}/proto/Material.proto
${CMAKE_CURRENT_SOURCE_DIR}/proto/Map.proto
)

#Create new lists of what sources and headers protoc will output after we invoke it
STRING(REPLACE ".proto" ".pb.cc;" PROJECT_PROTO_SRCS ${PROJECT_PROTOS})
STRING(REPLACE ".proto" ".pb.h;" PROJECT_PROTO_HDRS ${PROJECT_PROTOS})

LIST(APPEND PROJECT_HDRS ${PROJECT_PROTO_HDRS})
LIST(APPEND PROJECT_SRCS ${PROJECT_PROTO_SRCS})

SET_SOURCE_FILES_PROPERTIES( ${PROJECT_HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)
LIST(APPEND PROJECT_SRCS ${PROJECT_HDRS})

#Generate sources from our proto files and store them in the source tree
ADD_CUSTOM_COMMAND(
OUTPUT ${PROJECT_PROTO_SRCS} ${PROJECT_PROTO_HDRS}
COMMAND protoc-bin -I=${CMAKE_CURRENT_SOURCE_DIR}/proto/ --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/proto/ ${PROJECT_PROTOS}
DEPENDS protoc-bin ${PROJECT_PROTOS}
)

IF(WIN32)
    DFHACK_PLUGIN(mapexport ${PROJECT_SRCS} ${PROJECT_HDRS} LINK_LIBRARIES protobuf zlib)
ELSE()
    DFHACK_PLUGIN(mapexport ${PROJECT_SRCS} ${PROJECT_HDRS} LINK_LIBRARIES protobuf z)
ENDIF()
