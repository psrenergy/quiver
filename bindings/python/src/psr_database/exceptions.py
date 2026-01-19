"""Exception classes for PSR Database."""


class DatabaseError(Exception):
    """Base exception for PSR Database errors."""

    def __init__(self, message: str, error_code: int | None = None):
        super().__init__(message)
        self.error_code = error_code
