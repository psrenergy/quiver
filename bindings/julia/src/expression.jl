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

function _binop(op, lhs::Expression, rhs::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply(op, lhs.ptr, rhs.ptr, out))
    return Expression(out[])
end

function _binop(op, lhs::Expression, rhs::Real)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply_scalar_right(op, lhs.ptr, Float64(rhs), out))
    return Expression(out[])
end

function _binop(op, lhs::Real, rhs::Expression)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    check(C.quiver_expression_apply_scalar_left(op, Float64(lhs), rhs.ptr, out))
    return Expression(out[])
end

for (op, c_op) in [(:+, :ADD), (:-, :SUBTRACT), (:*, :MULTIPLY), (:/, :DIVIDE)]
    c_enum = Symbol("QUIVER_EXPRESSION_OP_", c_op)
    @eval begin
        Base.$op(a::Expression, b::Expression) = _binop(C.$c_enum, a, b)
        Base.$op(a::Expression, b::Real) = _binop(C.$c_enum, a, b)
        Base.$op(a::Real, b::Expression) = _binop(C.$c_enum, a, b)
    end
end

function save(e::Expression, path::String)
    check(C.quiver_expression_save(e.ptr, path))
    return nothing
end

function get_metadata(e::Expression)
    out = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    check(C.quiver_expression_get_metadata(e.ptr, out))
    return Binary.Metadata(out[])
end
