@echo off

SET BASE_PATH=%~dp0
SET QUIVER_JULIA_USE_LOCAL_SHARED_LIBRARY=1

CALL julia +1.12.5 --project=%BASE_PATH%\.. -e "import Pkg; Pkg.test(test_args=ARGS)" -- %*
