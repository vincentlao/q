
find_source_tree( LIBQ_HEADERS          "Header Files"          include/q "*.hpp" )
find_source_tree( LIBQ_INTERNAL_HEADERS "Internal Header Files" src       "*.hpp" )
find_source_tree( LIBQ_SOURCES          "Source Files"          src       "*.cpp" )

add_library( q ${LIBQ_HEADERS} ${LIBQ_INTERNAL_HEADERS} ${LIBQ_SOURCES} )

target_link_libraries( q ${CXXLIB} pthread dl )

include_directories( "include" )
