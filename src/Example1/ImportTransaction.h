#ifndef ImportTransaction_h
#define ImportTransaction_h

enum class ImportTransactionState {
    Idle,
    Preparing,
    Writing,
    Committing,
    Committed,
    RollingBack,
    RolledBack
};

// Small, dependency-free coordinator shared by the SAM builders.  Resource
// cleanup remains in each builder; this class only enforces transitions and
// makes rollback retryable and idempotent.
class ImportTransaction {
public:
    ImportTransactionState state() const { return m_state; }
    bool ownsResource() const { return m_ownsResource; }

    bool begin()
    {
        if (m_state != ImportTransactionState::Idle &&
            m_state != ImportTransactionState::RolledBack)
            return false;
        m_state = ImportTransactionState::Preparing;
        m_ownsResource = false;
        return true;
    }

    bool markResourceOwned()
    {
        if (m_state != ImportTransactionState::Preparing)
            return false;
        m_ownsResource = true;
        return true;
    }

    bool startWriting()
    {
        if (m_state != ImportTransactionState::Preparing || !m_ownsResource)
            return false;
        m_state = ImportTransactionState::Writing;
        return true;
    }

    bool startCommitting()
    {
        if (m_state != ImportTransactionState::Writing)
            return false;
        m_state = ImportTransactionState::Committing;
        return true;
    }

    bool markCommitted()
    {
        if (m_state != ImportTransactionState::Committing)
            return false;
        m_ownsResource = false;
        m_state = ImportTransactionState::Committed;
        return true;
    }

    template <typename Cleanup>
    bool rollback(Cleanup cleanup)
    {
        if (m_state == ImportTransactionState::Idle ||
            m_state == ImportTransactionState::RolledBack)
            return true;
        if (m_state == ImportTransactionState::Committed)
            return false;

        m_state = ImportTransactionState::RollingBack;
        if (m_ownsResource && !cleanup())
            return false;

        m_ownsResource = false;
        m_state = ImportTransactionState::RolledBack;
        return true;
    }

private:
    ImportTransactionState m_state = ImportTransactionState::Idle;
    bool m_ownsResource = false;
};

#endif // ImportTransaction_h
