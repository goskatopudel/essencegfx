#pragma once

#include <EASTL\shared_ptr.h>

template<typename T> 
struct TInternalRef : public eastl::shared_ptr<T> {
		using shared_ptr<T>::shared_ptr;
		using shared_ptr<T>::operator=;
		operator T *() const { return get(); };
};

#define DECORATE_CLASS_REF(T) \
typedef TInternalRef<T> T ## Ref; \
typedef eastl::weak_ptr<T> T ## WeakRef; \
typedef T ## Ref & T ## RefParam;