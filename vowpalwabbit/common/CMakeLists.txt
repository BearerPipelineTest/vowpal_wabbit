set(vw_common_sources
    include/vw/common/vw_exception.h
    include/vw/common/future_compat.h
    include/vw/common/hash.h
    include/vw/common/string_view.h
    include/vw/common/text_utils.h
    include/vw/common/vwvis.h
)

vw_add_library(
    NAME "common"
    TYPE "HEADER_ONLY"
    SOURCES ${vw_common_sources}
    PUBLIC_DEPS string_view_lite
    DESCRIPTION "Common utilities that are shared by every project. The only dependencies permitted are polyfill/vocabulary types"
    EXCEPTION_DESCRIPTION "Yes, also supports `VW_NOEXCEPT`"
    ENABLE_INSTALL
)
