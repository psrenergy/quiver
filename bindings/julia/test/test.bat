@echo off

SET BASE_PATH=%~dp0

CALL julia +1.12.3 --project=%BASE_PATH%\.. -e "import Pkg; Pkg.test(test_args=ARGS)" -- %*
