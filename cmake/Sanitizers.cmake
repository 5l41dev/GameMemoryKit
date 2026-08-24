# GameMemoryKit — sanitizer configuration.
#
# Enable with:  cmake -B build -DGMK_ENABLE_SANITIZERS=ON
#
# Enables AddressSanitizer and UndefinedBehaviorSanitizer where the toolchain
# supports them. MSVC supports ASan via /fsanitize=address; UBSan is not
# available on MSVC and is skipped there.

function(gmk_enable_sanitizers target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        target_link_options(${target} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )
    endif()
endfunction()
