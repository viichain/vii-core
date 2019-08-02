
#ifndef SOCI_TYPE_PTR_H_INCLUDED
#define SOCI_TYPE_PTR_H_INCLUDED

namespace soci { namespace details {

template <typename T>
class type_ptr
{
public:
    type_ptr(T * p) : p_(p) {}
    ~type_ptr() { delete p_; }

    T * get() const { return p_; }
    void release() const { p_ = 0; }

private:
    mutable T * p_;
};

} // namespace details
} // namespace soci

#endif // SOCI_TYPE_PTR_H_INCLUDED
