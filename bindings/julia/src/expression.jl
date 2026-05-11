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

function save(e::Expression, path::String)
    check(C.quiver_expression_save(e.ptr, path))
    return nothing
end

function get_metadata(e::Expression)
    out = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    check(C.quiver_expression_get_metadata(e.ptr, out))
    return Binary.Metadata(out[])
end

function aggregate(e::Expression, dimension::String, operation::String, param::Optional{Real} = nothing)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if param === nothing
        check(C.quiver_expression_aggregate(e.ptr, dimension, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(param))
        GC.@preserve param_ref begin
            check(C.quiver_expression_aggregate(e.ptr, dimension, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function aggregate_agents(e::Expression, operation::String, param::Optional{Real} = nothing)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    if param === nothing
        check(C.quiver_expression_aggregate_agents(e.ptr, operation, C_NULL, out))
    else
        param_ref = Ref(Cdouble(param))
        GC.@preserve param_ref begin
            check(C.quiver_expression_aggregate_agents(e.ptr, operation, param_ref, out))
        end
    end
    return Expression(out[])
end

function aggregate(f::Binary.File, dimension::String, operation::String, param::Optional{Real} = nothing)
    return aggregate(Expression(f), dimension, operation, param)
end

function aggregate_agents(f::Binary.File, operation::String, param::Optional{Real} = nothing)
    return aggregate_agents(Expression(f), operation, param)
end
