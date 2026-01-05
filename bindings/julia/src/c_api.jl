module C

using CEnum
using Libdl

global libpsr_database_c = raw"C:\Development\Database\database\build\bin\libpsr_database_c.dll"

@cenum psr_error_t::Int32 begin
    PSR_OK = 0
    PSR_ERROR_INVALID_ARGUMENT = -1
    PSR_ERROR_DATABASE = -2
end

@cenum psr_log_level_t::UInt32 begin
    PSR_LOG_DEBUG = 0
    PSR_LOG_INFO = 1
    PSR_LOG_WARN = 2
    PSR_LOG_ERROR = 3
    PSR_LOG_OFF = 4
end

struct psr_database_options_t
    read_only::Cint
    console_level::psr_log_level_t
end

function psr_database_options_default()
    @ccall libpsr_database_c.psr_database_options_default()::psr_database_options_t
end

mutable struct psr_database end

const psr_database_t = psr_database

function psr_database_open(path, options, error)
    @ccall libpsr_database_c.psr_database_open(path::Ptr{Cchar}, options::Ptr{psr_database_options_t}, error::Ptr{psr_error_t})::Ptr{psr_database_t}
end

function psr_database_close(db)
    @ccall libpsr_database_c.psr_database_close(db::Ptr{psr_database_t})::Cvoid
end

function psr_database_is_open(db)
    @ccall libpsr_database_c.psr_database_is_open(db::Ptr{psr_database_t})::Cint
end

function psr_database_path(db)
    @ccall libpsr_database_c.psr_database_path(db::Ptr{psr_database_t})::Ptr{Cchar}
end

function psr_error_string(error)
    @ccall libpsr_database_c.psr_error_string(error::psr_error_t)::Ptr{Cchar}
end

function psr_version()
    @ccall libpsr_database_c.psr_version()::Ptr{Cchar}
end

mutable struct sqlite3 end

# const PSR_C_API = __declspec(dllimport)

# const PSR_API = __declspec(dllimport)




end # module
