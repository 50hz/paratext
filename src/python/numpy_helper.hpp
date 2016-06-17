/*
    ParaText: parallel text reading
    Copyright (C) 2016. wise.io, Inc.

   Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.

 */

/*
  Coder: Damian Eads.
 */

#ifndef WISEIO_NUMPY_HELPER_HPP
#define WISEIO_NUMPY_HELPER_HPP

#include <Python.h>

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <memory>
#include <iostream>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <numpy/npy_math.h>


template <class T>
struct numpy_type {};

template <> struct numpy_type<uint8_t>  { static const long id = NPY_UINT8; };
template <> struct numpy_type<int8_t>   { static const long id = NPY_INT8; };
template <> struct numpy_type<uint16_t> { static const long id = NPY_UINT16; };
template <> struct numpy_type<int16_t>  { static const long id = NPY_INT16; };
template <> struct numpy_type<uint32_t> { static const long id = NPY_UINT32; };
template <> struct numpy_type<int32_t>  { static const long id = NPY_INT32; };
template <> struct numpy_type<uint64_t> { static const long id = NPY_UINT64; };
template <> struct numpy_type<int64_t>  { static const long id = NPY_INT64; };
template <> struct numpy_type<float> { static const long id = NPY_FLOAT; };
template <> struct numpy_type<double>  { static const long id = NPY_DOUBLE; };
template <> struct numpy_type<std::string>  { static const long id = NPY_OBJECT; };

#if defined(__APPLE__)
template <> struct numpy_type<unsigned long>  { static const long id = NPY_ULONG; };
#endif

inline PyObject *as_python_string(const std::string &in) {
  return PyUnicode_FromStringAndSize(in.c_str(), in.size());
}

template <class Container, class Enable=void>
struct build_array_impl {};

template <class Container>
struct build_array_impl<Container, typename std::enable_if<std::is_arithmetic<typename Container::value_type>::value>::type> {
  typedef typename Container::value_type value_type;

  static PyObject *build_array(const Container &container) {
    npy_intp fdims[] = {(npy_intp)container.size()};
    PyObject *array = (PyObject*)PyArray_SimpleNew(1, fdims, numpy_type<value_type>::id);
    try {
      value_type *data = (value_type*)PyArray_DATA((PyArrayObject*)array);
      for (size_t i = 0; i < container.size(); i++) {
        data[i] = container[i];
      }
    }
    catch (...) {
      Py_XDECREF(array);
      array = NULL;
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }

};

template <class Container>
struct build_array_impl<Container, typename std::enable_if<std::is_same<typename Container::value_type, std::string>::value>::type> {

  typedef typename Container::value_type value_type;

  static PyObject *build_array(const Container &container) {
    size_t sz = (size_t)container.size();
    npy_intp fdims[] = {(npy_intp)sz};
    PyObject *array = (PyObject*)PyArray_SimpleNew(1, fdims, NPY_OBJECT);
    try {
      for (size_t i = 0; i < container.size(); i++) {
        PyObject **ref = (PyObject **)PyArray_GETPTR1((PyArrayObject*)array, i);
        PyObject *newobj = as_python_string(container[i]);
        Py_XDECREF(*ref);
        *ref = newobj;
      }
    }
    catch (...) {
      for (size_t i = 0; i < sz; i++) {
        PyObject **ref = (PyObject **)PyArray_GETPTR1((PyArrayObject*)array, i);
        Py_XDECREF(*ref);
        *ref = Py_None;
        Py_XINCREF(*ref);
      }
      Py_XDECREF(array);
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }

};


template <class Iterator, class Enable=void>
struct build_array_from_range_impl {};

template <class Iterator>
struct build_array_from_range_impl<Iterator, typename std::enable_if<std::is_arithmetic<typename std::iterator_traits<Iterator>::value_type>::value>::type> {
  typedef typename Iterator::value_type value_type;

  static PyObject *build_array(const std::pair<Iterator, Iterator> &range) {
    npy_intp fdims[] = {(npy_intp)std::distance(range.first, range.second)};
    PyObject *array = NULL;
    try {
      array = (PyObject*)PyArray_SimpleNew(1, fdims, numpy_type<value_type>::id);
      value_type *data = (value_type*)PyArray_DATA((PyArrayObject*)array);
      size_t i = 0;
      for (Iterator it = range.first; it != range.second; it++, i++) {
        data[i] = *it;
      }
    }
    catch (...) {
      Py_XDECREF(array);
      array = NULL;
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }
};

template <class Iterator>
struct build_array_from_range_impl<Iterator, typename std::enable_if<std::is_same<typename std::iterator_traits<Iterator>::value_type, std::string>::value>::type> {

  typedef typename Iterator::value_type value_type;

  static PyObject *build_array(const std::pair<Iterator, Iterator> &range) {
    size_t sz = (npy_intp)std::distance(range.first, range.second);
    npy_intp fdims[] = {(npy_intp)sz};
    PyObject *array = (PyObject*)PyArray_SimpleNew(1, fdims, NPY_OBJECT);
    try {
      size_t i = 0;
      for (Iterator it = range.first; it != range.second; it++, i++) {
        PyObject **ref = (PyObject **)PyArray_GETPTR1((PyArrayObject*)array, i);
        PyObject *newobj = as_python_string(*it);
        Py_XDECREF(*ref);
        *ref = newobj;
      }
    }
    catch (...) {
      size_t i = 0;
      for (i = 0; i < sz; i++) {
        PyObject **ref = (PyObject **)PyArray_GETPTR1((PyArrayObject*)array, i);
        Py_XDECREF(*ref);
        *ref = Py_None;
        Py_XINCREF(*ref);
      }
      Py_XDECREF(array);
      array = NULL;
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }
};

template <class Populator>
struct base_insert_populator_impl {
  base_insert_populator_impl() {}
  virtual ~base_insert_populator_impl() {}

  virtual PyObject *populate(const Populator &populator) = 0;
};

template <class Populator, class T>
struct derived_insert_populator_impl : public base_insert_populator_impl<Populator> {
  typedef T value_type;

  derived_insert_populator_impl() {}
  virtual ~derived_insert_populator_impl() {}

  virtual PyObject *populate(const Populator &populator) {
    npy_intp fdims[] = {(npy_intp)populator.size()};
    PyObject *array = NULL;
    try {
      array = (PyObject*)PyArray_SimpleNew(1, fdims, numpy_type<value_type>::id);
      value_type *data = (value_type*)PyArray_DATA((PyArrayObject*)array);
      populator.insert_into_buffer(data);
    }
    catch (...) {
      Py_XDECREF(array);
      array = NULL;
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }
};


struct string_array_output_iterator  : public std::iterator<std::forward_iterator_tag, std::string> {
  string_array_output_iterator(PyArrayObject *array) : i(0), array(array) {}

  inline string_array_output_iterator &operator++() {
    PyObject *s = as_python_string(output);
    PyObject **ref = (PyObject **)PyArray_GETPTR1((PyArrayObject*)array, i);
    Py_XDECREF(*ref);
    *ref = s;
    i++;
    return *this;
  }

  inline string_array_output_iterator &operator++(int) {
    return operator++();
  }

  inline std::string &operator*() {
    return output;
  }

  long i;
  std::string output;
  PyArrayObject *array;
};

template <class Populator>
struct derived_insert_populator_impl<Populator, std::string> : public base_insert_populator_impl<Populator> {
  typedef std::string value_type;

  derived_insert_populator_impl() {}
  virtual ~derived_insert_populator_impl() {}

  virtual PyObject *populate(const Populator &populator) {
    npy_intp fdims[] = {(npy_intp)populator.size()};
    PyObject *array = NULL;
    array = (PyObject*)PyArray_SimpleNew(1, fdims, numpy_type<value_type>::id);
    try {
      string_array_output_iterator oit((PyArrayObject*)array);
      populator.insert_and_forget(oit);
    }
    catch (...) {
      Py_XDECREF(array);
      array = NULL;
      std::rethrow_exception(std::current_exception());
    }
    return array;
  }
};

template <class Populator>
PyObject *build_populator(const Populator &populator) {
  static std::unordered_map<std::type_index, std::shared_ptr<base_insert_populator_impl<Populator>>>
    populators({std::make_pair(std::type_index(typeid(uint8_t)),
                               std::make_shared<derived_insert_populator_impl<Populator, uint8_t>>()),
          std::make_pair(std::type_index(typeid(int8_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, int8_t>>()),
          std::make_pair(std::type_index(typeid(uint16_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, uint16_t>>()),
          std::make_pair(std::type_index(typeid(int16_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, int16_t>>()),
          std::make_pair(std::type_index(typeid(uint32_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, uint32_t>>()),
          std::make_pair(std::type_index(typeid(int32_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, int32_t>>()),
          std::make_pair(std::type_index(typeid(uint64_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, uint64_t>>()),
          std::make_pair(std::type_index(typeid(int64_t)),
                         std::make_shared<derived_insert_populator_impl<Populator, int64_t>>()),
          std::make_pair(std::type_index(typeid(float)),
                         std::make_shared<derived_insert_populator_impl<Populator, float>>()),
          std::make_pair(std::type_index(typeid(double)),
                         std::make_shared<derived_insert_populator_impl<Populator, double>>()),
          std::make_pair(std::type_index(typeid(std::string)),
                         std::make_shared<derived_insert_populator_impl<Populator, std::string>>())
          });
  auto it = populators.find(populator.get_type_index());
  if (it == populators.end()) {
    throw std::logic_error(std::string("cannot process type"));
  }
  return it->second->populate(populator);
}

template <class Container>
PyObject *build_array(const Container &container) {
  return (PyObject*)build_array_impl<Container>::build_array(container);
}

template <class Iterator>
PyObject *build_array_from_range(const std::pair<Iterator, Iterator> &range) {
  return (PyObject*)build_array_from_range_impl<Iterator>::build_array(range);
}

#endif
