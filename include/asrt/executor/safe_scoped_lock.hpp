
namespace asrt{
namespace details{

template <typename Mutex>
class unique_lock_no_throw {

public:
  // Tag type used to distinguish constructors.
  enum adopt_lock_t { adopt_lock };

  // Constructor adopts a lock that is already held.
  unique_lock_no_throw(Mutex& m, adopt_lock_t) noexcept
    : mutex_{m},
      is_locked_{true}
  {
  }

  // Constructor acquires the lock.
  explicit unique_lock_no_throw(Mutex& m) noexcept
    : mutex_{m}
  {
    mutex_.lock();
    is_locked_ = true;
  }

  // Destructor releases the lock.
  ~unique_lock_no_throw() noexcept
  {
    if (is_locked_)
      mutex_.unlock();
  }

  // Explicitly acquire the lock.
  void lock() noexcept
  {
    if (!is_locked_)
    {
      mutex_.lock();
      is_locked_ = true;
    }
  }

  // Explicitly release the lock.
  void unlock() noexcept
  {
    if (is_locked_)
    {
      mutex_.unlock();
      is_locked_ = false;
    }
  }

  // Test whether the lock is held.
  bool is_locked() const
  {
    return is_locked_;
  }

  // Get the underlying mutex.
  Mutex& mutex() noexcept
  {
    return mutex_;
  }

private:
  // The underlying mutex.
  Mutex& mutex_;

  // Whether the mutex is currently locked or unlocked.
  bool is_locked_;


};


}
}
