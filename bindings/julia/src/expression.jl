mutable struct Expression
    ptr::Ptr{C.quiver_expression}

    function Expression(ptr::Ptr{C.quiver_expression})
        e = new(ptr)
        finalizer(x -> x.ptr != C_NULL && C.quiver_expression_close(x.ptr), e)
        return e
    end
end

function Expression(file::Binary.File)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_from_file(file.ptr, out))
    return Expression(out[])
end

function close!(e::Expression)
    if e.ptr != C_NULL
        C.quiver_expression_close(e.ptr)
        e.ptr = C_NULL
    end
    return nothing
end

function _binop(operation, lhs::Expression, rhs::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply(operation, lhs.ptr, rhs.ptr, out))
    return Expression(out[])
end

function _binop(operation, lhs::Expression, rhs::Real)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply_scalar_right(operation, lhs.ptr, Float64(rhs), out))
    return Expression(out[])
end

function _binop(operation, lhs::Real, rhs::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply_scalar_left(operation, Float64(lhs), rhs.ptr, out))
    return Expression(out[])
end

function _unop(operation, e::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply_unary(operation, e.ptr, out))
    return Expression(out[])
end

Base.:-(a::Expression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_NEGATE, a)
Base.abs(a::Expression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_ABS, a)
Base.sqrt(a::Expression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_SQRT, a)
Base.log(a::Expression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_LOG, a)
Base.exp(a::Expression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_EXP, a)

Base.:+(a::Expression, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_ADD, a, b)
Base.:+(a::Expression, b::Real) = _binop(C.QUIVER_EXPRESSION_OPERATION_ADD, a, b)
Base.:+(a::Real, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_ADD, a, b)

Base.:-(a::Expression, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_SUBTRACT, a, b)
Base.:-(a::Expression, b::Real) = _binop(C.QUIVER_EXPRESSION_OPERATION_SUBTRACT, a, b)
Base.:-(a::Real, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_SUBTRACT, a, b)

Base.:*(a::Expression, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_MULTIPLY, a, b)
Base.:*(a::Expression, b::Real) = _binop(C.QUIVER_EXPRESSION_OPERATION_MULTIPLY, a, b)
Base.:*(a::Real, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_MULTIPLY, a, b)

Base.:/(a::Expression, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_DIVIDE, a, b)
Base.:/(a::Expression, b::Real) = _binop(C.QUIVER_EXPRESSION_OPERATION_DIVIDE, a, b)
Base.:/(a::Real, b::Expression) = _binop(C.QUIVER_EXPRESSION_OPERATION_DIVIDE, a, b)

Base.:+(a::Binary.File, b::Binary.File) = Expression(a) + Expression(b)
Base.:+(a::Binary.File, b::Real) = Expression(a) + b
Base.:+(a::Real, b::Binary.File) = a + Expression(b)
Base.:+(a::Binary.File, b::Expression) = Expression(a) + b
Base.:+(a::Expression, b::Binary.File) = a + Expression(b)

Base.:-(a::Binary.File, b::Binary.File) = Expression(a) - Expression(b)
Base.:-(a::Binary.File, b::Real) = Expression(a) - b
Base.:-(a::Real, b::Binary.File) = a - Expression(b)
Base.:-(a::Binary.File, b::Expression) = Expression(a) - b
Base.:-(a::Expression, b::Binary.File) = a - Expression(b)

Base.:*(a::Binary.File, b::Binary.File) = Expression(a) * Expression(b)
Base.:*(a::Binary.File, b::Real) = Expression(a) * b
Base.:*(a::Real, b::Binary.File) = a * Expression(b)
Base.:*(a::Binary.File, b::Expression) = Expression(a) * b
Base.:*(a::Expression, b::Binary.File) = a * Expression(b)

Base.:/(a::Binary.File, b::Binary.File) = Expression(a) / Expression(b)
Base.:/(a::Binary.File, b::Real) = Expression(a) / b
Base.:/(a::Real, b::Binary.File) = a / Expression(b)
Base.:/(a::Binary.File, b::Expression) = Expression(a) / b
Base.:/(a::Expression, b::Binary.File) = a / Expression(b)

Base.:-(a::Binary.File) = -Expression(a)
Base.abs(a::Binary.File) = abs(Expression(a))
Base.sqrt(a::Binary.File) = sqrt(Expression(a))
Base.log(a::Binary.File) = log(Expression(a))
Base.exp(a::Binary.File) = exp(Expression(a))

function Base.ifelse(condition::Expression, then_value::Expression, else_value::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(
        C.quiver_expression_apply_ternary(C.QUIVER_EXPRESSION_TERNARY_OPERATION_IFELSE,
            condition.ptr, then_value.ptr, else_value.ptr, out),
    )
    return Expression(out[])
end

Base.ifelse(condition::Binary.File, then_value::Binary.File, else_value::Binary.File) =
    ifelse(Expression(condition), Expression(then_value), Expression(else_value))
Base.ifelse(condition::Binary.File, then_value::Expression, else_value::Expression) =
    ifelse(Expression(condition), then_value, else_value)
Base.ifelse(condition::Expression, then_value::Binary.File, else_value::Expression) =
    ifelse(condition, Expression(then_value), else_value)
Base.ifelse(condition::Expression, then_value::Expression, else_value::Binary.File) =
    ifelse(condition, then_value, Expression(else_value))
Base.ifelse(condition::Binary.File, then_value::Binary.File, else_value::Expression) =
    ifelse(Expression(condition), Expression(then_value), else_value)
Base.ifelse(condition::Binary.File, then_value::Expression, else_value::Binary.File) =
    ifelse(Expression(condition), then_value, Expression(else_value))
Base.ifelse(condition::Expression, then_value::Binary.File, else_value::Binary.File) =
    ifelse(condition, Expression(then_value), Expression(else_value))

function save(e::Expression, path::String)
    check(C.quiver_expression_save(e.ptr, path))
    return nothing
end

function get_metadata(e::Expression)
    out = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    check(C.quiver_expression_get_metadata(e.ptr, out))
    return Binary.Metadata(out[])
end

function aggregate(e::Expression, dimension::String, operation::String, parameter::Optional{Real} = nothing)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if parameter === nothing
        check(C.quiver_expression_aggregate(e.ptr, dimension, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(parameter))
        GC.@preserve param_ref begin
            check(C.quiver_expression_aggregate(e.ptr, dimension, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function aggregate_agents(e::Expression, operation::String, parameter::Optional{Real} = nothing)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if parameter === nothing
        check(C.quiver_expression_aggregate_agents(e.ptr, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(parameter))
        GC.@preserve param_ref begin
            check(C.quiver_expression_aggregate_agents(e.ptr, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function aggregate(f::Binary.File, dimension::String, operation::String, parameter::Optional{Real} = nothing)
    return aggregate(Expression(f), dimension, operation, parameter)
end

function aggregate_agents(f::Binary.File, operation::String, parameter::Optional{Real} = nothing)
    return aggregate_agents(Expression(f), operation, parameter)
end
