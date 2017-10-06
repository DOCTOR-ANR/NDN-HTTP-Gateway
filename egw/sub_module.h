#pragma once

#include "module.h"

template <class T> //, class = class std::enable_if<std::is_base_of<Module, T>::value, T>::type>
class SubModule : public Module {
protected:
    T &_parent;

public:
    SubModule(size_t concurrency, T &parent) : Module(concurrency), _parent(parent) {

    }

    virtual ~SubModule() {

    }

    virtual void run() override = 0;
};