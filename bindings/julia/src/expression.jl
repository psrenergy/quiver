mutable struct Expression <: AbstractExpression
    ptr::Ptr{C.quiver_expression}

    function Expression(ptr::Ptr{C.quiver_expression})
        e = new(ptr)
        finalizer(x -> x.ptr != C_NULL && C.quiver_expression_close(x.ptr), e)
        return e
    end
end

function Expression(file::Binary.File)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve file check(C.quiver_expression_from_file(file.ptr, out))
    return Expression(out[])
end

function close!(e::Expression)
    if e.ptr != C_NULL
        C.quiver_expression_close(e.ptr)
        e.ptr = C_NULL
    end
    return nothing
end

# Operations convert their operands here. Private on purpose: a public identity constructor would
# hand back the caller's own handle, so closing the result would close the argument.
_expression(e::Expression) = e
_expression(a::AbstractExpression) = Expression(a)

function _binop(operation, a::AbstractExpression, b::AbstractExpression)
    lhs, rhs = _expression(a), _expression(b)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve lhs rhs check(C.quiver_expression_apply(operation, lhs.ptr, rhs.ptr, out))
    return Expression(out[])
end

function _binop(operation, a::AbstractExpression, b::Real)
    lhs = _expression(a)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve lhs check(C.quiver_expression_apply_scalar_right(operation, lhs.ptr, Float64(b), out))
    return Expression(out[])
end

function _binop(operation, a::Real, b::AbstractExpression)
    rhs = _expression(b)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve rhs check(C.quiver_expression_apply_scalar_left(operation, Float64(a), rhs.ptr, out))
    return Expression(out[])
end

function _unop(operation, a::AbstractExpression)
    e = _expression(a)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve e check(C.quiver_expression_apply_unary(operation, e.ptr, out))
    return Expression(out[])
end

Base.:-(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_NEGATE, a)
Base.abs(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_ABS, a)
Base.sqrt(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_SQRT, a)
Base.log(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_LOG, a)
Base.exp(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_EXP, a)

for (op, cop) in ((:+, :QUIVER_EXPRESSION_OPERATION_ADD), (:-, :QUIVER_EXPRESSION_OPERATION_SUBTRACT),
    (:*, :QUIVER_EXPRESSION_OPERATION_MULTIPLY), (:/, :QUIVER_EXPRESSION_OPERATION_DIVIDE))
    @eval begin
        Base.$op(a::AbstractExpression, b::AbstractExpression) = _binop(C.$cop, a, b)
        Base.$op(a::AbstractExpression, b::Real) = _binop(C.$cop, a, b)
        Base.$op(a::Real, b::AbstractExpression) = _binop(C.$cop, a, b)
    end
end

# Element-wise comparisons: 1.0 (true) / 0.0 (false) per element; a NaN operand propagates as NaN.
# Named functions gt/lt/gte/lte/eq/neq mirror the Lua surface; eq/neq are named-only because
# overloading ==/!= to return an Expression would break Dict/Set/isequal. Operator sugar (> < >= <=)
# is provided for the four orderings. Generated for the three operand shapes over AbstractExpression
# and Real that the arithmetic operators above cover.
for (fname, cop) in ((:gt, :QUIVER_EXPRESSION_OPERATION_GT), (:lt, :QUIVER_EXPRESSION_OPERATION_LT),
    (:gte, :QUIVER_EXPRESSION_OPERATION_GTE), (:lte, :QUIVER_EXPRESSION_OPERATION_LTE),
    (:eq, :QUIVER_EXPRESSION_OPERATION_EQ), (:neq, :QUIVER_EXPRESSION_OPERATION_NEQ))
    @eval begin
        $fname(a::AbstractExpression, b::AbstractExpression) = _binop(C.$cop, a, b)
        $fname(a::AbstractExpression, b::Real) = _binop(C.$cop, a, b)
        $fname(a::Real, b::AbstractExpression) = _binop(C.$cop, a, b)
    end
end

for (op, fname) in ((:(>), :gt), (:(<), :lt), (:(>=), :gte), (:(<=), :lte))
    @eval begin
        Base.$op(a::AbstractExpression, b::AbstractExpression) = $fname(a, b)
        Base.$op(a::AbstractExpression, b::Real) = $fname(a, b)
        Base.$op(a::Real, b::AbstractExpression) = $fname(a, b)
    end
end

# Logical operators on boolean-valued expressions (nonzero = true, NaN propagates); the result is
# unitless. `&`/`|` are used for and/or because `&&`/`||` are short-circuit syntax that can't be
# overloaded (on Bool, `&`/`|` are the non-short-circuit logical operators); `!` is a real function
# so it stays the logical-not operator. Generated for the same three operand shapes.
for (op, cop) in ((:(&), :QUIVER_EXPRESSION_OPERATION_AND), (:(|), :QUIVER_EXPRESSION_OPERATION_OR))
    @eval begin
        Base.$op(a::AbstractExpression, b::AbstractExpression) = _binop(C.$cop, a, b)
        Base.$op(a::AbstractExpression, b::Real) = _binop(C.$cop, a, b)
        Base.$op(a::Real, b::AbstractExpression) = _binop(C.$cop, a, b)
    end
end

Base.:!(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_NOT, a)

function Base.ifelse(condition::AbstractExpression, then_value::AbstractExpression, else_value::AbstractExpression)
    c, t, e = _expression(condition), _expression(then_value), _expression(else_value)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve c t e check(
        C.quiver_expression_apply_ternary(C.QUIVER_EXPRESSION_TERNARY_OPERATION_IFELSE, c.ptr, t.ptr, e.ptr, out),
    )
    return Expression(out[])
end

function save(a::AbstractExpression, path::String)
    e = _expression(a)
    GC.@preserve e check(C.quiver_expression_save(e.ptr, path))
    return nothing
end

# A file uses Binary.get_metadata(::File), its handle's metadata, never the conversion.
function get_metadata(e::Expression)
    out = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    GC.@preserve e check(C.quiver_expression_get_metadata(e.ptr, out))
    return Binary.Metadata(out[])
end

function aggregate(
    a::AbstractExpression,
    dimension::String,
    operation::C.quiver_expression_aggregate_operation_t,
    parameter::Optional{Real} = nothing,
)
    e = _expression(a)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if parameter === nothing
        GC.@preserve e check(C.quiver_expression_aggregate(e.ptr, dimension, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(parameter))
        GC.@preserve e param_ref begin
            check(C.quiver_expression_aggregate(e.ptr, dimension, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function aggregate_agents(
    a::AbstractExpression,
    operation::C.quiver_expression_aggregate_operation_t,
    parameter::Optional{Real} = nothing,
)
    e = _expression(a)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if parameter === nothing
        GC.@preserve e check(C.quiver_expression_aggregate_agents(e.ptr, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(parameter))
        GC.@preserve e param_ref begin
            check(C.quiver_expression_aggregate_agents(e.ptr, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function select_agents(a::AbstractExpression, labels::Vector{<:AbstractString})
    e = _expression(a)
    cstrings = [Base.cconvert(Cstring, s) for s in labels]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve e cstrings begin
        check(C.quiver_expression_select_agents(e.ptr, ptrs, length(labels), out))
    end
    return Expression(out[])
end

function rename_agents(a::AbstractExpression, mapping::AbstractDict{<:AbstractString, <:AbstractString})
    e = _expression(a)
    old_labels = String[String(k) for k in keys(mapping)]
    new_labels = String[String(mapping[k]) for k in old_labels]
    old_cstrings = [Base.cconvert(Cstring, s) for s in old_labels]
    new_cstrings = [Base.cconvert(Cstring, s) for s in new_labels]
    old_ptrs = [Base.unsafe_convert(Cstring, cs) for cs in old_cstrings]
    new_ptrs = [Base.unsafe_convert(Cstring, cs) for cs in new_cstrings]
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve e old_cstrings new_cstrings begin
        check(C.quiver_expression_rename_agents(e.ptr, old_ptrs, new_ptrs, length(old_labels), out))
    end
    return Expression(out[])
end
