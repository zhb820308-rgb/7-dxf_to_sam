#ifndef DxfImportError_h
#define DxfImportError_h

// Stable error categories shared by parsing and conversion stages.
enum class DxfImportErrorCode {
    None = 0,
    InvalidArgument,
    ReadFailed,
    ExpansionLimit,
    ConversionLimit,
    NoSupportedEntities,
    ConversionFailed,
    Canceled,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

#endif
