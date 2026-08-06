# Strict-but-practical warning set shared by all SappChoir targets.
function(sappchoir_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wsign-conversion
      -Wno-unused-parameter
    )
  endif()
endfunction()
