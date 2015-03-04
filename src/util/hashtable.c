/*
  Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>

  This file is part of CCC.

  CCC is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  CCC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CCC.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "hashtable.h"

int ht_init(ht_table *ht, ht_params *params) {
}

void ht_destroy(ht_table *ht);
int ht_insert(ht_table *ht, ht_link *elem);
int ht_remove(ht_table *ht, ht_link *elem);

void *ht_lookup(const ht_table, const void *key);
