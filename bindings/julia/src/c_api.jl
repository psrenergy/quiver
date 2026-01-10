module C

#! format: off

using CEnum
using Libdl

const libpsr_database_c = joinpath(@__DIR__, "..", "..", "..", "build", "bin", "libpsr_database_c.dll")  


@cenum psr_error_t::Int32 begin
    PSR_OK = 0
    PSR_ERROR_INVALID_ARGUMENT = -1
    PSR_ERROR_DATABASE = -2
    PSR_ERROR_MIGRATION = -3
    PSR_ERROR_SCHEMA = -4
    PSR_ERROR_CREATE_ELEMENT = -5
    PSR_ERROR_NOT_FOUND = -6
end

@cenum psr_value_type_t::UInt32 begin
    PSR_VALUE_NULL = 0
    PSR_VALUE_INT64 = 1
    PSR_VALUE_DOUBLE = 2
    PSR_VALUE_STRING = 3
    PSR_VALUE_ARRAY = 4
end

struct var"##Ctag#232"
    data::NTuple{16, UInt8}
end

function Base.getproperty(x::Ptr{var"##Ctag#232"}, f::Symbol)
    f === :int_value && return Ptr{Int64}(x + 0)
    f === :double_value && return Ptr{Cdouble}(x + 0)
    f === :string_value && return Ptr{Ptr{Cchar}}(x + 0)
    f === :array_value && return Ptr{var"##Ctag#233"}(x + 0)
    return getfield(x, f)
end

function Base.getproperty(x::var"##Ctag#232", f::Symbol)
    r = Ref{var"##Ctag#232"}(x)
    ptr = Base.unsafe_convert(Ptr{var"##Ctag#232"}, r)
    fptr = getproperty(ptr, f)
    GC.@preserve r unsafe_load(fptr)
end

function Base.setproperty!(x::Ptr{var"##Ctag#232"}, f::Symbol, v)
    unsafe_store!(getproperty(x, f), v)
end

function Base.propertynames(x::var"##Ctag#232", private::Bool = false)
    (:int_value, :double_value, :string_value, :array_value, if private
            fieldnames(typeof(x))
        else
            ()
        end...)
end

struct psr_value
    data::NTuple{24, UInt8}
end

function Base.getproperty(x::Ptr{psr_value}, f::Symbol)
    f === :type && return Ptr{psr_value_type_t}(x + 0)
    f === :data && return Ptr{var"##Ctag#232"}(x + 8)
    return getfield(x, f)
end

function Base.getproperty(x::psr_value, f::Symbol)
    r = Ref{psr_value}(x)
    ptr = Base.unsafe_convert(Ptr{psr_value}, r)
    fptr = getproperty(ptr, f)
    GC.@preserve r unsafe_load(fptr)
end

function Base.setproperty!(x::Ptr{psr_value}, f::Symbol, v)
    unsafe_store!(getproperty(x, f), v)
end

function Base.propertynames(x::psr_value, private::Bool = false)
    (:type, :data, if private
            fieldnames(typeof(x))
        else
            ()
        end...)
end

const psr_value_t = psr_value

struct psr_read_result_t
    error::psr_error_t
    values::Ptr{psr_value_t}
    count::Csize_t
end

function psr_error_string(error)
    @ccall libpsr_database_c.psr_error_string(error::psr_error_t)::Ptr{Cchar}
end

function psr_version()
    @ccall libpsr_database_c.psr_version()::Ptr{Cchar}
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

function psr_database_open(path, options)
    @ccall libpsr_database_c.psr_database_open(path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_migrations(db_path, migrations_path, options)
    @ccall libpsr_database_c.psr_database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_schema(db_path, schema_path, options)
    @ccall libpsr_database_c.psr_database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_close(db)
    @ccall libpsr_database_c.psr_database_close(db::Ptr{psr_database_t})::Cvoid
end

function psr_database_is_healthy(db)
    @ccall libpsr_database_c.psr_database_is_healthy(db::Ptr{psr_database_t})::Cint
end

function psr_database_path(db)
    @ccall libpsr_database_c.psr_database_path(db::Ptr{psr_database_t})::Ptr{Cchar}
end

function psr_database_current_version(db)
    @ccall libpsr_database_c.psr_database_current_version(db::Ptr{psr_database_t})::Int64
end

function psr_database_set_version(db, version)
    @ccall libpsr_database_c.psr_database_set_version(db::Ptr{psr_database_t}, version::Int64)::psr_error_t
end

function psr_database_migrate_up(db, migrations_path)
    @ccall libpsr_database_c.psr_database_migrate_up(db::Ptr{psr_database_t}, migrations_path::Ptr{Cchar})::psr_error_t
end

function psr_database_begin_transaction(db)
    @ccall libpsr_database_c.psr_database_begin_transaction(db::Ptr{psr_database_t})::psr_error_t
end

function psr_database_commit(db)
    @ccall libpsr_database_c.psr_database_commit(db::Ptr{psr_database_t})::psr_error_t
end

function psr_database_rollback(db)
    @ccall libpsr_database_c.psr_database_rollback(db::Ptr{psr_database_t})::psr_error_t
end

mutable struct psr_element end

const psr_element_t = psr_element

function psr_database_create_element(db, collection, element)
    @ccall libpsr_database_c.psr_database_create_element(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, element::Ptr{psr_element_t})::Int64
end

function psr_database_read_scalar_parameters(db, collection, attribute)
    @ccall libpsr_database_c.psr_database_read_scalar_parameters(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar})::psr_read_result_t
end

function psr_database_read_scalar_parameter(db, collection, attribute, label)
    @ccall libpsr_database_c.psr_database_read_scalar_parameter(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar})::psr_read_result_t
end

function psr_database_read_vector_parameters(db, collection, attribute)
    @ccall libpsr_database_c.psr_database_read_vector_parameters(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar})::psr_read_result_t
end

function psr_database_read_vector_parameter(db, collection, attribute, label)
    @ccall libpsr_database_c.psr_database_read_vector_parameter(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar})::psr_read_result_t
end

function psr_database_read_set_parameters(db, collection, attribute)
    @ccall libpsr_database_c.psr_database_read_set_parameters(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar})::psr_read_result_t
end

function psr_database_read_set_parameter(db, collection, attribute, label)
    @ccall libpsr_database_c.psr_database_read_set_parameter(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar})::psr_read_result_t
end

function psr_value_free(value)
    @ccall libpsr_database_c.psr_value_free(value::Ptr{psr_value_t})::Cvoid
end

function psr_read_result_free(result)
    @ccall libpsr_database_c.psr_read_result_free(result::Ptr{psr_read_result_t})::Cvoid
end

function psr_element_create()
    @ccall libpsr_database_c.psr_element_create()::Ptr{psr_element_t}
end

function psr_element_destroy(element)
    @ccall libpsr_database_c.psr_element_destroy(element::Ptr{psr_element_t})::Cvoid
end

function psr_element_clear(element)
    @ccall libpsr_database_c.psr_element_clear(element::Ptr{psr_element_t})::Cvoid
end

function psr_element_set_int(element, name, value)
    @ccall libpsr_database_c.psr_element_set_int(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Int64)::psr_error_t
end

function psr_element_set_double(element, name, value)
    @ccall libpsr_database_c.psr_element_set_double(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Cdouble)::psr_error_t
end

function psr_element_set_string(element, name, value)
    @ccall libpsr_database_c.psr_element_set_string(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::psr_error_t
end

function psr_element_set_null(element, name)
    @ccall libpsr_database_c.psr_element_set_null(element::Ptr{psr_element_t}, name::Ptr{Cchar})::psr_error_t
end

function psr_element_set_array_int(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_int(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::psr_error_t
end

function psr_element_set_array_double(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_double(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::psr_error_t
end

function psr_element_set_array_string(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_string(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::psr_error_t
end

function psr_element_has_scalars(element)
    @ccall libpsr_database_c.psr_element_has_scalars(element::Ptr{psr_element_t})::Cint
end

function psr_element_has_arrays(element)
    @ccall libpsr_database_c.psr_element_has_arrays(element::Ptr{psr_element_t})::Cint
end

function psr_element_scalar_count(element)
    @ccall libpsr_database_c.psr_element_scalar_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_array_count(element)
    @ccall libpsr_database_c.psr_element_array_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_to_string(element)
    @ccall libpsr_database_c.psr_element_to_string(element::Ptr{psr_element_t})::Ptr{Cchar}
end

function psr_string_free(str)
    @ccall libpsr_database_c.psr_string_free(str::Ptr{Cchar})::Cvoid
end

struct var"##Ctag#233"
    elements::Ptr{psr_value_t}
    count::Csize_t
end
function Base.getproperty(x::Ptr{var"##Ctag#233"}, f::Symbol)
    f === :elements && return Ptr{Ptr{psr_value_t}}(x + 0)
    f === :count && return Ptr{Csize_t}(x + 8)
    return getfield(x, f)
end

function Base.getproperty(x::var"##Ctag#233", f::Symbol)
    r = Ref{var"##Ctag#233"}(x)
    ptr = Base.unsafe_convert(Ptr{var"##Ctag#233"}, r)
    fptr = getproperty(ptr, f)
    GC.@preserve r unsafe_load(fptr)
end

function Base.setproperty!(x::Ptr{var"##Ctag#233"}, f::Symbol, v)
    unsafe_store!(getproperty(x, f), v)
end


#! format: on


end # module
