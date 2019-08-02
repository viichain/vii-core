
#ifndef SOCI_TYPE_HOLDER_H_INCLUDED
#define SOCI_TYPE_HOLDER_H_INCLUDED
#include <typeinfo>

namespace soci
{

namespace details
{

template <typename T>
class type_holder;

class holder
{
public:
    holder() {}
    virtual ~holder() {}

    template<typename T>
    T get()
    {
        type_holder<T>* p = dynamic_cast<type_holder<T> *>(this);
        if (p)
        {
            return p->template value<T>();
        }
        else
        {
            throw std::bad_cast();
        }
    }

private:

    template<typename T>
    T value();
};

template <typename T>
class type_holder : public holder
{
public:
    type_holder(T * t) : t_(t) {}
    ~type_holder() { delete t_; }

    template<typename TypeValue>
    TypeValue value() const { return *t_; }

private:
    T * t_;
};

} // namespace details

} // namespace soci

#endif // SOCI_TYPE_HOLDER_H_INCLUDED
