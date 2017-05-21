///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "counted.hpp"

int counted::default_construction_count;
int counted::copy_construction_count;
int counted::move_construction_count;
int counted::destruction_count;
