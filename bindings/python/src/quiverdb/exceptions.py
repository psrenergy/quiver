class QuiverError(Exception):
    """Error from the Quiver C API."""

    def __repr__(self) -> str:
        return f"QuiverError({self.args[0]!r})" if self.args else "QuiverError()"
