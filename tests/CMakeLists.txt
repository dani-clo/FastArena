add_executable(fastarena_tests
	arena_tests.cpp
)

target_link_libraries(fastarena_tests
	PRIVATE
	FastArena::fastarena
)

target_compile_features(fastarena_tests PRIVATE cxx_std_23)

add_test(NAME fastarena_tests COMMAND fastarena_tests)

