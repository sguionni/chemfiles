include(CheckCXXCompilerFlag)

# C++11 support in some compilers. The other compilers are supported with the
# set(CMAKE_CXX_STANDARD 11) command in the top-level CMakeLists.txt
set(cxx11_flag_needed OFF)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    set(cxx11_flag_needed ON)
elseif(${CMAKE_COMPILER_IS_GNUCC})
    if(${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 5.0)
        set(cxx11_flag_needed ON)
    endif()
endif()

if(${cxx11_flag_needed})
    CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
    CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
    if(COMPILER_SUPPORTS_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    elseif(COMPILER_SUPPORTS_CXXOX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    else()
        message(SEND_ERROR
            "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.
            Please use a different C++ compiler.")
    endif()
endif()

macro(set_debug_flag_if_possible _flag_)
    CHECK_CXX_COMPILER_FLAG("${_flag_}" CXX_SUPPORTS${_flag_})
    if(CXX_SUPPORTS${_flag_})
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${_flag_}")
    endif()
endmacro()

# Add some warnings in debug mode
if(MSVC)
    set_debug_flag_if_possible("/W4")
else()
    set_debug_flag_if_possible("-Wall")
    set_debug_flag_if_possible("-Wextra")
    set_debug_flag_if_possible("-Wconversion")
    set_debug_flag_if_possible("-Wsign-conversion")
    set_debug_flag_if_possible("-Wsign-promo")
endif()

if(FORTRAN_BINDING)
include(CheckFortranCompilerFlag)
macro(set_fortran_debug_flag_if_possible _flag_)
    CHECK_Fortran_COMPILER_FLAG("${_flag_}" FC_SUPPORTS${_flag_})
    if(FC_SUPPORTS${_flag_})
        set(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} ${_flag_}")
    endif()
endmacro()

set_fortran_debug_flag_if_possible("-Wall")
endif()

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-std=c99" CC_SUPPORTS_C99)

if(CC_SUPPORTS_C99)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()

if(MSVC)
    add_definitions("/D COMPILER_IS_MSVC")
    add_definitions("/D NOMINMAX")
    add_definitions("/D BOOST_ALL_NO_LIB")
endif()
