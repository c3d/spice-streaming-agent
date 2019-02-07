/*
 * Include catch/catch.hpp or catch2/catch.hpp
 * according to what configure found
 *
 * \copyright
 * Copyright 2019 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_CATCH_HPP
#include <config.h>

#if   defined(HAVE_CATCH2_CATCH_HPP)
#include <catch2/catch.hpp>
#elif defined(HAVE_CATCH_CATCH_HPP)
#include <catch/catch.hpp>
#endif

#endif // SPICE_CATCH_HPP
