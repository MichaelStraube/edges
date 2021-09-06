/*
 *  util.h
 *
 *  Copyright (C) 2020 Michael Straube <michael.straubej@gmail.com>
 *  Copyright (C) 2006-2020 Pacman Development Team <pacman-dev@archlinux.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ED_UTIL_H
#define ED_UTIL_H

#include <stdio.h>

char *safe_fgets(char *s, int size, FILE *stream);
size_t strtrim(char *str);
void wordsplit_free(char **ws);
char **wordsplit(const char *str);

#endif /* ED_UTIL_H */
