
add_executable(PlateSolver
    IPlateSolver.h
    PlateSolver.h
    PlateSolver.cpp
    main.cpp
)

target_link_libraries(PlateSolver 
    PRIVATE
    stellarsolver

    # `SSolverio` is `SSolverUtilsLib`, but with fewer dependencies
    SSolverio
)

target_include_directories(PlateSolver PUBLIC 
    "${CMAKE_CURRENT_LIST_DIR}")

include(GNUInstallDirs)
install(TARGETS PlateSolver
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
